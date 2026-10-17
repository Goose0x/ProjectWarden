#include "Slice0/KodSlice0GameMode.h"
#include "Game/KodBuildingYaw.h"
#include "Game/KodCollision.h"
#include "Game/KodMeshSockets.h"
#include "Slice0/KodFaultline.h"
#include "Slice0/KodMapLayout.h"
#include "Slice0/KodMapLife.h"
#include "Slice0/KodSlice0PlayerController.h"
#include "Game/KodHUD.h"
#include "Game/KodPlayerController.h"
#include "Game/KodSkirmishSetup.h"
#include "Engine/Light.h"
#include "Game/KodUiArt.h"
#include "Game/KodRTSCameraPawn.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "Actors/KodUnit.h"
#include "Actors/KodBuilding.h"
#include "Actors/KodMeshAttach.h"
#include "Actors/KodResourceNode.h"
#include "Data/KodResourceNodeDefinition.h"
#include "Data/KodAbilityDefinition.h"
#include "Data/KodUnitDefinition.h"
#include "Data/KodBuildingDefinition.h"
#include "Data/KodMeshCatalog.h"
#include "Warden/KodWardenPaths.h"
#include "Sim/KodSimSubsystem.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Game/KodSpectatorPawn.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/Texture.h"
#include "Camera/CameraComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/Parse.h"

namespace
{
	bool IsSlice0GreyboxProxyName(const FString& Name)
	{
		// Prefix and infix. PIE renames objects to UEDPIE_0_<name>, so Contains covers both.
		return Name.Contains(TEXT("PROXY_"));
	}

	bool IsSlice0GroundFloorName(const FString& Name)
	{
		// Outliner label is exactly PROXY_GROUND. PIE prefixes the object name
		// (UEDPIE_0_PROXY_GROUND) when the asset itself uses that name.
		// PROXY_LBL_GROUND is a text label and must stay in the mute set.
		return Name.Equals(TEXT("PROXY_GROUND"), ESearchCase::IgnoreCase)
			|| Name.EndsWith(TEXT("_PROXY_GROUND"), ESearchCase::IgnoreCase);
	}

	bool IsSlice0HostileAnchorName(const FString& Name)
	{
		// Outliner label is exactly PROXY_HOSTILE. PIE may prefix the object name.
		return Name.Equals(TEXT("PROXY_HOSTILE"), ESearchCase::IgnoreCase)
			|| Name.EndsWith(TEXT("_PROXY_HOSTILE"), ESearchCase::IgnoreCase);
	}

	bool ActorNameOrLabelMatches(const AActor* Actor, bool (*Predicate)(const FString&))
	{
		if (!Actor || !Predicate)
		{
			return false;
		}
		if (Predicate(Actor->GetName()))
		{
			return true;
		}
		// Outliner label when it differs from the object name (StaticMeshActor_* labeled PROXY_*).
		// GetActorNameOrLabel returns that label only in editor builds (WITH_EDITORONLY_DATA).
		// Packaged builds fall back to GetName(), which is StaticMeshActor_* and will not match.
		return Predicate(Actor->GetActorNameOrLabel());
	}

	bool IsSlice0GreyboxProxy(const AActor* Actor)
	{
		return ActorNameOrLabelMatches(Actor, &IsSlice0GreyboxProxyName);
	}

	bool IsSlice0GroundFloor(const AActor* Actor)
	{
		return ActorNameOrLabelMatches(Actor, &IsSlice0GroundFloorName);
	}

	void ApplyGroundChannels(AActor* Actor)
	{
		if (!Actor)
		{
			return;
		}
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		Actor->GetComponents(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive)
			{
				continue;
			}
			Primitive->SetCollisionResponseToChannel(KodCollision::Ground, ECR_Block);
			Primitive->SetCollisionResponseToChannel(KodCollision::Select, ECR_Ignore);
		}
	}

	void ApplyWorldGroundChannels(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor)
			{
				continue;
			}
			const bool bGround = Actor->ActorHasTag(FName(TEXT("KodGround")))
				|| Actor->ActorHasTag(FName(TEXT("KodGeneratedGround")))
				|| IsSlice0GroundFloor(Actor)
				|| Actor->GetClass()->GetName().Contains(TEXT("Landscape"));
			if (bGround)
			{
				ApplyGroundChannels(Actor);
			}
		}
	}

	bool IsSlice0HostileAnchorLabel(const AActor* Actor)
	{
		return ActorNameOrLabelMatches(Actor, &IsSlice0HostileAnchorName);
	}

	bool IsSlice0CommandCenterName(const FString& Name)
	{
		return Name.Contains(TEXT("PROXY_CC"), ESearchCase::IgnoreCase);
	}

	bool IsSlice0CommandCenter(const AActor* Actor)
	{
		if (Actor && Actor->ActorHasTag(FName(TEXT("KodBuildingCommandCenter"))))
		{
			return true;
		}
		return ActorNameOrLabelMatches(Actor, &IsSlice0CommandCenterName);
	}

	void EnableResourceSelectCollision(AActor* Actor)
	{
		if (!Actor)
		{
			return;
		}
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		Actor->GetComponents(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive || Primitive->ComponentHasTag(FName(TEXT("KodFx"))))
			{
				continue;
			}
			Primitive->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
			Primitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Primitive->SetCollisionResponseToChannel(KodCollision::Select, ECR_Block);
			Primitive->SetCollisionResponseToChannel(KodCollision::Ground, ECR_Ignore);
		}
	}

	float ResolveResourceFloorZ(
		UWorld* World,
		const UKodSimSubsystem* Sim,
		float X,
		float Y,
		float HintZ,
		const TArray<AActor*>& ExtraIgnored)
	{
		if (!World)
		{
			return HintZ;
		}
		FCollisionQueryParams Params(TEXT("KodResourceFloor"), /*bTraceComplex*/ false);
		if (Sim)
		{
			TArray<FKodSimEntityState> States;
			TArray<AActor*> Actors;
			Sim->CopyEntitySnapshot(States, Actors);
			for (AActor* Actor : Actors)
			{
				Params.AddIgnoredActor(Actor);
			}
		}
		for (AActor* Actor : ExtraIgnored)
		{
			Params.AddIgnoredActor(Actor);
		}
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			Params.AddIgnoredActor(PC->GetPawn());
		}

		const FVector Start(X, Y, HintZ + 8000.f);
		const FVector End(X, Y, HintZ - 8000.f);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Start, End, KodCollision::Ground, Params) && Hit.bBlockingHit)
		{
			return Hit.ImpactPoint.Z;
		}
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params) && Hit.bBlockingHit)
		{
			return Hit.ImpactPoint.Z;
		}
		return HintZ;
	}

	bool MuteActorPrimitives(AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		Actor->GetComponents(Primitives);
		bool bMutedActor = false;
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive)
			{
				continue;
			}
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			bMutedActor = true;
		}
		return bMutedActor;
	}

	bool CellHoldsResourceNode(const UKodSimSubsystem* Sim, int32 CellX, int32 CellY)
	{
		if (!Sim)
		{
			return false;
		}
		TArray<FKodResourceNodeState> Nodes;
		Sim->CopyResourceNodes(Nodes);
		for (const FKodResourceNodeState& Node : Nodes)
		{
			const int32 NodeX = KodBuildGrid::CellOf(Node.Position.X);
			const int32 NodeY = KodBuildGrid::CellOf(Node.Position.Y);
			if (FMath::Abs(NodeX - CellX) <= 1 && FMath::Abs(NodeY - CellY) <= 1)
			{
				return true;
			}
		}
		return false;
	}
}

namespace
{
	bool IsPlayerBaseCandidate(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}
		const FString Label = Actor->GetActorNameOrLabel();
		return Label.Contains(TEXT("PROXY_CC"))
			|| Actor->ActorHasTag(FName(TEXT("KodBuildingCommandCenter")))
			|| Actor->ActorHasTag(FName(TEXT("KodStartSpot_0")))
			|| Actor->ActorHasTag(FName(TEXT("P1_SPAWN")));
	}

	int32 PlayerBaseScore(const AActor* Actor)
	{
		const FVector At = Actor->GetActorLocation();
		const bool bOrigin = FMath::Abs(At.X) < 50.f && FMath::Abs(At.Y) < 50.f;
		int32 Score = bOrigin ? 0 : 500;
		if (Actor->ActorHasTag(FName(TEXT("KodStartSpot_0"))))
		{
			Score += 1000;
		}
		if (Actor->GetActorNameOrLabel().Contains(TEXT("PROXY_CC")))
		{
			Score += 200;
		}
		if (Actor->ActorHasTag(FName(TEXT("KodBuildingCommandCenter")))
			|| Actor->ActorHasTag(FName(TEXT("P1_SPAWN"))))
		{
			Score += 100;
		}
		return Score;
	}

	AActor* PickBaseToSeat(UWorld* World)
	{
		AActor* Proxy = nullptr;
		AActor* Tagged = nullptr;
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsPlayerBaseCandidate(Actor))
			{
				continue;
			}
			const bool bProxy = Actor->GetActorNameOrLabel().Contains(TEXT("PROXY_CC"));
			AActor*& Slot = bProxy ? Proxy : Tagged;
			if (!Slot || Actor->GetName() < Slot->GetName())
			{
				Slot = Actor;
			}
		}
		return Proxy ? Proxy : Tagged;
	}
}

AActor* AKodSlice0GameMode::FindBaseToSeat(UWorld* World)
{
	return PickBaseToSeat(World);
}

AActor* AKodSlice0GameMode::FindPlayerBaseActor(UWorld* World)
{
	AActor* Best = nullptr;
	int32 BestScore = MIN_int32;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsPlayerBaseCandidate(Actor))
		{
			continue;
		}
		const int32 Score = PlayerBaseScore(Actor);
		if (!Best || Score > BestScore || (Score == BestScore && Actor->GetName() < Best->GetName()))
		{
			Best = Actor;
			BestScore = Score;
		}
	}
	return Best;
}

AKodSlice0GameMode::AKodSlice0GameMode()
{
	PlayerControllerClass = AKodSlice0PlayerController::StaticClass();
	DefaultPawnClass = AKodRTSCameraPawn::StaticClass();
	// Parent sets this too. Repeat it here so a map or config cannot leave Slice 0 without AKodHUD.
	HUDClass = AKodHUD::StaticClass();
	WorkerSpawnCount = KodEconomyDefaults::WorkerSpawnCount;
	PlayableCells = KodEconomyDefaults::PlayableCells;
	PrimaryActorTick.bCanEverTick = false;
	FaultlineMaps.Add(TEXT("L_FaultlineTraining"));
}

void AKodSlice0GameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// Ensure bootstrap catalog exists before PIE actors resolve soft paths.
	UKodSlice0Bootstrap::EnsureCatalog(GetTransientPackage());
}

void AKodSlice0GameMode::MuteGreyboxProxyCollision()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Log, TEXT("Slice0 PROXY collision muted Count=0 KeptGround=0 ByTag=0 ByLabel=0"));
		return;
	}

	int32 Muted = 0;
	int32 KeptGround = 0;
	int32 ByTag = 0;
	int32 ByLabel = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Tags first. Packaged builds do not have outliner labels, so PROXY_ names miss.
		// KodGround wins over KodGreybox when both are set, so a mis-tagged floor stays.
		if (Actor->ActorHasTag(FName(TEXT("KodGround"))))
		{
			++KeptGround;
			++ByTag;
			continue;
		}
		if (Actor->ActorHasTag(FName(TEXT("KodGreybox"))))
		{
			++ByTag;
			if (MuteActorPrimitives(Actor))
			{
				++Muted;
			}
			continue;
		}

		if (!IsSlice0GreyboxProxy(Actor))
		{
			continue;
		}

		// Floor stays BlockAll: ECC_Pawn / ECC_Visibility move traces, and the pawn capsule.
		// Props (CC, Dozer, crate, dock, pads, bounds, dirt, labels) still go NoCollision.
		++ByLabel;
		if (IsSlice0GroundFloor(Actor))
		{
			++KeptGround;
			continue;
		}
		if (MuteActorPrimitives(Actor))
		{
			++Muted;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Slice0 PROXY collision muted Count=%d KeptGround=%d ByTag=%d ByLabel=%d"),
		Muted,
		KeptGround,
		ByTag,
		ByLabel);
	if (Muted > 0 && KeptGround == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Slice0 PROXY ground floor not kept (tag KodGround or label PROXY_GROUND). Floor traces will miss."));
	}
}

struct FKodSunIni
{
	bool bOverride = false;
	bool bPreferTagged = true;
	float Pitch = -37.f;
	float Yaw = 135.f;
	float Roll = 0.f;
	bool bHaveRotation = false;
	float IntensityLux = 7.8f;
	bool bHaveIntensity = false;
	float TemperatureK = 4300.f;
	bool bHaveTemperature = false;
	float SourceAngle = 2.5f;
	bool bHaveSourceAngle = false;
	float SourceSoftAngle = 1.f;
	bool bHaveSourceSoft = false;
	float ShadowAmount = 0.85f;
	bool bHaveShadowAmount = false;
	float SkyIntensity = 1.45f;
	bool bHaveSkyIntensity = false;
	FLinearColor SkyTint = FLinearColor::FromSRGBColor(FColor(0xFA, 0xBC, 0xA9));
	bool bHaveSkyTint = false;
	FLinearColor LowerHemisphere = FLinearColor::FromSRGBColor(FColor(0x5E, 0x44, 0x38));
	bool bHaveLower = false;
	float MinOcclusion = 0.25f;
	bool bHaveMinOcclusion = false;
	FLinearColor OcclusionTint = FLinearColor::FromSRGBColor(FColor(0x3A, 0x28, 0x1C));
	bool bHaveOcclusionTint = false;
	bool bRealTimeCapture = true;
	float ExposureBiasEV = 0.5f;
	bool bHaveExposure = false;
	FString LutPath;
	float LutIntensity = 0.4f;
	bool bHaveLutIntensity = false;
	float WhiteBalanceK = 6000.f;
	bool bHaveWhiteBalance = false;
	FLinearColor ShadowGrade = FLinearColor(0.90f, 0.86f, 0.98f);
	bool bHaveShadowGrade = false;
};

static bool ParseKodColor(const FString& Text, FLinearColor& Out)
{
	FString Trim = Text.TrimStartAndEnd();
	if (Trim.IsEmpty())
	{
		return false;
	}
	if (Trim.StartsWith(TEXT("#")) || (Trim.Len() >= 6 && Trim.Len() <= 8 && !Trim.Contains(TEXT("(")) && !Trim.Contains(TEXT(","))))
	{
		FString Hex = Trim;
		if (Hex.StartsWith(TEXT("#")))
		{
			Hex.RightChopInline(1);
		}
		Out = FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
		return true;
	}
	float R = 0.f;
	float G = 0.f;
	float B = 0.f;
	bool bAny = false;
	auto Grab = [&Trim, &bAny](const TCHAR* Key, float& Dest)
	{
		const FString Pattern = FString::Printf(TEXT("%s="), Key);
		const int32 At = Trim.Find(Pattern, ESearchCase::IgnoreCase);
		if (At == INDEX_NONE)
		{
			return;
		}
		bAny = true;
		Dest = FCString::Atof(*Trim.Mid(At + Pattern.Len()));
	};
	Grab(TEXT("R"), R);
	Grab(TEXT("G"), G);
	Grab(TEXT("B"), B);
	if (!bAny)
	{
		FString Inner = Trim;
		Inner.RemoveFromStart(TEXT("("));
		Inner.RemoveFromEnd(TEXT(")"));
		TArray<FString> Parts;
		Inner.ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.Num() < 3)
		{
			return false;
		}
		R = FCString::Atof(*Parts[0]);
		G = FCString::Atof(*Parts[1]);
		B = FCString::Atof(*Parts[2]);
	}
	if (R > 1.f || G > 1.f || B > 1.f)
	{
		Out = FLinearColor::FromSRGBColor(FColor(
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(R), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(G), 0, 255)),
			static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(B), 0, 255))));
		return true;
	}
	Out = FLinearColor(R, G, B);
	return true;
}

static bool ReadKodColor(const TCHAR* Key, FLinearColor& Out)
{
	FString Text;
	if (!GConfig || !GConfig->GetString(TEXT("KodLighting"), Key, Text, GGameIni) || Text.IsEmpty())
	{
		return false;
	}
	return ParseKodColor(Text, Out);
}

static FKodSunIni ReadKodLighting()
{
	FKodSunIni Sun;
	if (!GConfig)
	{
		return Sun;
	}
	bool bOverride = false;
	if (GConfig->GetBool(TEXT("KodLighting"), TEXT("bOverrideLevelLighting"), bOverride, GGameIni))
	{
		Sun.bOverride = bOverride;
	}
	bool bPrefer = true;
	if (GConfig->GetBool(TEXT("KodLighting"), TEXT("bPreferTaggedLevelLights"), bPrefer, GGameIni))
	{
		Sun.bPreferTagged = bPrefer;
	}
	FString Rotation;
	if (GConfig->GetString(TEXT("KodLighting"), TEXT("SunRotation"), Rotation, GGameIni) && !Rotation.IsEmpty())
	{
		auto Grab = [&Rotation](const TCHAR* Key, float& Dest)
		{
			const FString Pattern = FString::Printf(TEXT("%s="), Key);
			const int32 At = Rotation.Find(Pattern, ESearchCase::IgnoreCase);
			if (At != INDEX_NONE)
			{
				Dest = FCString::Atof(*Rotation.Mid(At + Pattern.Len()));
			}
		};
		Grab(TEXT("Pitch"), Sun.Pitch);
		Grab(TEXT("Yaw"), Sun.Yaw);
		Grab(TEXT("Roll"), Sun.Roll);
		Sun.bHaveRotation = true;
	}
	float LegacyPitch = Sun.Pitch;
	float LegacyYaw = Sun.Yaw;
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("Pitch"), LegacyPitch, GGameIni))
	{
		Sun.Pitch = LegacyPitch;
		Sun.bHaveRotation = true;
	}
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("Yaw"), LegacyYaw, GGameIni))
	{
		Sun.Yaw = LegacyYaw;
		Sun.bHaveRotation = true;
	}
	float Intensity = Sun.IntensityLux;
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("SunIntensityLux"), Intensity, GGameIni)
		|| GConfig->GetFloat(TEXT("KodLighting"), TEXT("Intensity"), Intensity, GGameIni))
	{
		Sun.IntensityLux = Intensity;
		Sun.bHaveIntensity = true;
	}
	float Temperature = Sun.TemperatureK;
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("SunTemperatureK"), Temperature, GGameIni))
	{
		Sun.TemperatureK = Temperature;
		Sun.bHaveTemperature = true;
	}
	float Source = Sun.SourceAngle;
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("SunSourceAngle"), Source, GGameIni))
	{
		Sun.SourceAngle = Source;
		Sun.bHaveSourceAngle = true;
	}
	float Soft = Sun.SourceSoftAngle;
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("SunSourceSoftAngle"), Soft, GGameIni))
	{
		Sun.SourceSoftAngle = Soft;
		Sun.bHaveSourceSoft = true;
	}
	float Shadow = Sun.ShadowAmount;
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("SunShadowAmount"), Shadow, GGameIni))
	{
		Sun.ShadowAmount = Shadow;
		Sun.bHaveShadowAmount = true;
	}
	float Sky = Sun.SkyIntensity;
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("SkyLightIntensity"), Sky, GGameIni))
	{
		Sun.SkyIntensity = Sky;
		Sun.bHaveSkyIntensity = true;
	}
	if (ReadKodColor(TEXT("SkyLightTint"), Sun.SkyTint) || ReadKodColor(TEXT("Color"), Sun.SkyTint))
	{
		Sun.bHaveSkyTint = true;
	}
	if (ReadKodColor(TEXT("SkyLightLowerHemisphereColor"), Sun.LowerHemisphere))
	{
		Sun.bHaveLower = true;
	}
	float Occlusion = Sun.MinOcclusion;
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("SkyLightMinOcclusion"), Occlusion, GGameIni))
	{
		Sun.MinOcclusion = Occlusion;
		Sun.bHaveMinOcclusion = true;
	}
	if (ReadKodColor(TEXT("SkyLightOcclusionTint"), Sun.OcclusionTint))
	{
		Sun.bHaveOcclusionTint = true;
	}
	bool bCapture = Sun.bRealTimeCapture;
	if (GConfig->GetBool(TEXT("KodLighting"), TEXT("SkyLightRealTimeCapture"), bCapture, GGameIni))
	{
		Sun.bRealTimeCapture = bCapture;
	}
	float Exposure = Sun.ExposureBiasEV;
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("ExposureBiasEV"), Exposure, GGameIni))
	{
		Sun.ExposureBiasEV = Exposure;
		Sun.bHaveExposure = true;
	}
	GConfig->GetString(TEXT("KodLighting"), TEXT("ColorGradingLUT"), Sun.LutPath, GGameIni);
	float Lut = Sun.LutIntensity;
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("ColorGradingIntensity"), Lut, GGameIni))
	{
		Sun.LutIntensity = Lut;
		Sun.bHaveLutIntensity = true;
	}
	float White = Sun.WhiteBalanceK;
	if (GConfig->GetFloat(TEXT("KodLighting"), TEXT("WhiteBalanceTemp"), White, GGameIni))
	{
		Sun.WhiteBalanceK = White;
		Sun.bHaveWhiteBalance = true;
	}
	if (ReadKodColor(TEXT("ShadowGrade"), Sun.ShadowGrade))
	{
		Sun.bHaveShadowGrade = true;
	}
	return Sun;
}

static void ApplyKodShadowAmount(UDirectionalLightComponent* Light, float Amount)
{
	if (!Light)
	{
		return;
	}
	if (FFloatProperty* Prop = FindFProperty<FFloatProperty>(Light->GetClass(), TEXT("ShadowAmount")))
	{
		Prop->SetPropertyValue_InContainer(Light, Amount);
	}
}

int32 AKodSlice0GameMode::KeepSingleDirectionalSun(UWorld* World, bool bTutorialLook)
{
	if (!World)
	{
		return 0;
	}
	TArray<ADirectionalLight*> Suns;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (*It)
		{
			Suns.Add(*It);
		}
	}
	const FKodSunIni Lighting = ReadKodLighting();
	ADirectionalLight* KeptSun = nullptr;
	if (Lighting.bPreferTagged)
	{
		for (ADirectionalLight* Sun : Suns)
		{
			if (Sun && Sun->ActorHasTag(FName(TEXT("KodDesertLight_v1"))))
			{
				KeptSun = Sun;
				break;
			}
		}
	}
	if (!KeptSun && Suns.Num() > 0)
	{
		KeptSun = Suns[0];
	}
	bool bSpawned = false;
	const bool bTagged = KeptSun && KeptSun->ActorHasTag(FName(TEXT("KodDesertLight_v1")));
	if (!KeptSun)
	{
		const FRotator SpawnRotation(Lighting.Pitch, Lighting.Yaw, Lighting.Roll);
		(void)bTutorialLook;
		KeptSun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(SpawnRotation));
		bSpawned = KeptSun != nullptr;
	}
	int32 Enabled = 0;
	for (ADirectionalLight* Sun : Suns)
	{
		UDirectionalLightComponent* SunComponent = Sun ? Cast<UDirectionalLightComponent>(Sun->GetLightComponent()) : nullptr;
		if (!Sun || !SunComponent || Sun == KeptSun)
		{
			continue;
		}
		SunComponent->SetAtmosphereSunLight(false);
		SunComponent->ForwardShadingPriority = 0;
		SunComponent->SetCastShadows(false);
		SunComponent->SetVisibility(false);
		Sun->SetActorHiddenInGame(true);
		UE_LOG(LogTemp, Log, TEXT("KodSun ExtraDisabled Name=%s"), *Sun->GetName());
	}
	if (Suns.Num() == 0 && KeptSun)
	{
		Suns.Add(KeptSun);
	}
	if (KeptSun)
	{
		UDirectionalLightComponent* KeptComponent = Cast<UDirectionalLightComponent>(KeptSun->GetLightComponent());
		const bool bPlacedStatic = !bSpawned && KeptComponent && KeptComponent->Mobility == EComponentMobility::Static;
		if (KeptComponent && KeptComponent->Mobility != EComponentMobility::Movable)
		{
			// PIE previews a static sun. Standalone -game does not, and an unbuilt
			// lightmap leaves the world black. Promote the one kept sun.
			const float AuthoredIntensity = KeptComponent->Intensity;
			const float IntensityFloor = bTutorialLook ? 6.5f : 4.f;
			if (bPlacedStatic)
			{
				UE_LOG(LogTemp, Log, TEXT("KodSun PlacedStatic Name=%s"), *KeptSun->GetName());
			}
			UE_LOG(LogTemp, Log, TEXT("KodSun PromoteMovable Name=%s Intensity=%.2f Floor=%.2f"),
				*KeptSun->GetName(),
				AuthoredIntensity,
				IntensityFloor);
			KeptComponent->SetMobility(EComponentMobility::Movable);
			KeptComponent->SetAtmosphereSunLight(true);
			KeptComponent->SetVisibility(true);
			KeptComponent->SetCastShadows(true);
			KeptComponent->bAffectsWorld = true;
			KeptComponent->SetIntensity(FMath::Max(AuthoredIntensity, IntensityFloor));
			KeptComponent->SetDynamicShadowDistanceMovableLight(30000.f);
		}
		if (KeptComponent)
		{
			KeptComponent->SetMobility(EComponentMobility::Movable);
			KeptComponent->SetAtmosphereSunLight(true);
			KeptComponent->SetVisibility(true);
			KeptComponent->SetCastShadows(true);
			KeptComponent->SetDynamicShadowDistanceMovableLight(30000.f);
			const float Authored = KeptComponent->Intensity;
			const FRotator AuthoredRot = KeptSun->GetActorRotation();
			const bool bFaultline = World->GetMapName().Contains(TEXT("Faultline"));
			const bool bApplyIni = !bTagged && Lighting.bOverride && (bSpawned || bFaultline);
			if (bApplyIni && Lighting.bHaveRotation)
			{
				KeptSun->SetActorRotation(FRotator(Lighting.Pitch, Lighting.Yaw, Lighting.Roll));
			}
			else if (bSpawned)
			{
				KeptSun->SetActorRotation(FRotator(Lighting.Pitch, Lighting.Yaw, Lighting.Roll));
			}
			if ((bSpawned || (bApplyIni && Lighting.bHaveIntensity) || Authored < 0.05f) && !bTagged)
			{
				const float Wanted = Lighting.bHaveIntensity ? Lighting.IntensityLux : Authored;
				KeptComponent->SetIntensity(bFaultline || bSpawned ? Wanted : FMath::Max(Authored, Wanted));
			}
			if (bApplyIni && Lighting.bHaveTemperature)
			{
				KeptComponent->SetTemperature(Lighting.TemperatureK);
				KeptComponent->SetUseTemperature(true);
			}
			else if (bSpawned)
			{
				KeptComponent->SetTemperature(5200.f);
				KeptComponent->SetUseTemperature(true);
			}
			if (bApplyIni && Lighting.bHaveSourceAngle)
			{
				KeptComponent->SetLightSourceAngle(Lighting.SourceAngle);
			}
			else if (bSpawned)
			{
				KeptComponent->SetLightSourceAngle(0.35f);
			}
			if (bApplyIni && Lighting.bHaveSourceSoft)
			{
				KeptComponent->SetLightSourceSoftAngle(Lighting.SourceSoftAngle);
			}
			if (bApplyIni && Lighting.bHaveShadowAmount)
			{
				ApplyKodShadowAmount(KeptComponent, Lighting.ShadowAmount);
			}
			const FRotator FinalRot = KeptSun->GetActorRotation();
			UE_LOG(LogTemp, Log, TEXT("KodSun Intensity=%.2f Authored=%.2f Rot=P%.0f Y%.0f AuthoredRot=P%.0f Y%.0f Placed=%d"),
				KeptComponent->Intensity,
				Authored,
				FinalRot.Pitch,
				FinalRot.Yaw,
				AuthoredRot.Pitch,
				AuthoredRot.Yaw,
				bSpawned ? 0 : 1);
		}
		if (KeptComponent && KeptComponent->IsVisible())
		{
			++Enabled;
		}
		else if (bPlacedStatic)
		{
			++Enabled;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("KodSun Directional=%d Enabled=%d"), Suns.Num(), Enabled);
	return Enabled;
}

namespace
{
	TAutoConsoleVariable<float> CVarKodExposure(
		TEXT("kod.Exposure"),
		1.f,
		TEXT("EV100 bias on the match post-process volume. Manual exposure, no pre-exposure override. Tuned for a sun of 6.5 to 10."),
		ECVF_Default);
}

void AKodSlice0GameMode::ApplyMatchExposure(UWorld* World)
{
	if (IConsoleVariable* AutoExposure = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AutoExposure")))
	{
		AutoExposure->Set(0, ECVF_SetByCode);
	}
	if (IConsoleVariable* PreExposure = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride")))
	{
		PreExposure->Set(0.f, ECVF_SetByCode);
	}
	if (!World)
	{
		return;
	}
	const float Bias = CVarKodExposure.GetValueOnGameThread();
	APostProcessVolume* UnboundPost = nullptr;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (It->bUnbound)
		{
			UnboundPost = *It;
			break;
		}
	}
	if (!UnboundPost)
	{
		UnboundPost = World->SpawnActor<APostProcessVolume>();
	}
	if (!UnboundPost)
	{
		return;
	}
	UnboundPost->bUnbound = true;
	UnboundPost->bEnabled = true;
	UnboundPost->Priority = 100.f;
	UnboundPost->BlendWeight = 1.f;
	FPostProcessSettings& Settings = UnboundPost->Settings;
	Settings.bOverride_AutoExposureMethod = true;
	Settings.AutoExposureMethod = AEM_Manual;
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = Bias;
	Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Settings.AutoExposureApplyPhysicalCameraExposure = false;
	Settings.bOverride_AutoExposureMinBrightness = false;
	Settings.bOverride_AutoExposureMaxBrightness = false;
	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = 0.4f;
	Settings.bOverride_MotionBlurAmount = true;
	Settings.MotionBlurAmount = 0.f;
	UE_LOG(LogTemp, Log, TEXT("KodLook Exposure Manual Bias=%.2f PreExposureOverride=0"), Bias);
}

static void WriteKodGrade(FPostProcessSettings& Settings, const FKodSunIni& Lighting)
{
	if (Lighting.bHaveExposure)
	{
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = Lighting.ExposureBiasEV;
	}
	if (Lighting.bHaveWhiteBalance)
	{
		Settings.bOverride_WhiteTemp = true;
		Settings.WhiteTemp = Lighting.WhiteBalanceK;
	}
	if (Lighting.bHaveShadowGrade)
	{
		Settings.bOverride_ColorGainShadows = true;
		Settings.ColorGainShadows = FVector4(Lighting.ShadowGrade.R, Lighting.ShadowGrade.G, Lighting.ShadowGrade.B, 1.f);
	}
	if (Lighting.bHaveLutIntensity)
	{
		Settings.bOverride_ColorGradingIntensity = true;
		Settings.ColorGradingIntensity = Lighting.LutIntensity;
	}
	if (!Lighting.LutPath.IsEmpty())
	{
		UTexture* Lut = LoadObject<UTexture>(nullptr, *Lighting.LutPath);
		if (!Lut && !Lighting.LutPath.Contains(TEXT(".")))
		{
			const FString Leaf = FPaths::GetBaseFilename(Lighting.LutPath);
			Lut = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("%s.%s"), *Lighting.LutPath, *Leaf));
		}
		if (Lut)
		{
			Settings.bOverride_ColorGradingLUT = true;
			Settings.ColorGradingLUT = Lut;
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("KodLighting LUT Miss %s"), *Lighting.LutPath);
		}
	}
}

static void ApplyKodFillLights(UWorld* World, const FKodSunIni& Lighting)
{
	if (!World || !Lighting.bOverride)
	{
		return;
	}
	const FLinearColor SkyColor = Lighting.bHaveSkyTint
		? Lighting.SkyTint
		: FLinearColor::FromSRGBColor(FColor(0xFA, 0xBC, 0xA9));
	const FLinearColor Lower = Lighting.bHaveLower
		? Lighting.LowerHemisphere
		: FLinearColor::FromSRGBColor(FColor(0x5E, 0x44, 0x38));
	const float SkyIntensity = Lighting.bHaveSkyIntensity ? Lighting.SkyIntensity : 1.45f;
	int32 SkyCount = 0;
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		ASkyLight* SkyActor = *It;
		USkyLightComponent* Sky = SkyActor ? SkyActor->GetLightComponent() : nullptr;
		if (!Sky)
		{
			continue;
		}
		++SkyCount;
		if (Sky->Mobility == EComponentMobility::Static)
		{
			Sky->SetMobility(EComponentMobility::Movable);
		}
		Sky->SetIntensity(SkyIntensity);
		Sky->SetLightColor(SkyColor);
		Sky->bLowerHemisphereIsBlack = false;
		Sky->SetLowerHemisphereColor(Lower);
		Sky->SetIndirectLightingIntensity(1.f);
		if (Lighting.bHaveMinOcclusion)
		{
			Sky->MinOcclusion = Lighting.MinOcclusion;
		}
		if (Lighting.bHaveOcclusionTint)
		{
			Sky->OcclusionTint = Lighting.OcclusionTint.ToFColor(true);
		}
		Sky->SetRealTimeCapture(true);
		Sky->RecaptureSky();
		UE_LOG(LogTemp, Log, TEXT("KodLighting Sky Name=%s Intensity=%.2f RealTime=%d Extra=%d Tint=%s Lower=%s"),
			*SkyActor->GetName(),
			Sky->Intensity,
			Sky->bRealTimeCapture ? 1 : 0,
			SkyCount > 1 ? 1 : 0,
			*SkyColor.ToString(),
			*Lower.ToString());
	}
	if (SkyCount == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("KodLighting Sky Name=None"));
	}
	const FLinearColor FogTint = Lower;
	int32 FogCount = 0;
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		++FogCount;
		if (UExponentialHeightFogComponent* Fog = It->GetComponent())
		{
			Fog->SetFogInscatteringColor(FogTint);
			Fog->SetDirectionalInscatteringColor(SkyColor);
			Fog->SetVolumetricFog(false);
			UE_LOG(LogTemp, Log, TEXT("KodLighting Fog Name=%s Extra=%d Tint=%s"),
				*It->GetName(),
				FogCount > 1 ? 1 : 0,
				*FogTint.ToString());
		}
	}
	APostProcessVolume* Owned = nullptr;
	APostProcessVolume* HighestOther = nullptr;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("KodLighting"))))
		{
			Owned = *It;
		}
		else if (It->bUnbound && (!HighestOther || It->Priority > HighestOther->Priority))
		{
			HighestOther = *It;
		}
	}
	if (!Owned)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Owned = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
		if (Owned)
		{
			Owned->Tags.Add(FName(TEXT("KodLighting")));
		}
	}
	if (Owned)
	{
		Owned->bUnbound = true;
		Owned->bEnabled = true;
		Owned->Priority = 10000.f;
		Owned->BlendWeight = 1.f;
		WriteKodGrade(Owned->Settings, Lighting);
		UE_LOG(LogTemp, Log, TEXT("KodLighting Volume Name=%s Priority=%.0f Weight=%.2f Other=%s OtherPriority=%.0f Exposure=%.2f WB=%.0f ShadowGrade=%s"),
			*Owned->GetName(),
			Owned->Priority,
			Owned->BlendWeight,
			HighestOther ? *HighestOther->GetName() : TEXT("None"),
			HighestOther ? HighestOther->Priority : 0.f,
			Owned->Settings.AutoExposureBias,
			Owned->Settings.WhiteTemp,
			*Lighting.ShadowGrade.ToString());
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UCameraComponent*> Cameras;
		It->GetComponents(Cameras);
		for (UCameraComponent* Camera : Cameras)
		{
			if (!Camera)
			{
				continue;
			}
			Camera->PostProcessBlendWeight = 1.f;
			WriteKodGrade(Camera->PostProcessSettings, Lighting);
			UE_LOG(LogTemp, Log, TEXT("KodLighting Camera Name=%s Blend=%.2f"), *Camera->GetName(), Camera->PostProcessBlendWeight);
		}
	}
}

static void ApplySc2Look(UWorld* World)
{
	if (!World)
	{
		return;
	}
	auto SetCVar = [](const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByCode);
		}
	};
	SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), 0);
	SetCVar(TEXT("r.ReflectionMethod"), 0);
	SetCVar(TEXT("r.DefaultFeature.MotionBlur"), 0);
	SetCVar(TEXT("r.DefaultFeature.AutoExposure"), 0);
	float ShotSeconds = -1.f;
	const bool bShotAfter = FParse::Value(FCommandLine::Get(), TEXT("-kod.ShotAfter="), ShotSeconds)
		|| FParse::Value(FCommandLine::Get(), TEXT("kod.ShotAfter="), ShotSeconds);
	AKodSlice0GameMode::ApplyMatchExposure(World);
	bool bPlacedSun = false;
	for (TActorIterator<ADirectionalLight> SunIt(World); SunIt; ++SunIt)
	{
		if (*SunIt)
		{
			bPlacedSun = true;
			break;
		}
	}
	bool bHaveSky = false;
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		bHaveSky = true;
		if (USkyLightComponent* Sky = It->GetLightComponent())
		{
			if (Sky->Mobility == EComponentMobility::Static)
			{
				UE_LOG(LogTemp, Log, TEXT("KodLight PlacedStatic Name=%s"), *It->GetName());
				Sky->SetMobility(EComponentMobility::Movable);
				Sky->RecaptureSky();
				UE_LOG(LogTemp, Log, TEXT("KodLight SkyPromoted Name=%s"), *It->GetName());
			}
			else if (bShotAfter)
			{
				Sky->RecaptureSky();
			}
		}
		break;
	}
	// A level that already has a sun keeps its own sky. An empty map still gets fill.
	if (!bHaveSky && !bPlacedSun)
	{
		ASkyLight* SkyActor = World->SpawnActor<ASkyLight>();
		if (SkyActor)
		{
			if (USkyLightComponent* Sky = SkyActor->GetLightComponent())
			{
				Sky->SetMobility(EComponentMobility::Movable);
				Sky->SetIntensity(0.15f);
				Sky->SetLightColor(FLinearColor(0.55f, 0.62f, 0.72f));
			}
		}
	}
	bool bHaveFog = false;
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		bHaveFog = true;
		break;
	}
	if (!bHaveFog && !bPlacedSun)
	{
		AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>();
		if (Fog && Fog->GetComponent())
		{
			UExponentialHeightFogComponent* FogComp = Fog->GetComponent();
			FogComp->SetFogDensity(0.004f);
			FogComp->SetFogHeightFalloff(0.2f);
			FogComp->SetFogInscatteringColor(FLinearColor(0.45f, 0.5f, 0.58f));
			FogComp->SetVolumetricFog(false);
		}
	}
	const bool bTraining = KodFaultline::IsTrainingMap(World);
	AKodSlice0GameMode::KeepSingleDirectionalSun(World, bTraining);
	if (bTraining || World->GetMapName().Contains(TEXT("Faultline")))
	{
		ApplyKodFillLights(World, ReadKodLighting());
	}
	UE_LOG(LogTemp, Log, TEXT("KodFaultline Sun single directional tutorial=%d PlacedSun=%d Sky=%d Fog=%d"),
		bTraining ? 1 : 0,
		bPlacedSun ? 1 : 0,
		bHaveSky ? 1 : 0,
		bHaveFog ? 1 : 0);
}

void AKodSlice0GameMode::StartPlay()
{
	// The first HUD draw can land inside Super::StartPlay. Light the world first.
	ApplySc2Look(GetWorld());
	Super::StartPlay();
	int32 CommandResX = 0;
	int32 CommandResY = 0;
	const TCHAR* CommandLine = FCommandLine::Get();
	if (CommandLine
		&& FParse::Value(CommandLine, TEXT("-ResX="), CommandResX)
		&& FParse::Value(CommandLine, TEXT("-ResY="), CommandResY)
		&& CommandResX >= 320
		&& CommandResY >= 240
		&& GEngine)
	{
		if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
		{
			UserSettings->SetScreenResolution(FIntPoint(CommandResX, CommandResY));
			UserSettings->SetFullscreenMode(EWindowMode::Windowed);
			UserSettings->ApplyResolutionSettings(false);
			UE_LOG(LogTemp, Log, TEXT("KodRes CommandLine %dx%d"), CommandResX, CommandResY);
		}
	}
	if (GetWorld() && GetWorld()->URL.HasOption(TEXT("Menu")))
	{
		if (UKodSimSubsystem* Sim = GetWorld()->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->SetMenuMode(true);
		}
	}

	// Placed greybox has already run BeginPlay. Mute before the smoke Ranger spawn.
	MuteGreyboxProxyCollision();
	ApplyWorldGroundChannels(GetWorld());
	ApplySc2Look(GetWorld());
	ApplyStartingResources();

	UKodSlice0Bootstrap::EnsureCatalog(this);

	// Touch Resolve so soft-path miss path is exercised in logs / debugger.
	UKodSlice0Bootstrap::ResolveWeapon(FName(KodWardenPaths::Id_RangerRifle), this);
	UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_Marine), this);
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			auto RegisterUnit = [Sim](const UKodUnitDefinition* Def, FName FallbackId, FName DisplayId)
			{
				if (!Def)
				{
					return;
				}
				FKodTrainSpec Spec;
				Spec.UnitId = Def->DefinitionId.IsNone() ? FallbackId : Def->DefinitionId;
				Spec.DisplayId = DisplayId;
				Spec.Cost = Def->Cost;
				if (Spec.Cost.BuildTime <= 0)
				{
					Spec.Cost.BuildTime = Def->BuildTicks;
				}
				Spec.bCanGather = Def->bCanGather;
				Spec.bBiological = Def->bBiological;
				Spec.bInfantry = Def->bInfantry;
				Spec.bFlyer = Def->bFlyer;
				Spec.bOrganic = Def->UsesOrganicDeath();
				Spec.bHealer = Def->bHealer;
				Spec.CargoSize = Def->CargoSize;
				Spec.BaseArmor = FMath::Max(0, FMath::RoundToInt(Def->Armor));
				Spec.EnergyMax = Def->EnergyMax;
				Spec.EnergyStart = Def->EnergyStart;
				Spec.RequiresId = Def->RequiresId;
				Sim->RegisterTrainSpec(Spec);
				if (!Def->Wireframe.IsNull())
				{
					FKodUiArt::RegisterWireframe(Spec.UnitId, Def->Wireframe.ToSoftObjectPath());
				}
				if (!Def->CommandIcon.IsNull())
				{
					FKodUiArt::RegisterIcon(Spec.UnitId, Def->CommandIcon.ToSoftObjectPath());
				}
			};
			if (const UKodUnitDefinition* Amp = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_Dozer), this))
			{
				RegisterUnit(Amp, FName(KodWardenPaths::Id_Dozer), FName(TEXT("AMP")));
			}
			if (const UKodUnitDefinition* Marine = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_Marine), this))
			{
				RegisterUnit(Marine, FName(KodWardenPaths::Id_Marine), FName(TEXT("Marine")));
				FKodTrainSpec Spec;
				Spec.UnitId = Marine->DefinitionId.IsNone() ? FName(KodWardenPaths::Id_Marine) : Marine->DefinitionId;
				if (Marine->Abilities.Num() > 0)
				{
					for (const FKodAbilitySpec& Ability : Marine->Abilities)
					{
						Sim->RegisterUnitAbility(Spec.UnitId, Ability);
					}
				}
				else if (const UKodAbilityDefinition* Stim = UKodSlice0Bootstrap::MakeCombatStim(this))
				{
					Sim->RegisterUnitAbility(Spec.UnitId, Stim->ToSpec());
				}
				if (const UKodAbilityDefinition* StimAsset = Cast<UKodAbilityDefinition>(UKodSlice0Bootstrap::ResolveDefinition(FName(KodWardenPaths::Id_CombatStim), this)))
				{
					if (!StimAsset->CommandIcon.IsNull())
					{
						FKodUiArt::RegisterIcon(FName(KodWardenPaths::Id_CombatStim), StimAsset->CommandIcon.ToSoftObjectPath());
					}
				}
			}
			if (const UKodUnitDefinition* Scinder = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_ScinderInfantry), this))
			{
				RegisterUnit(Scinder, FName(KodWardenPaths::Id_ScinderInfantry), FName(TEXT("Scinder Infantry")));
			}
			if (const UKodUnitDefinition* Fighter = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_ScnFighter), this))
			{
				RegisterUnit(Fighter, FName(KodWardenPaths::Id_ScnFighter), FName(TEXT("Fighter")));
			}
			if (const UKodUnitDefinition* Blade = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_ScnTwinblade), this))
			{
				RegisterUnit(Blade, FName(KodWardenPaths::Id_ScnTwinblade), FName(TEXT("Twin-blade")));
			}
			if (const UKodUnitDefinition* ScnWorker = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_ScnWorker), this))
			{
				RegisterUnit(ScnWorker, FName(KodWardenPaths::Id_ScnWorker), FName(TEXT("Worker")));
			}
			auto RegisterBuild = [Sim, this](const TCHAR* TypeId, const TArray<FName>& Trains)
			{
				const UKodBuildingDefinition* Def = UKodSlice0Bootstrap::ResolveBuilding(FName(TypeId), this);
				if (!Def)
				{
					return;
				}
				FKodBuildSpec Spec;
				Spec.TypeId = Def->DefinitionId.IsNone() ? FName(TypeId) : Def->DefinitionId;
				Spec.DisplayId = FName(TypeId);
				Spec.FootprintX = FMath::Max(1, Def->FootprintX);
				Spec.FootprintY = FMath::Max(1, Def->FootprintY);
				Spec.Cost = Def->Cost;
				if (Spec.Cost.BuildTime <= 0)
				{
					Spec.Cost.BuildTime = Def->GetResolvedBuildTicks();
				}
				Spec.MaxHealth = Def->MaxHealth;
				Spec.SupplyProvided = Def->SupplyProvided;
				Spec.bIsDropOff = Def->bIsDropOff;
				Spec.bExtractor = Def->bExtractor;
				Spec.Trains = Trains;
				Spec.RequiresId = Def->RequiresId;
				Spec.MenuTier = Def->MenuTier;
				Spec.bTownHall = Def->bTownHall;
				Spec.bCanLower = Def->bCanLower;
				Spec.CargoCap = Def->CargoCap;
				Spec.AttackDamage = Def->AttackDamage;
				Spec.AttackRangeUU = Def->AttackRangeUU;
				Spec.bAttacksAir = Def->bAttacksAir;
				Spec.Armor = Def->Armor;
				Spec.bArmored = Def->AttributeTags.HasTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Attribute.Armored")), false));
				Spec.bStructure = Def->AttributeTags.HasTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Attribute.Structure")), false));
				if (Spec.TypeId == FName(KodWardenPaths::Id_CommandCenter))
				{
					Spec.bTownHall = true;
					Spec.bIsDropOff = true;
				}
				else if (Spec.TypeId == FName(KodWardenPaths::Id_Barracks) && Spec.RequiresId.IsNone())
				{
					Spec.RequiresId = FName(KodWardenPaths::Id_SupplyDepot);
				}
				else if (Spec.TypeId == FName(KodWardenPaths::Id_SupplyDepot))
				{
					Spec.bCanLower = true;
				}
				else if (Spec.TypeId == FName(KodWardenPaths::Id_Bunker))
				{
					if (Spec.CargoCap <= 0)
					{
						Spec.CargoCap = 4;
						Spec.MaxHealth = 400.f;
					}
					if (Spec.RequiresId.IsNone())
					{
						Spec.RequiresId = FName(KodWardenPaths::Id_Barracks);
					}
					if (Spec.Armor <= 0)
					{
						Spec.Armor = 1;
					}
					Spec.bArmored = true;
					Spec.bStructure = true;
				}
				else if (Spec.TypeId == FName(KodWardenPaths::Id_Turret))
				{
					Spec.MenuTier = 1;
					if (Spec.RequiresId.IsNone())
					{
						Spec.RequiresId = FName(KodWardenPaths::Id_EngineeringBay);
					}
					if (Spec.AttackDamage <= 0)
					{
						Spec.AttackDamage = 12;
						Spec.AttackRangeUU = 7 * 200;
						Spec.bAttacksAir = true;
					}
				}
				Sim->RegisterBuildSpec(Spec);
				if (!Def->Wireframe.IsNull())
				{
					FKodUiArt::RegisterWireframe(Spec.TypeId, Def->Wireframe.ToSoftObjectPath());
				}
				if (!Def->CommandIcon.IsNull())
				{
					FKodUiArt::RegisterIcon(Spec.TypeId, Def->CommandIcon.ToSoftObjectPath());
				}
			};
			TArray<FName> NoTrains;
			TArray<FName> MarineTrain;
			MarineTrain.Add(FName(KodWardenPaths::Id_Marine));
			MarineTrain.Add(FName(KodWardenPaths::Id_Medic));
			RegisterBuild(KodWardenPaths::Id_CommandCenter, NoTrains);
			RegisterBuild(KodWardenPaths::Id_SupplyDepot, NoTrains);
			RegisterBuild(KodWardenPaths::Id_Barracks, MarineTrain);
			RegisterBuild(KodWardenPaths::Id_Rig, NoTrains);
			RegisterBuild(KodWardenPaths::Id_EngineeringBay, NoTrains);
			RegisterBuild(KodWardenPaths::Id_Bunker, NoTrains);
			RegisterBuild(KodWardenPaths::Id_Turret, NoTrains);
			if (const UKodUnitDefinition* Medic = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_Medic), this))
			{
				RegisterUnit(Medic, FName(KodWardenPaths::Id_Medic), FName(TEXT("Medic")));
			}
			auto StampRoster = [Sim](FName UnitId, bool bBiological, bool bInfantry, bool bHealer, int32 Cargo, int32 EnergyMax, int32 EnergyStart, FName RequiresId)
			{
				FKodTrainSpec Spec;
				if (!Sim->TryGetTrainSpec(UnitId, Spec))
				{
					return;
				}
				Spec.bBiological = bBiological;
				Spec.bInfantry = bInfantry;
				Spec.bHealer = bHealer;
				Spec.CargoSize = Cargo;
				Spec.EnergyMax = EnergyMax;
				Spec.EnergyStart = EnergyStart;
				if (!RequiresId.IsNone())
				{
					Spec.RequiresId = RequiresId;
				}
				Sim->RegisterTrainSpec(Spec);
			};
			StampRoster(FName(KodWardenPaths::Id_Dozer), true, false, false, 0, 0, 0, FName());
			StampRoster(FName(KodWardenPaths::Id_Marine), true, true, false, 1, 0, 0, FName());
			if (FKodTrainSpec MarineLight; Sim->TryGetTrainSpec(FName(KodWardenPaths::Id_Marine), MarineLight))
			{
				MarineLight.bLight = true;
				Sim->RegisterTrainSpec(MarineLight);
			}
			StampRoster(FName(KodWardenPaths::Id_Medic), true, true, true, 1, 200, 50, FName(KodWardenPaths::Id_EngineeringBay));
			FKodUpgradeSpec Weapons;
			Weapons.UpgradeId = FName(KodWardenPaths::Id_InfantryWeapons);
			Weapons.DisplayId = FName(TEXT("Infantry Weapons"));
			Weapons.BuildingId = FName(KodWardenPaths::Id_EngineeringBay);
			Weapons.Credits = 100;
			Weapons.Luminene = 100;
			Weapons.BuildTicks = 114 * 16;
			Weapons.WeaponBonus = 1;
			Sim->RegisterUpgradeSpec(Weapons);
			FKodUpgradeSpec Armor;
			Armor.UpgradeId = FName(KodWardenPaths::Id_InfantryArmor);
			Armor.DisplayId = FName(TEXT("Infantry Armor"));
			Armor.BuildingId = FName(KodWardenPaths::Id_EngineeringBay);
			Armor.Credits = 100;
			Armor.Luminene = 100;
			Armor.BuildTicks = 114 * 16;
			Armor.ArmorBonus = 1;
			Sim->RegisterUpgradeSpec(Armor);
			FKodUpgradeSpec Feed;
			Feed.UpgradeId = FName(KodWardenPaths::Id_SatelliteFeed);
			Feed.DisplayId = FName(TEXT("Satellite Feed"));
			Feed.BuildingId = FName(KodWardenPaths::Id_CommandCenter);
			Feed.Credits = 150;
			Feed.Luminene = 150;
			Feed.BuildTicks = 80 * 16;
			Feed.SatelliteFeed = 1;
			Feed.EnergyMax = 200;
			Feed.EnergyStart = 50;
			Sim->RegisterUpgradeSpec(Feed);
			// Placeholder until the Director stamps it. 14 ticks is 0.875s at 16 Hz.
			FKodUpgradeSpec BunkerTurret;
			BunkerTurret.UpgradeId = FName(KodWardenPaths::Id_BunkerTurret);
			BunkerTurret.DisplayId = FName(TEXT("Bunker Turret"));
			BunkerTurret.BuildingId = FName(KodWardenPaths::Id_Bunker);
			BunkerTurret.Credits = 50;
			BunkerTurret.Luminene = 25;
			BunkerTurret.BuildTicks = 25 * 16;
			BunkerTurret.AttackDamage = 12;
			BunkerTurret.AttackRangeUU = 1200;
			BunkerTurret.AttacksAir = 0;
			BunkerTurret.CooldownTicks = 14;
			BunkerTurret.BonusVsLight = 6;
			BunkerTurret.BonusHealth = 100;
			Sim->RegisterUpgradeSpec(BunkerTurret);
			auto AddOrbital = [Sim](FName Id, int32 Energy, int32 Cooldown, int32 Duration, int32 Supply, int32 GatherRate, int32 Sight, int32 Radius)
			{
				FKodAbilitySpec Spec;
				Spec.AbilityId = Id;
				Spec.DisplayName = Id;
				Spec.EnergyCost = Energy;
				Spec.CooldownTicks = Cooldown;
				Spec.DurationTicks = Duration;
				Spec.SupplyBonus = Supply;
				Spec.GatherRatePermille = GatherRate;
				Spec.SightUU = Sight;
				Spec.OrbitRadiusUU = Radius;
				Sim->RegisterAbilitySpec(Spec);
			};
			AddOrbital(FName(KodWardenPaths::Id_OrbitalGathererDrop), 50, 0, 0, 0, 2000, 0, 0);
			AddOrbital(FName(KodWardenPaths::Id_SupplyBoost), 50, 0, 0, 8, 1000, 0, 0);
			AddOrbital(FName(KodWardenPaths::Id_IsrView), 50, 16 * 4, 30 * 16, 0, 1000, 1200, 500);
			if (const UKodUnitDefinition* Gatherer = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_OrbitalGatherer), this))
			{
				RegisterUnit(Gatherer, FName(KodWardenPaths::Id_OrbitalGatherer), FName(TEXT("Orbital Gatherer")));
			}
			if (const UKodUnitDefinition* Platform = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_IsrPlatform), this))
			{
				RegisterUnit(Platform, FName(KodWardenPaths::Id_IsrPlatform), FName(TEXT("ISR Platform")));
			}
		}
	}

	// Nodes and workers seat off the command center after it has moved.
	const bool bFaultline = KodFaultline::IsTrainingMap(GetWorld());
	if (bFaultline)
	{
		PlayableCells = 112;
	}
	PublishPlayableBounds();
	if (bFaultline)
	{
		KodFaultline::Begin(GetWorld());
		SpawnWorkersIfNeeded();
	}
	else
	{
		SeatPlayerBase();
		SpawnResourceField();
		SpawnWorkersIfNeeded();
		SpawnAiOpponent();
		PlaceExpansionFields();
	}
	if (UWorld* InsetWorld = GetWorld())
	{
		if (UKodSimSubsystem* InsetSim = InsetWorld->GetSubsystem<UKodSimSubsystem>())
		{
			const bool bInset = InsetSim->AreResourcePointsInset(2);
			UE_LOG(LogTemp, Log, TEXT("KodPlayable Inset=2 OK=%d"), bInset ? 1 : 0);
			checkf(bInset, TEXT("Resource node or stand is closer than 2 cells to the playable edge"));
		}
	}
	if (UWorld* StartWorld = GetWorld())
	{
		const FKodMatchStart MatchStart = FKodSkirmishSetup::ResolveMatchStart(FCommandLine::Get(), GetMatchOptions());
		if (UKodSimSubsystem* StartSim = StartWorld->GetSubsystem<UKodSimSubsystem>())
		{
			if (MatchStart.bPrompt)
			{
				StartSim->SetSimPaused(true);
				SetAwaitFactionPrompt(true);
				if (APlayerController* StartPC = StartWorld->GetFirstPlayerController())
				{
					if (AKodHUD* StartHUD = Cast<AKodHUD>(StartPC->GetHUD()))
					{
						StartHUD->OpenFactionPrompt();
					}
				}
			}
			else
			{
				ApplyStartFaction(MatchStart.FactionIndex);
			}
		}
		for (TActorIterator<ALight> LightIt(StartWorld); LightIt; ++LightIt)
		{
			const ULightComponent* PlacedLight = LightIt->GetLightComponent();
			if (PlacedLight && PlacedLight->Mobility == EComponentMobility::Static)
			{
				UE_LOG(LogTemp, Log, TEXT("KodLight PlacedStatic Name=%s"), *LightIt->GetName());
			}
		}
	}
	FocusOpeningCamera();
	if (UWorld* LayoutWorld = GetWorld())
	{
		KodMapLayout::Load(LayoutWorld);
	}
	if (UWorld* DressWorld = GetWorld())
	{
		FActorSpawnParameters DressParams;
		DressParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		DressWorld->SpawnActor<AKodMapLife>(AKodMapLife::StaticClass(), FTransform::Identity, DressParams);
	}
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->EnsureBalanceTables();
			Sim->ApplyBalance();
		}
	}
	ApplyWorldGroundChannels(GetWorld());
	const UWorld* ReadyWorld = GetWorld();
	UE_LOG(LogTemp, Log, TEXT("KodPlay Ready Map=%s Training=%d Prompt=%d"),
		ReadyWorld ? *FPaths::GetBaseFilename(ReadyWorld->GetMapName()) : TEXT("none"),
		KodFaultline::IsTrainingMap(ReadyWorld) ? 1 : 0,
		IsAwaitingFactionPrompt() ? 1 : 0);
}

void AKodSlice0GameMode::PublishPlayableBounds()
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return;
	}

	const int32 Cells = FMath::Clamp(PlayableCells, 16, 512);
	const float Half = static_cast<float>(Cells * KodBuildGrid::CellUU) * 0.5f;
	FBox Chosen(FVector(-Half, -Half, -200.f), FVector(Half - 1.f, Half - 1.f, 400.f));

	FBox Placed(ForceInit);
	bool bPlaced = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->ActorHasTag(FName(TEXT("KodGeneratedGround"))))
		{
			continue;
		}
		const FString Label = Actor->GetActorNameOrLabel();
		if (!Actor->ActorHasTag(FName(TEXT("KodGround"))) && !Label.Contains(TEXT("PROXY_GROUND")))
		{
			continue;
		}
		const FBox Bounds = Actor->GetComponentsBoundingBox(true);
		if (!Bounds.IsValid)
		{
			continue;
		}
		Placed += Bounds;
		bPlaced = true;
	}
	if (bPlaced && Placed.GetSize().X > Chosen.GetSize().X && Placed.GetSize().Y > Chosen.GetSize().Y)
	{
		Chosen = Placed;
	}

	bool bHavePlane = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (*It && (*It)->ActorHasTag(FName(TEXT("KodGeneratedGround"))))
		{
			bHavePlane = true;
			break;
		}
	}
	if (!bHavePlane)
	{
		UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		UMaterialInterface* GroundMat = nullptr;
		if (FModuleManager::Get().ModuleExists(TEXT("AssetRegistry")))
		{
			FAssetRegistryModule& Assets = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& Registry = Assets.Get();
			Registry.WaitForCompletion();
			const TCHAR* Paths[] = {
				TEXT("/Game/FancyDev/Materials/MI_fancy_grid.MI_fancy_grid"),
				TEXT("/Game/FancyDev/Materials/MI_fancy_grid_dark.MI_fancy_grid_dark"),
				TEXT("/Game/Advanced_Auto_Material/Landscape_Material_Instances/MI_Adv_MM_Landscape_28_Desert_Tutorial.MI_Adv_MM_Landscape_28_Desert_Tutorial")
			};
			for (const TCHAR* Path : Paths)
			{
				const FSoftObjectPath Soft(Path);
				if (!Registry.GetAssetByObjectPath(Soft).IsValid())
				{
					continue;
				}
				GroundMat = Cast<UMaterialInterface>(Soft.TryLoad());
				if (GroundMat)
				{
					break;
				}
			}
		}
		if (!GroundMat)
		{
			GroundMat = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		if (PlaneMesh)
		{
			const FVector Center = Chosen.GetCenter();
			const FVector Extent = Chosen.GetExtent();
			const FTransform Xform(FRotator::ZeroRotator, FVector(Center.X, Center.Y, -2.f));
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Plane = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Xform, Params);
			if (Plane)
			{
				Plane->Tags.Add(FName(TEXT("KodGround")));
				Plane->Tags.Add(FName(TEXT("KodGeneratedGround")));
				UStaticMeshComponent* Mesh = Plane->GetStaticMeshComponent();
				if (Mesh)
				{
					Mesh->SetMobility(EComponentMobility::Movable);
					Mesh->SetStaticMesh(PlaneMesh);
					Plane->SetActorScale3D(FVector(FMath::Max(1.f, Extent.X * 2.f / 100.f), FMath::Max(1.f, Extent.Y * 2.f / 100.f), 1.f));
					if (GroundMat)
					{
						Mesh->SetMaterial(0, GroundMat);
					}
					Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
					Mesh->SetCollisionResponseToChannel(KodCollision::Ground, ECR_Block);
					Mesh->SetCollisionResponseToChannel(KodCollision::Select, ECR_Ignore);
					Mesh->SetCanEverAffectNavigation(false);
				}
			}
		}
	}

	Sim->SetPlayableWorldBox(Chosen);
	UE_LOG(LogTemp, Log, TEXT("KodPlayable Min=%.0f,%.0f Max=%.0f,%.0f"), Chosen.Min.X, Chosen.Min.Y, Chosen.Max.X, Chosen.Max.Y);

	for (TActorIterator<ADirectionalLight> LightIt(World); LightIt; ++LightIt)
	{
		if (ULightComponent* Light = LightIt->GetLightComponent())
		{
			Light->SetMobility(EComponentMobility::Movable);
		}
	}
	for (TActorIterator<ASkyLight> SkyIt(World); SkyIt; ++SkyIt)
	{
		if (USkyLightComponent* Sky = SkyIt->GetLightComponent())
		{
			Sky->SetMobility(EComponentMobility::Movable);
		}
	}

	bool bGeneratedGround = false;
	for (TActorIterator<AActor> GroundIt(World); GroundIt; ++GroundIt)
	{
		if (*GroundIt && (*GroundIt)->ActorHasTag(FName(TEXT("KodGeneratedGround"))))
		{
			bGeneratedGround = true;
			break;
		}
	}
	if (bGeneratedGround)
	{
		for (TActorIterator<AActor> HideIt(World); HideIt; ++HideIt)
		{
			AActor* Blockout = *HideIt;
			if (!Blockout || Blockout->ActorHasTag(FName(TEXT("KodGeneratedGround"))))
			{
				continue;
			}
			const bool bOldFloor = IsSlice0GroundFloor(Blockout) || Blockout->ActorHasTag(FName(TEXT("KodGround")));
			const bool bGreybox = Blockout->ActorHasTag(FName(TEXT("KodGreybox"))) || IsSlice0GreyboxProxy(Blockout);
			if (!bOldFloor && !bGreybox)
			{
				continue;
			}
			if (IsSlice0CommandCenter(Blockout))
			{
				continue;
			}
			Blockout->SetActorHiddenInGame(true);
		}
	}
}

void AKodSlice0GameMode::SeatPlayerBase()
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return;
	}
	AActor* Anchor = PickBaseToSeat(World);
	if (!Anchor)
	{
		return;
	}
	Anchor->Tags.AddUnique(FName(TEXT("KodStartSpot_0")));
	constexpr float Base = 8300.f;
	TArray<AActor*> Ignored;
	Ignored.Add(Anchor);
	const float FloorZ = ResolveResourceFloorZ(World, Sim, -Base, -Base, Anchor->GetActorLocation().Z, Ignored);
	PlaceActorAt(Anchor, FVector(-Base, -Base, FloorZ));
	const FVector Seated = Anchor->GetActorLocation();
	if (FKodEntityId SeatedId = Sim->FindIdForActor(Anchor); SeatedId.IsValid())
	{
		Sim->SetEntityGroundPosition(SeatedId, Seated);
	}
	UE_LOG(LogTemp, Log, TEXT("KodBase Player Loc=%.0f,%.0f,%.0f"), Seated.X, Seated.Y, Seated.Z);
}

void AKodSlice0GameMode::PlaceExpansionFields()
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return;
	}
	const FVector Sites[2] = { FVector(-8300.f, -2900.f, 0.f), FVector(8300.f, 2900.f, 0.f) };
	const FVector Facing[2] = { FVector(-1.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f) };
	TArray<AActor*> Ignored;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const float FloorZ = ResolveResourceFloorZ(World, Sim, Sites[Index].X, Sites[Index].Y, 0.f, Ignored);
		PlaceFieldAt(FVector(Sites[Index].X, Sites[Index].Y, FloorZ), Facing[Index]);
		UE_LOG(LogTemp, Log, TEXT("KodEcon Expand Loc=%.0f,%.0f"), Sites[Index].X, Sites[Index].Y);
	}
}

void AKodSlice0GameMode::FocusOpeningCamera()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (AActor* Base = FindPlayerBaseActor(World))
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			FKodStructureState Structure;
			if (Sim->TryGetStructure(Sim->FindIdForActor(Base), Structure)
				&& FVector::Dist2D(Structure.Position, Base->GetActorLocation()) > 300.f
				&& (FMath::Abs(Structure.Position.X) > 50.f || FMath::Abs(Structure.Position.Y) > 50.f))
			{
				PlaceActorAt(Base, Structure.Position);
			}
		}
		TArray<AKodUnit*> Crew;
		for (TActorIterator<AKodUnit> It(World); It; ++It)
		{
			AKodUnit* Unit = *It;
			if (!Unit || Unit->TeamId != 0)
			{
				continue;
			}
			bool bWorker = Unit->ActorHasTag(FName(TEXT("KodUnitDozer")));
			if (!bWorker)
			{
				if (const UKodUnitDefinition* Def = Unit->GetDefinition())
				{
					const FName DefId = Def->DefinitionId.IsNone() ? Def->GetFName() : Def->DefinitionId;
					bWorker = Def->bCanGather || DefId == FName(KodWardenPaths::Id_Dozer) || DefId == FName(KodWardenPaths::Id_ScnWorker);
				}
			}
			if (bWorker)
			{
				Crew.Add(Unit);
			}
		}
		Crew.Sort([](const AKodUnit& A, const AKodUnit& B)
		{
			return A.EntityId.Value < B.EntityId.Value;
		});
		UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
		const FVector Center = Base->GetActorLocation();
		for (int32 Index = 0; Index < Crew.Num(); ++Index)
		{
			const float Angle = (360.f / FMath::Max(1, Crew.Num())) * static_cast<float>(Index);
			const FVector Spot = Center + FVector(1.f, 0.f, 0.f).RotateAngleAxis(Angle, FVector::UpVector) * 640.f;
			PlaceActorAt(Crew[Index], FVector(Spot.X, Spot.Y, Center.Z));
			if (Sim && Crew[Index]->EntityId.IsValid())
			{
				Sim->SetEntityGroundPosition(Crew[Index]->EntityId, Crew[Index]->GetActorLocation());
			}
		}
		ApplyCommandCenterMesh(Base);
		UE_LOG(LogTemp, Log, TEXT("KodCam WorkersOnCC Count=%d Loc=%.0f,%.0f"), Crew.Num(), Center.X, Center.Y);
	}
	for (TActorIterator<AKodRTSCameraPawn> It(World); It; ++It)
	{
		if (It->IsA(AKodSpectatorPawn::StaticClass()))
		{
			continue;
		}
		It->SnapToPlayerStart();
		It->LogOpeningCamera();
	}
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (AKodPlayerController* KodPC = Cast<AKodPlayerController>(PC))
		{
			KodPC->SelectPlayerCommandCenter();
		}
	}
}

void AKodSlice0GameMode::SpawnHostileTestTarget()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Local smoke Ranger is team 0 (AKodPlayerState default). Team 1 is the attack target.
	constexpr int32 HostileTeamId = 1;
	const FVector Fallback(1200.f, 600.f, 100.f);
	FVector Location = Fallback;
	bool bFoundAnchor = false;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->ActorHasTag(FName(TEXT("KodHostileAnchor"))))
		{
			Location = Actor->GetActorLocation();
			bFoundAnchor = true;
			break;
		}
	}
	if (!bFoundAnchor)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (IsSlice0HostileAnchorLabel(Actor))
			{
				Location = Actor->GetActorLocation();
				bFoundAnchor = true;
				break;
			}
		}
	}

	if (bFoundAnchor)
	{
		// Project onto the floor the smoke Ranger stands on. The label's own Z is text height.
		Location.Z = SmokeRangerOffset.Z;
	}
	else
	{
		Location = Fallback;
	}

	const TCHAR* Anchor = bFoundAnchor ? TEXT("PROXY_HOSTILE") : TEXT("fallback");
	const FTransform Transform(FRotator::ZeroRotator, Location);
	// Same Marine definition as the player infantry (skeletal mesh + AnimBP when they load).
	// Cube only if that mesh soft ref misses. TeamColor tints team 1 red. No AI controller.
	AKodUnit* Hostile = UKodSlice0Bootstrap::SpawnMarine(World, Transform, HostileTeamId, /*bForceCubeBody*/ false);
	if (!Hostile)
	{
		UE_LOG(LogTemp, Warning, TEXT("Slice0 Hostile spawn failed Anchor=%s Loc=%.0f,%.0f,%.0f"),
			Anchor,
			Location.X,
			Location.Y,
			Location.Z);
		return;
	}

	const FVector Spawned = Hostile->GetActorLocation();
	UE_LOG(LogTemp, Log, TEXT("Slice0 Hostile spawned Name=%s Team=%d Loc=%.0f,%.0f,%.0f Anchor=%s Body=%s"),
		*Hostile->GetName(),
		Hostile->TeamId,
		Spawned.X,
		Spawned.Y,
		Spawned.Z,
		Anchor,
		Hostile->GetMountedBodyName());
}

void AKodSlice0GameMode::ApplyStartingResources()
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	Sim->SetTeamBank(0, StartingJadeite, StartingLuminene);
	UE_LOG(LogTemp, Log, TEXT("KodEcon Bank Team=0 Jadeite=%d Luminene=%d"), StartingJadeite, StartingLuminene);
}

void AKodSlice0GameMode::PlaceFieldAt(const FVector& AnchorLocation, const FVector& FieldDir)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return;
	}
	UKodSlice0Bootstrap::EnsureCatalog(this);
	UKodResourceNodeDefinition* JadeiteDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_JadeiteNode), this);
	UKodResourceNodeDefinition* LumineneDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_LumineneVent), this);
	if (!LumineneDef)
	{
		LumineneDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_OilSource), this);
	}

	auto RegisterNode = [&](AActor* Actor, EKodResourceType Type, const UKodResourceNodeDefinition* Def) -> bool
	{
		if (!Actor)
		{
			return false;
		}
		const bool bLuminene = Type == EKodResourceType::Luminene;
		FKodResourceNodeSpec Spec;
		Spec.Type = Type;
		Spec.Amount = Def ? Def->Amount : (bLuminene ? KodEconomyDefaults::LumineneNodeAmount : KodEconomyDefaults::JadeiteNodeAmount);
		Spec.HarvestPerTrip = Def ? Def->HarvestPerTrip : (bLuminene ? KodEconomyDefaults::LumineneHarvestPerTrip : KodEconomyDefaults::JadeiteHarvestPerTrip);
		Spec.HarvestTicks = (Def && Def->HarvestTicks > 0)
			? Def->HarvestTicks
			: (bLuminene ? KodEconomyDefaults::LumineneHarvestTicks : KodEconomyDefaults::JadeiteHarvestTicks);
		Spec.TricklePerTrip = Def ? Def->TricklePerTrip : (bLuminene ? KodEconomyDefaults::LumineneTricklePerTrip : 0);
		Spec.GatherStandUU = (Def && Def->GatherStandUU > 0)
			? Def->GatherStandUU
			: (bLuminene ? KodEconomyDefaults::LumineneGatherStandUU : KodEconomyDefaults::JadeiteGatherStandUU);
		Spec.DefinitionId = (Def && !Def->DefinitionId.IsNone())
			? Def->DefinitionId
			: FName(bLuminene ? KodWardenPaths::Id_LumineneVent : KodWardenPaths::Id_JadeiteNode);
		if (AKodResourceNode* Node = Cast<AKodResourceNode>(Actor))
		{
			if (Def)
			{
				Node->ApplyDefinition(Def);
			}
			else
			{
				Node->ApplyPlaceholder(Type);
			}
		}
		EnableResourceSelectCollision(Actor);
		const FKodEntityId Id = Sim->RegisterResourceNode(Actor, Spec);
		if (!Id.IsValid())
		{
			return false;
		}
		if (AKodResourceNode* Node = Cast<AKodResourceNode>(Actor))
		{
			Node->SetEntityId(Id);
			Node->ApplyJadeiteStandIn();
			Node->SetActorHiddenInGame(false);
		}
		const FVector SpawnedAt = Actor->GetActorLocation();
		UE_LOG(LogTemp, Log, TEXT("KodEcon Node Spawn Id=%d Type=%s Loc=%.0f,%.0f,%.0f"),
			Id.Value,
			KodResourceTypeName(Type),
			SpawnedAt.X,
			SpawnedAt.Y,
			SpawnedAt.Z);
		return true;
	};

	constexpr float ArcRadius = 1160.f;
	constexpr float CrystalSpacing = 120.f;
	constexpr int32 CrystalCount = 6;
	const float SinHalf = FMath::Clamp(CrystalSpacing / (2.f * ArcRadius), 0.f, 0.99f);
	const float StepDeg = FMath::RadiansToDegrees(2.f * FMath::Asin(SinHalf));
	struct FNodePlacement
	{
		EKodResourceType Type = EKodResourceType::Jadeite;
		FVector Location = FVector::ZeroVector;
	};
	TArray<FNodePlacement> Placements;
	Placements.Reserve(CrystalCount + 1);
	for (int32 Index = 0; Index < CrystalCount; ++Index)
	{
		const float Angle = (static_cast<float>(Index) - 2.5f) * StepDeg;
		const FVector Dir = FieldDir.RotateAngleAxis(Angle, FVector::UpVector);
		FNodePlacement Spot;
		Spot.Type = EKodResourceType::Jadeite;
		Spot.Location = AnchorLocation + Dir * ArcRadius;
		Placements.Add(Spot);
	}

	constexpr float GasFoot = 200.f;
	constexpr float CcCorner = 500.f * 1.41421356f;
	const FVector GasDir = FieldDir.RotateAngleAxis(45.f, FVector::UpVector).GetSafeNormal2D();
	float VentRadius = 1400.f;
	FVector VentLocation = AnchorLocation + GasDir * VentRadius;
	for (int32 Push = 0; Push < 8; ++Push)
	{
		float Nearest = MAX_flt;
		for (const FNodePlacement& Spot : Placements)
		{
			Nearest = FMath::Min(Nearest, FVector::Dist2D(VentLocation, Spot.Location));
		}
		const bool bClearsCc = FVector::Dist2D(VentLocation, AnchorLocation) >= CcCorner + GasFoot;
		if (Nearest >= 400.f && bClearsCc)
		{
			break;
		}
		VentRadius += 80.f;
		VentLocation = AnchorLocation + GasDir * VentRadius;
	}
	FNodePlacement VentSpot;
	VentSpot.Type = EKodResourceType::Luminene;
	VentSpot.Location = VentLocation;
	Placements.Add(VentSpot);

	int32 JadeiteCount = 0;
	int32 LumineneCount = 0;
	TArray<AActor*> Spawned;
	for (const FNodePlacement& Spot : Placements)
	{
		const float FloorZ = ResolveResourceFloorZ(World, Sim, Spot.Location.X, Spot.Location.Y, AnchorLocation.Z, Spawned);
		const FTransform Xform(FRotator::ZeroRotator, FVector(Spot.Location.X, Spot.Location.Y, FloorZ));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AKodResourceNode* Node = World->SpawnActor<AKodResourceNode>(AKodResourceNode::StaticClass(), Xform, Params);
		if (!Node)
		{
			continue;
		}
		const UKodResourceNodeDefinition* Def = Spot.Type == EKodResourceType::Luminene ? LumineneDef : JadeiteDef;
		if (!RegisterNode(Node, Spot.Type, Def))
		{
			Node->Destroy();
			continue;
		}
		Spawned.Add(Node);
		if (Spot.Type == EKodResourceType::Luminene)
		{
			++LumineneCount;
		}
		else
		{
			++JadeiteCount;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("KodEcon Nodes Jadeite=%d Luminene=%d Arc=1160 Gas=%.0f Source=Default"),
		JadeiteCount,
		LumineneCount,
		VentRadius);
}

namespace
{
	struct FKodWorkerRing
	{
		FVector Center = FVector::ZeroVector;
		FVector Forward = FVector(1.f, 0.f, 0.f);
		float Radius = 700.f;
		float StepDegrees = 24.f;
	};

	FKodWorkerRing MakeWorkerRing(const FVector& AnchorLocation, const FVector& FieldDir, float FootprintCm, float BodySpanCm, int32 Count, bool bSocket, const FVector& SocketAt)
	{
		FKodWorkerRing Ring;
		Ring.Center = AnchorLocation;
		const float Span = FMath::Max(80.f, BodySpanCm);
		const float UnitRadius = Span * 0.5f;
		const float HalfFoot = FMath::Max(0.f, FootprintCm) * 0.5f;
		Ring.Radius = HalfFoot + UnitRadius;
		const int32 SafeCount = FMath::Max(1, Count);
		const float Needed = (static_cast<float>(SafeCount) * Span) / (2.f * PI);
		Ring.Radius = FMath::Max(Ring.Radius, Needed);
		FVector Dir = FieldDir;
		Dir.Z = 0.f;
		if (bSocket)
		{
			FVector FromCenter = SocketAt - AnchorLocation;
			FromCenter.Z = 0.f;
			if (FromCenter.SizeSquared2D() > 1.f && FromCenter.Size2D() + 1.f >= HalfFoot + UnitRadius)
			{
				Dir = FromCenter;
				Ring.Radius = FMath::Max(Ring.Radius, FromCenter.Size2D());
			}
		}
		Dir = Dir.GetSafeNormal2D();
		Ring.Forward = Dir.IsNearlyZero() ? FVector(1.f, 0.f, 0.f) : Dir;
		Ring.StepDegrees = SafeCount > 1 ? FMath::RadiansToDegrees(Span / Ring.Radius) : 0.f;
		return Ring;
	}

	float AnchorFootprintCm(const UKodSimSubsystem* Sim, AActor* Anchor)
	{
		float Footprint = 1000.f;
		if (Sim && Anchor)
		{
			FKodStructureState Structure;
			if (Sim->TryGetStructure(Sim->FindIdForActor(Anchor), Structure))
			{
				const int32 Cells = FMath::Max(Structure.FootprintX, Structure.FootprintY);
				if (Cells > 0)
				{
					Footprint = static_cast<float>(Cells) * static_cast<float>(KodBuildGrid::CellUU);
				}
			}
		}
		return Footprint;
	}

	FName GathererIdForTeam(const UKodSimSubsystem* Sim, int32 TeamId)
	{
		if (Sim && TeamId == 0 && Sim->GetPlayerFaction() == 1)
		{
			return FName(KodWardenPaths::Id_ScnWorker);
		}
		return FName(KodWardenPaths::Id_Dozer);
	}
}

int32 AKodSlice0GameMode::SpawnWorkerFan(const FVector& AnchorLocation, const FVector& FieldDir, int32 TeamId, int32 Count, AActor* Anchor)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim || Count <= 0)
	{
		return 0;
	}
	FVector SocketAt = AnchorLocation;
	const bool bSocket = (Anchor && KodMeshSockets::TryGet(Anchor, KodMeshSockets::WorkerExit, SocketAt))
		|| (Anchor && KodMeshSockets::TryGet(Anchor, KodMeshSockets::Door, SocketAt));
	const FName WorkerId = GathererIdForTeam(Sim, TeamId);
	const FKodMeshChoice Body = FKodMeshCatalog::Resolve(WorkerId);
	const float BodySpan = Body.BoundsCm > 10.f ? Body.BoundsCm : 80.f;
	const FKodWorkerRing Ring = MakeWorkerRing(AnchorLocation, FieldDir, AnchorFootprintCm(Sim, Anchor), BodySpan, Count, bSocket, SocketAt);
	int32 Spawned = 0;
	TArray<AActor*> Ignored;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = (static_cast<float>(Index) - (static_cast<float>(Count) - 1.f) * 0.5f) * Ring.StepDegrees;
		const FVector Dir = Ring.Forward.RotateAngleAxis(Angle, FVector::UpVector);
		float Radius = Ring.Radius;
		FVector Spot = Ring.Center + Dir * Radius;
		for (int32 Step = 0; Step < 8; ++Step)
		{
			const int32 CellX = KodBuildGrid::CellOf(Spot.X);
			const int32 CellY = KodBuildGrid::CellOf(Spot.Y);
			if (!Sim->IsCellBlocked(CellX, CellY, -1, true) && !CellHoldsResourceNode(Sim, CellX, CellY))
			{
				break;
			}
			Radius += static_cast<float>(KodBuildGrid::CellUU);
			Spot = Ring.Center + Dir * Radius;
		}
		const float FloorZ = ResolveResourceFloorZ(World, Sim, Spot.X, Spot.Y, AnchorLocation.Z, Ignored);
		const FTransform Xform(FRotator::ZeroRotator, FVector(Spot.X, Spot.Y, FloorZ));
		AKodUnit* Worker = UKodSlice0Bootstrap::SpawnGatherer(World, Xform, TeamId, WorkerId);
		if (!Worker)
		{
			continue;
		}
		Ignored.Add(Worker);
		++Spawned;
		const FVector At = Worker->GetActorLocation();
		UE_LOG(LogTemp, Log, TEXT("KodEcon Worker Spawn Id=%d Team=%d Def=%s Loc=%.0f,%.0f,%.0f"),
			Worker->EntityId.Value,
			TeamId,
			*WorkerId.ToString(),
			At.X,
			At.Y,
			At.Z);
	}
	return Spawned;
}

void AKodSlice0GameMode::SpawnResourceField()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Log, TEXT("KodEcon Nodes Jadeite=0 Luminene=0 Source=Default"));
		return;
	}
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	if (!Sim)
	{
		UE_LOG(LogTemp, Log, TEXT("KodEcon Nodes Jadeite=0 Luminene=0 Source=Default"));
		return;
	}

	UKodSlice0Bootstrap::EnsureCatalog(this);
	UKodResourceNodeDefinition* JadeiteDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_JadeiteNode), this);
	UKodResourceNodeDefinition* LumineneDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_LumineneVent), this);
	if (!LumineneDef)
	{
		LumineneDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_OilSource), this);
	}

	struct FTaggedNode
	{
		AActor* Actor = nullptr;
		EKodResourceType Type = EKodResourceType::Jadeite;
		FString SortKey;
	};

	TArray<FTaggedNode> Tagged;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		const bool bJadeite = Actor->ActorHasTag(FName(TEXT("KodResourceJadeite")));
		const bool bLuminene = Actor->ActorHasTag(FName(TEXT("KodResourceLuminene")))
			|| Actor->ActorHasTag(FName(TEXT("KodResourceOil")));
		const bool bPlacedNode = Actor->IsA<AKodResourceNode>();
		if (!bJadeite && !bLuminene && !bPlacedNode)
		{
			continue;
		}
		FTaggedNode Entry;
		Entry.Actor = Actor;
		if (bJadeite)
		{
			Entry.Type = EKodResourceType::Jadeite;
		}
		else if (bLuminene)
		{
			Entry.Type = EKodResourceType::Luminene;
		}
		else if (const AKodResourceNode* Placed = Cast<AKodResourceNode>(Actor))
		{
			Entry.Type = Placed->ResourceType;
		}
		Entry.SortKey = FString::Printf(TEXT("%d_%s"), bJadeite ? 0 : 1, *Actor->GetName());
		Tagged.Add(Entry);
	}
	Tagged.Sort([](const FTaggedNode& A, const FTaggedNode& B)
	{
		return A.SortKey < B.SortKey;
	});

	auto RegisterNode = [&](AActor* Actor, EKodResourceType Type, const UKodResourceNodeDefinition* Def) -> bool
	{
		if (!Actor)
		{
			return false;
		}
		const bool bLuminene = Type == EKodResourceType::Luminene;
		FKodResourceNodeSpec Spec;
		Spec.Type = Type;
		Spec.Amount = Def ? Def->Amount : (bLuminene ? KodEconomyDefaults::LumineneNodeAmount : KodEconomyDefaults::JadeiteNodeAmount);
		Spec.HarvestPerTrip = Def ? Def->HarvestPerTrip : (bLuminene ? KodEconomyDefaults::LumineneHarvestPerTrip : KodEconomyDefaults::JadeiteHarvestPerTrip);
		Spec.HarvestTicks = (Def && Def->HarvestTicks > 0)
			? Def->HarvestTicks
			: (bLuminene ? KodEconomyDefaults::LumineneHarvestTicks : KodEconomyDefaults::JadeiteHarvestTicks);
		Spec.TricklePerTrip = Def ? Def->TricklePerTrip : (bLuminene ? KodEconomyDefaults::LumineneTricklePerTrip : 0);
		Spec.GatherStandUU = (Def && Def->GatherStandUU > 0)
			? Def->GatherStandUU
			: (bLuminene ? KodEconomyDefaults::LumineneGatherStandUU : KodEconomyDefaults::JadeiteGatherStandUU);
		Spec.DefinitionId = (Def && !Def->DefinitionId.IsNone())
			? Def->DefinitionId
			: FName(bLuminene ? KodWardenPaths::Id_LumineneVent : KodWardenPaths::Id_JadeiteNode);

		if (AKodResourceNode* Node = Cast<AKodResourceNode>(Actor))
		{
			if (Def)
			{
				Node->ApplyDefinition(Def);
			}
			else
			{
				Node->ApplyPlaceholder(Type);
			}
		}
		EnableResourceSelectCollision(Actor);
		const FKodEntityId Id = Sim->RegisterResourceNode(Actor, Spec);
		if (!Id.IsValid())
		{
			return false;
		}
		if (AKodResourceNode* Node = Cast<AKodResourceNode>(Actor))
		{
			Node->SetEntityId(Id);
			Node->ApplyJadeiteStandIn();
			Node->SetActorHiddenInGame(false);
		}
		const FVector SpawnedAt = Actor->GetActorLocation();
		UE_LOG(LogTemp, Log, TEXT("KodEcon Node Spawn Id=%d Type=%s Loc=%.0f,%.0f,%.0f"),
			Id.Value,
			KodResourceTypeName(Type),
			SpawnedAt.X,
			SpawnedAt.Y,
			SpawnedAt.Z);
		return true;
	};

	if (Tagged.Num() > 0)
	{
		int32 JadeiteCount = 0;
		int32 LumineneCount = 0;
		for (const FTaggedNode& Entry : Tagged)
		{
			const UKodResourceNodeDefinition* Def = Entry.Type == EKodResourceType::Luminene ? LumineneDef : JadeiteDef;
			if (!RegisterNode(Entry.Actor, Entry.Type, Def))
			{
				continue;
			}
			if (Entry.Type == EKodResourceType::Luminene)
			{
				++LumineneCount;
			}
			else
			{
				++JadeiteCount;
			}
		}
		RegisterCommandCenterDropOff();
		UE_LOG(LogTemp, Log, TEXT("KodEcon Nodes Jadeite=%d Luminene=%d Source=Tagged"), JadeiteCount, LumineneCount);
		return;
	}

	AActor* AnchorActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsSlice0CommandCenter(Actor))
		{
			continue;
		}
		if (!AnchorActor || Actor->GetName() < AnchorActor->GetName())
		{
			AnchorActor = Actor;
		}
	}
	if (!AnchorActor)
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			AnchorActor = *It;
			break;
		}
	}

	const FVector AnchorLocation = AnchorActor ? AnchorActor->GetActorLocation() : SmokeRangerOffset;

	TArray<FVector> UnitPoints;
	{
		TArray<FKodSimEntityState> States;
		TArray<AActor*> Actors;
		Sim->CopyEntitySnapshot(States, Actors);
		for (const FKodSimEntityState& State : States)
		{
			UnitPoints.Add(State.Position);
		}
	}

	FVector MarineMid = AnchorLocation;
	if (UnitPoints.Num() > 0)
	{
		MarineMid = FVector::ZeroVector;
		for (const FVector& Point : UnitPoints)
		{
			MarineMid += Point;
		}
		MarineMid /= static_cast<float>(UnitPoints.Num());
	}

	// Past the CC, on the side opposite the Marines, so the line reads as the base minerals.
	FVector FieldDir = AnchorLocation - MarineMid;
	FieldDir.Z = 0.f;
	if (FieldDir.SizeSquared2D() < FMath::Square(200.f))
	{
		FieldDir = FVector(1.f, 0.f, 0.f);
		if (UnitPoints.Num() >= 2)
		{
			FVector Sep = UnitPoints.Last() - UnitPoints[0];
			Sep.Z = 0.f;
			if (Sep.SizeSquared2D() > 1.f)
			{
				FieldDir = FVector(-Sep.Y, Sep.X, 0.f);
			}
		}
	}
	FieldDir = FieldDir.GetSafeNormal2D();
	if (FieldDir.IsNearlyZero())
	{
		FieldDir = FVector(1.f, 0.f, 0.f);
	}
	if (FMath::Abs(AnchorLocation.X) + FMath::Abs(AnchorLocation.Y) > 4000.f)
	{
		// In front of a corner command center, toward the map, not further into the corner.
		FieldDir = FVector(AnchorLocation.X >= 0.f ? -1.f : 1.f, AnchorLocation.Y >= 0.f ? -1.f : 1.f, 0.f).GetSafeNormal2D();
	}

	const float FacingDeg = FMath::RadiansToDegrees(FMath::Atan2(FieldDir.Y, FieldDir.X));
	UE_LOG(LogTemp, Log, TEXT("KodEcon Field Anchor=%s Loc=%.0f,%.0f,%.0f Facing=%.0f"),
		AnchorActor ? *AnchorActor->GetActorNameOrLabel() : TEXT("fallback"),
		AnchorLocation.X,
		AnchorLocation.Y,
		AnchorLocation.Z,
		FacingDeg);

	PlaceFieldAt(AnchorLocation, FieldDir);
	RegisterCommandCenterDropOff();
}

static void ApplyPrototypeStructureMesh(AActor* Actor, const UKodBuildingDefinition* Def)
{
	(void)Def;
	AKodSlice0GameMode::ApplyCommandCenterMesh(Actor);
}

bool AKodSlice0GameMode::PlaceActorAt(AActor* Actor, const FVector& Dest)
{
	if (!Actor)
	{
		return false;
	}
	if (USceneComponent* Root = Actor->GetRootComponent())
	{
		Root->SetMobility(EComponentMobility::Movable);
	}
	else
	{
		USceneComponent* NewRoot = NewObject<USceneComponent>(Actor, TEXT("KodMoveRoot"));
		Actor->SetRootComponent(NewRoot);
		NewRoot->RegisterComponent();
	}
	TInlineComponentArray<UPrimitiveComponent*> Primitives;
	Actor->GetComponents(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive)
		{
			Primitive->SetMobility(EComponentMobility::Movable);
		}
	}
	Actor->SetActorLocation(Dest, false, nullptr, ETeleportType::TeleportPhysics);
	return FVector::Dist2D(Actor->GetActorLocation(), Dest) <= 50.f;
}

static void LiftCommandCenterOntoGround(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	UWorld* World = Actor->GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Loc = Actor->GetActorLocation();
	FCollisionQueryParams Params(TEXT("KodCCGround"), false, Actor);
	FHitResult Hit;
	float GroundZ = Loc.Z;
	if (World->LineTraceSingleByChannel(Hit, Loc + FVector(0.f, 0.f, 6000.f), Loc - FVector(0.f, 0.f, 6000.f), ECC_WorldStatic, Params)
		&& Hit.bBlockingHit)
	{
		GroundZ = Hit.ImpactPoint.Z;
	}
	Actor->UpdateComponentTransforms();
	const FBox Bounds = Actor->GetComponentsBoundingBox(true);
	if (Bounds.IsValid && Bounds.Min.Z < GroundZ + 2.f)
	{
		Actor->AddActorWorldOffset(FVector(0.f, 0.f, GroundZ + 6.f - Bounds.Min.Z));
	}
	UE_LOG(LogTemp, Log, TEXT("KodCC Ground Actor=%s Loc=%.0f,%.0f,%.0f Ground=%.0f"),
		*Actor->GetActorNameOrLabel(),
		Actor->GetActorLocation().X,
		Actor->GetActorLocation().Y,
		Actor->GetActorLocation().Z,
		GroundZ);
}

void AKodSlice0GameMode::ApplyCommandCenterMesh(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	constexpr float Footprint = 5.f * static_cast<float>(KodBuildGrid::CellUU);
	Actor->SetActorHiddenInGame(false);
	if (AKodBuilding* Building = Cast<AKodBuilding>(Actor))
	{
		if (UKodBuildingDefinition* Def = Building->GetDefinition())
		{
			Building->ApplyDefinition(Def);
		}
		UMeshComponent* Shown = nullptr;
		if (Building->BuildingSkel && Building->BuildingSkel->GetSkeletalMeshAsset())
		{
			Building->BuildingSkel->SetHiddenInGame(false);
			Building->BuildingSkel->SetVisibility(true);
			Shown = Building->BuildingSkel;
		}
		if (Building->BuildingMesh)
		{
			const bool bCube = !Building->BuildingMesh->GetStaticMesh()
				|| Building->BuildingMesh->GetStaticMesh()->GetPathName().Contains(TEXT("/Engine/BasicShapes/"));
			if (!Shown || bCube)
			{
				FName MeshId = FName(KodWardenPaths::Id_CommandCenter);
				if (const UWorld* MeshWorld = Actor->GetWorld())
				{
					if (const UKodSimSubsystem* MeshSim = MeshWorld->GetSubsystem<UKodSimSubsystem>())
					{
						if (MeshSim->GetPlayerFaction() == 1)
						{
							MeshId = FName(TEXT("ScinderHQ"));
						}
					}
				}
				const FKodMeshChoice Choice = FKodMeshCatalog::Resolve(MeshId);
				if (Choice.StaticMesh)
				{
					Building->BuildingMesh->SetStaticMesh(Choice.StaticMesh);
					Building->BuildingMesh->SetRelativeScale3D(FVector(Choice.Scale));
					if (Choice.bFallback || Choice.StaticMesh->GetPathName().Contains(TEXT("/Engine/BasicShapes/")))
					{
						UE_LOG(LogTemp, Log, TEXT("KodMesh Fallback Id=%s Path=%s"), *MeshId.ToString(), *Choice.StaticMesh->GetPathName());
					}
				}
				Building->BuildingMesh->SetHiddenInGame(false);
				Building->BuildingMesh->SetVisibility(true);
				if (!Shown)
				{
					Shown = Building->BuildingMesh;
				}
			}
		}
		const FString MeshName = Shown ? Shown->GetName() : FString(TEXT("None"));
		UE_LOG(LogTemp, Log, TEXT("KodCC Mesh=%s Span=%.0f Actor=%s Hidden=%d"),
			*MeshName,
			Footprint,
			*Actor->GetActorNameOrLabel(),
			Actor->IsHidden() ? 1 : 0);
		LiftCommandCenterOntoGround(Actor);
		return;
	}
	FName MeshId = FName(KodWardenPaths::Id_CommandCenter);
	if (const UWorld* MeshWorld = Actor->GetWorld())
	{
		if (const UKodSimSubsystem* MeshSim = MeshWorld->GetSubsystem<UKodSimSubsystem>())
		{
			if (MeshSim->GetPlayerFaction() == 1)
			{
				MeshId = FName(TEXT("ScinderHQ"));
			}
		}
	}
	const FKodMeshChoice Choice = FKodMeshCatalog::Resolve(MeshId);
	UStaticMesh* Mesh = Choice.bLoaded ? Choice.StaticMesh : nullptr;
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, KodWardenPaths::ProtoHqMesh);
	}
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UE_LOG(LogTemp, Log, TEXT("KodMesh Placeholder Id=%s Path=/Engine/BasicShapes/Cube.Cube"), *MeshId.ToString());
	}
	if (!Mesh)
	{
		return;
	}
	const FVector Scale = Choice.bLoaded ? FVector(Choice.Scale) : FVector(Footprint / 100.f, Footprint / 100.f, Footprint * 0.45f / 100.f);
	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	Actor->GetComponents(Meshes);
	UStaticMeshComponent* Comp = nullptr;
	for (UStaticMeshComponent* Candidate : Meshes)
	{
		if (Candidate && !Candidate->ComponentHasTag(FName(TEXT("KodFx"))))
		{
			Comp = Candidate;
			break;
		}
	}
	if (!Comp)
	{
		Comp = NewObject<UStaticMeshComponent>(Actor, TEXT("KodCCMesh"));
		if (USceneComponent* Root = Actor->GetRootComponent())
		{
			Comp->SetupAttachment(Root);
		}
		else
		{
			Actor->SetRootComponent(Comp);
		}
		Comp->RegisterComponent();
	}
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(Mesh);
	Comp->SetHiddenInGame(false);
	Comp->SetVisibility(true);
	const float ShownYaw = KodBuildingYaw::OffsetDegrees();
	Comp->SetRelativeScale3D(Scale);
	Comp->SetRelativeRotation(FRotator(0.f, ShownYaw, 0.f));
	Comp->SetRelativeLocation(FVector(0.f, 0.f, Choice.Z));
	KodMeshAttach::Apply(Actor, Comp, MeshId, Choice.bLoaded && !Choice.bFallback);
	if (Choice.bFallback || !Choice.bLoaded || Mesh->GetPathName().Contains(TEXT("/Engine/BasicShapes/")))
	{
		UE_LOG(LogTemp, Log, TEXT("KodMesh Fallback Id=%s Path=%s"), *MeshId.ToString(), *Mesh->GetPathName());
	}
	UE_LOG(LogTemp, Log, TEXT("KodCC Mesh=%s Span=%.0f Actor=%s Hidden=%d"),
		*Mesh->GetName(),
		Footprint,
		*Actor->GetActorNameOrLabel(),
		Actor->IsHidden() ? 1 : 0);
	LiftCommandCenterOntoGround(Actor);
}

void AKodSlice0GameMode::ApplyStartFaction(int32 FactionIndex)
{
	Super::ApplyStartFaction(FactionIndex);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsSlice0CommandCenter(Actor))
		{
			continue;
		}
		const AKodBuilding* Building = Cast<AKodBuilding>(Actor);
		if (Building && Building->TeamId != 0)
		{
			continue;
		}
		ApplyCommandCenterMesh(Actor);
	}

	// Workers are seated before the faction pick, so a Scinder start still has AMP bodies.
	// Swap those to SCN_WORKER and stand the crew on a ring outside the headquarters.
	UKodSimSubsystem* WorkerSim = World->GetSubsystem<UKodSimSubsystem>();
	if (!WorkerSim)
	{
		return;
	}
	AActor* WorkerAnchor = FindPlayerBaseActor(World);
	if (!WorkerAnchor)
	{
		return;
	}
	TArray<AKodUnit*> Crew;
	for (TActorIterator<AKodUnit> It(World); It; ++It)
	{
		AKodUnit* Unit = *It;
		if (!Unit || Unit->TeamId != 0)
		{
			continue;
		}
		bool bWorker = Unit->ActorHasTag(FName(TEXT("KodUnitDozer")));
		if (!bWorker)
		{
			if (const UKodUnitDefinition* Def = Unit->GetDefinition())
			{
				const FName DefId = Def->DefinitionId.IsNone() ? Def->GetFName() : Def->DefinitionId;
				bWorker = Def->bCanGather || DefId == FName(KodWardenPaths::Id_Dozer) || DefId == FName(KodWardenPaths::Id_ScnWorker);
			}
		}
		if (bWorker && FVector::Dist2D(Unit->GetActorLocation(), WorkerAnchor->GetActorLocation()) < 2500.f)
		{
			Crew.Add(Unit);
		}
	}
	// TArray::Sort dereferences pointer elements. The predicate sees the unit, not the pointer.
	Crew.Sort([](const AKodUnit& A, const AKodUnit& B)
	{
		return A.EntityId.Value < B.EntityId.Value;
	});
	if (Crew.Num() == 0)
	{
		return;
	}
	const FName WorkerId = FactionIndex == 1 ? FName(KodWardenPaths::Id_ScnWorker) : FName(KodWardenPaths::Id_Dozer);
	UKodUnitDefinition* WorkerDef = UKodSlice0Bootstrap::ResolveUnit(WorkerId, this);
	const FKodMeshChoice Body = FKodMeshCatalog::Resolve(WorkerId);
	const float BodySpan = Body.BoundsCm > 10.f ? Body.BoundsCm : 80.f;
	FVector SocketAt = WorkerAnchor->GetActorLocation();
	const bool bSocket = KodMeshSockets::TryGet(WorkerAnchor, KodMeshSockets::WorkerExit, SocketAt)
		|| KodMeshSockets::TryGet(WorkerAnchor, KodMeshSockets::Door, SocketAt);
	FVector FieldDir = FVector(1.f, 0.f, 0.f);
	{
		FVector Sum = FVector::ZeroVector;
		int32 NodeCount = 0;
		for (TActorIterator<AKodResourceNode> It(World); It; ++It)
		{
			if (AKodResourceNode* Node = *It)
			{
				Sum += Node->GetActorLocation();
				++NodeCount;
			}
		}
		if (NodeCount > 0)
		{
			FVector Dir = (Sum / static_cast<float>(NodeCount)) - WorkerAnchor->GetActorLocation();
			Dir.Z = 0.f;
			if (Dir.SizeSquared2D() > 1.f)
			{
				FieldDir = Dir.GetSafeNormal2D();
			}
		}
	}
	const FKodWorkerRing Ring = MakeWorkerRing(
		WorkerAnchor->GetActorLocation(),
		FieldDir,
		AnchorFootprintCm(WorkerSim, WorkerAnchor),
		BodySpan,
		Crew.Num(),
		bSocket,
		SocketAt);
	TArray<AActor*> Ignored;
	for (int32 Index = 0; Index < Crew.Num(); ++Index)
	{
		AKodUnit* Unit = Crew[Index];
		if (!Unit)
		{
			continue;
		}
		if (WorkerDef)
		{
			const UKodUnitDefinition* Current = Unit->GetDefinition();
			const FName CurrentId = Current ? (Current->DefinitionId.IsNone() ? Current->GetFName() : Current->DefinitionId) : FName();
			if (CurrentId != WorkerId)
			{
				Unit->Definition = WorkerDef;
				Unit->ApplyDefinition(WorkerDef);
			}
		}
		const float Angle = (static_cast<float>(Index) - (static_cast<float>(Crew.Num()) - 1.f) * 0.5f) * Ring.StepDegrees;
		const FVector Dir = Ring.Forward.RotateAngleAxis(Angle, FVector::UpVector);
		const FVector Spot = Ring.Center + Dir * Ring.Radius;
		const float FloorZ = ResolveResourceFloorZ(World, WorkerSim, Spot.X, Spot.Y, Ring.Center.Z, Ignored);
		Unit->SetActorLocation(FVector(Spot.X, Spot.Y, FloorZ), false, nullptr, ETeleportType::TeleportPhysics);
		Unit->SnapToGroundPublic();
		if (Unit->EntityId.IsValid())
		{
			WorkerSim->SetEntityGroundPosition(Unit->EntityId, Unit->GetActorLocation());
		}
		Ignored.Add(Unit);
	}
	UE_LOG(LogTemp, Log, TEXT("KodFaction Workers Id=%s Count=%d Radius=%.0f"),
		*WorkerId.ToString(),
		Crew.Num(),
		Ring.Radius);
}

bool AKodSlice0GameMode::PlaceShotBuildings(FVector& OutFocus)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		OutFocus = FVector::ZeroVector;
		return false;
	}
	AActor* BaseActor = FindPlayerBaseActor(World);
	FVector Base = BaseActor ? BaseActor->GetActorLocation() : FVector(-7500.f, -7500.f, 80.f);
	if (BaseActor && FMath::Abs(Base.X) < 50.f && FMath::Abs(Base.Y) < 50.f)
	{
		const FVector Start( -7500.f, -7500.f, Base.Z > 1.f ? Base.Z : 80.f);
		PlaceActorAt(BaseActor, Start);
		Base = BaseActor->GetActorLocation();
		if (FKodEntityId BaseId = Sim->FindIdForActor(BaseActor); BaseId.IsValid())
		{
			Sim->SetEntityGroundPosition(BaseId, Base);
		}
	}
	ApplyCommandCenterMesh(BaseActor);
	const int32 Faction = Sim->GetPlayerFaction();
	const bool bScinder = Faction == 1;
	TArray<FVector> FocusPoints;
	FocusPoints.Add(Base);

	auto PlaceBuilding = [this, World, Sim, &FocusPoints](FName TypeId, const FVector& At, int32 FootX, int32 FootY)
	{
		UKodBuildingDefinition* Def = UKodSlice0Bootstrap::ResolveBuilding(TypeId, World);
		if (!Def || Def->DefinitionId != TypeId)
		{
			UKodBuildingDefinition* Fresh = NewObject<UKodBuildingDefinition>(GetTransientPackage());
			Fresh->DefinitionId = TypeId;
			Fresh->FootprintX = FootX;
			Fresh->FootprintY = FootY;
			Fresh->MaxHealth = Def ? Def->MaxHealth : 1500.f;
			Def = Fresh;
		}
		const FTransform Xform(FRotator::ZeroRotator, At);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AKodBuilding* Building = World->SpawnActor<AKodBuilding>(AKodBuilding::StaticClass(), Xform, Params);
		if (!Building)
		{
			return;
		}
		Building->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
		Building->TeamId = 0;
		Building->Definition = Def;
		Building->Tags.AddUnique(FName(*FString::Printf(TEXT("KodShotName_%s"), *TypeId.ToString())));
		Building->Tags.AddUnique(FName(TEXT("KodShotLabel")));
		Building->ApplyDefinition(Def);
		PlaceActorAt(Building, At);
		FKodStructureSpec Spec;
		Spec.DefinitionId = TypeId;
		Spec.DisplayId = TypeId;
		Spec.TeamId = 0;
		Spec.FootprintX = FootX;
		Spec.FootprintY = FootY;
		Spec.MaxHealth = Def->MaxHealth;
		const FKodEntityId Id = Sim->RegisterStructure(Building, Spec);
		Building->EntityId = Id;
		PlaceActorAt(Building, At);
		if (Id.IsValid())
		{
			Sim->SetEntityGroundPosition(Id, Building->GetActorLocation());
			Sim->SetStructureBuildFraction(Id, 0.10f);
		}
		const FVector PlacedAt = Building->GetActorLocation();
		FocusPoints.Add(PlacedAt);
		UE_LOG(LogTemp, Log, TEXT("KodShotBuildings Place Id=%s Loc=%.0f,%.0f,%.0f"),
			*TypeId.ToString(),
			PlacedAt.X,
			PlacedAt.Y,
			PlacedAt.Z);
	};
	auto PlaceUnit = [World, &FocusPoints](FName TypeId, const FVector& At)
	{
		UKodUnitDefinition* Def = UKodSlice0Bootstrap::ResolveUnit(TypeId, World);
		if (!Def)
		{
			Def = NewObject<UKodUnitDefinition>(GetTransientPackage());
			Def->DefinitionId = TypeId;
			Def->MaxHealth = 80.f;
			Def->MoveSpeed = 400.f;
			Def->SightRadius = 1200.f;
		}
		const FTransform Xform(FRotator::ZeroRotator, At);
		AKodUnit* Unit = World->SpawnActor<AKodUnit>(AKodUnit::StaticClass(), Xform);
		if (!Unit)
		{
			return;
		}
		Unit->TeamId = 0;
		Unit->Definition = Def;
		Unit->Tags.AddUnique(FName(*FString::Printf(TEXT("KodShotName_%s"), *TypeId.ToString())));
		Unit->Tags.AddUnique(FName(TEXT("KodShotLabel")));
		Unit->ApplyDefinition(Def);
		FocusPoints.Add(Unit->GetActorLocation());
		UE_LOG(LogTemp, Log, TEXT("KodShotBuildings Place Id=%s Loc=%.0f,%.0f,%.0f"),
			*TypeId.ToString(),
			At.X,
			At.Y,
			At.Z);
	};

	struct FShotPlace
	{
		FName Id;
		bool bBuilding = false;
		float SpanCm = 200.f;
	};
	TArray<FShotPlace> Plan;
	auto AddBuilding = [&Plan](FName Id, int32 Cells)
	{
		FShotPlace Place;
		Place.Id = Id;
		Place.bBuilding = true;
		const float Grid = static_cast<float>(FMath::Max(1, Cells)) * static_cast<float>(KodBuildGrid::CellUU);
		const float Fit = FKodMeshCatalog::FootprintCm(Id);
		const FKodMeshChoice Choice = FKodMeshCatalog::Resolve(Id);
		Place.SpanCm = Fit > 0.f ? Fit : (Choice.BoundsCm > 10.f ? Choice.BoundsCm : Grid);
		Plan.Add(Place);
	};
	auto AddUnit = [&Plan](FName Id)
	{
		FShotPlace Place;
		Place.Id = Id;
		Place.bBuilding = false;
		const FKodMeshChoice Choice = FKodMeshCatalog::Resolve(Id);
		Place.SpanCm = Choice.BoundsCm > 10.f ? Choice.BoundsCm : 200.f;
		Plan.Add(Place);
	};
	if (bScinder)
	{
		AddUnit(FName(TEXT("ScinderHost")));
		AddBuilding(FName(TEXT("RiteWell")), 2);
		AddUnit(FName(KodWardenPaths::Id_ScnWorker));
		AddUnit(FName(TEXT("Faultburst")));
		AddUnit(FName(TEXT("Shardbitten")));
	}
	else
	{
		AddBuilding(FName(KodWardenPaths::Id_Barracks), 3);
		AddBuilding(FName(KodWardenPaths::Id_SupplyDepot), 2);
		AddUnit(FName(KodWardenPaths::Id_Marine));
	}
	const FName HallId = bScinder ? FName(TEXT("ScinderHQ")) : FName(KodWardenPaths::Id_CommandCenter);
	float Edge = FKodMeshCatalog::FootprintCm(HallId) * 0.5f;
	if (Edge < 1.f)
	{
		Edge = 500.f;
	}
	const float HallHalf = Edge;
	FBox RowBox(ForceInit);
	for (const FShotPlace& Place : Plan)
	{
		const float Half = Place.SpanCm * 0.5f;
		const float Gap = Place.SpanCm + 200.f;
		const float Center = Edge + Gap + Half;
		// +Y keeps the row on open sand. +X runs into the ridge beside the base.
		const FVector At = Base + FVector(0.f, Center + 1400.f, 0.f);
		const int32 Cells = FMath::Max(1, FMath::RoundToInt(Place.SpanCm / static_cast<float>(KodBuildGrid::CellUU)));
		if (Place.bBuilding)
		{
			PlaceBuilding(Place.Id, At, Cells, Cells);
		}
		else
		{
			PlaceUnit(Place.Id, At);
		}
		RowBox += At + FVector(Half, Half, 0.f);
		RowBox += At - FVector(Half, Half, 0.f);
		Edge = Center + Half;
	}

	OutFocus = RowBox.IsValid ? RowBox.GetCenter() : Base;
	OutFocus.Z = Base.Z;
	const FVector RowSize = RowBox.IsValid ? RowBox.GetSize() : FVector(HallHalf * 2.f, HallHalf * 2.f, 0.f);
	for (TActorIterator<AKodRTSCameraPawn> It(World); It; ++It)
	{
		if (It->IsA(AKodSpectatorPawn::StaticClass()))
		{
			continue;
		}
		It->FrameGroundSpan(OutFocus, RowSize.X, RowSize.Y);
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("KodShotBuildings Faction=%d Focus=%.0f,%.0f,%.0f"),
		Faction,
		OutFocus.X,
		OutFocus.Y,
		OutFocus.Z);
	return true;
}

bool AKodSlice0GameMode::PlaceShotRiteWell(FVector& OutFocus, int32& OutStructureId)
{
	OutFocus = FVector::ZeroVector;
	OutStructureId = 0;
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return false;
	}
	AActor* BaseActor = FindPlayerBaseActor(World);
	FVector Base = BaseActor ? BaseActor->GetActorLocation() : FVector(-7500.f, -7500.f, 80.f);
	if (BaseActor && FMath::Abs(Base.X) < 50.f && FMath::Abs(Base.Y) < 50.f)
	{
		const FVector Start(-7500.f, -7500.f, Base.Z > 1.f ? Base.Z : 80.f);
		PlaceActorAt(BaseActor, Start);
		Base = BaseActor->GetActorLocation();
		if (FKodEntityId BaseId = Sim->FindIdForActor(BaseActor); BaseId.IsValid())
		{
			Sim->SetEntityGroundPosition(BaseId, Base);
		}
	}
	const FVector At = Base + FVector(1200.f, 0.f, 0.f);
	UKodBuildingDefinition* Def = UKodSlice0Bootstrap::ResolveBuilding(FName(TEXT("RiteWell")), World);
	if (!Def || Def->DefinitionId != FName(TEXT("RiteWell")))
	{
		UKodBuildingDefinition* Fresh = NewObject<UKodBuildingDefinition>(GetTransientPackage());
		Fresh->DefinitionId = FName(TEXT("RiteWell"));
		Fresh->FootprintX = 3;
		Fresh->FootprintY = 3;
		Fresh->MaxHealth = 1000.f;
		Def = Fresh;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKodBuilding* Building = World->SpawnActor<AKodBuilding>(AKodBuilding::StaticClass(), FTransform(FRotator::ZeroRotator, At), Params);
	if (!Building)
	{
		return false;
	}
	Building->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
	Building->TeamId = 0;
	Building->Definition = Def;
	Building->Tags.AddUnique(FName(TEXT("KodShotName_RiteWell")));
	Building->Tags.AddUnique(FName(TEXT("KodShotLabel")));
	Building->ApplyDefinition(Def);
	PlaceActorAt(Building, At);
	FKodStructureSpec Spec;
	Spec.DefinitionId = FName(TEXT("RiteWell"));
	Spec.DisplayId = FName(TEXT("RiteWell"));
	Spec.TeamId = 0;
	Spec.FootprintX = FMath::Max(1, Def->FootprintX);
	Spec.FootprintY = FMath::Max(1, Def->FootprintY);
	Spec.MaxHealth = Def->MaxHealth;
	const FKodEntityId Id = Sim->RegisterStructure(Building, Spec);
	Building->EntityId = Id;
	PlaceActorAt(Building, At);
	if (Id.IsValid())
	{
		Sim->SetEntityGroundPosition(Id, Building->GetActorLocation());
		Sim->SetStructureBuildFraction(Id, 0.f);
	}
	OutStructureId = Id.Value;
	OutFocus = (Base + Building->GetActorLocation()) * 0.5f;
	OutFocus.Z = Base.Z;
	for (TActorIterator<AKodRTSCameraPawn> It(World); It; ++It)
	{
		if (It->IsA(AKodSpectatorPawn::StaticClass()))
		{
			continue;
		}
		It->JumpTo(OutFocus);
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("KodShotRiteWell Place Loc=%.0f,%.0f,%.0f Focus=%.0f,%.0f"),
		Building->GetActorLocation().X,
		Building->GetActorLocation().Y,
		Building->GetActorLocation().Z,
		OutFocus.X,
		OutFocus.Y);
	return Id.IsValid();
}

bool AKodSlice0GameMode::PlaceShotBunker(FVector& OutFocus)
{
	OutFocus = FVector::ZeroVector;
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return false;
	}
	AActor* BaseActor = FindPlayerBaseActor(World);
	FVector Base = BaseActor ? BaseActor->GetActorLocation() : FVector(-7500.f, -7500.f, 80.f);
	if (FMath::Abs(Base.X) < 50.f && FMath::Abs(Base.Y) < 50.f)
	{
		Base = FVector(-7500.f, -7500.f, Base.Z > 1.f ? Base.Z : 80.f);
	}
	const FVector At = Base + FVector(1400.f, 400.f, 0.f);
	const FName BunkerId(KodWardenPaths::Id_Bunker);
	UKodBuildingDefinition* Def = UKodSlice0Bootstrap::ResolveBuilding(BunkerId, World);
	if (!Def || Def->DefinitionId != BunkerId)
	{
		UKodBuildingDefinition* Fresh = NewObject<UKodBuildingDefinition>(GetTransientPackage());
		Fresh->DefinitionId = BunkerId;
		Fresh->FootprintX = 3;
		Fresh->FootprintY = 3;
		Fresh->MaxHealth = 400.f;
		Def = Fresh;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKodBuilding* Building = World->SpawnActor<AKodBuilding>(AKodBuilding::StaticClass(), FTransform(FRotator::ZeroRotator, At), Params);
	if (!Building)
	{
		return false;
	}
	Building->TeamId = 0;
	Building->Definition = Def;
	Building->Tags.AddUnique(FName(TEXT("KodShotName_VG_BUNKER")));
	Building->Tags.AddUnique(FName(TEXT("KodShotLabel")));
	Building->ApplyDefinition(Def);
	PlaceActorAt(Building, At);
	FKodStructureSpec Spec;
	Spec.DefinitionId = BunkerId;
	Spec.DisplayId = FName(TEXT("Bunker"));
	Spec.TeamId = 0;
	Spec.FootprintX = 3;
	Spec.FootprintY = 3;
	Spec.MaxHealth = Def->MaxHealth;
	const FKodEntityId Id = Sim->RegisterStructure(Building, Spec);
	Building->EntityId = Id;
	PlaceActorAt(Building, At);
	if (Id.IsValid())
	{
		Sim->SetEntityGroundPosition(Id, Building->GetActorLocation());
		Sim->SetStructureBuildFraction(Id, 0.10f);
	}
	OutFocus = Building->GetActorLocation();
	for (TActorIterator<AKodRTSCameraPawn> It(World); It; ++It)
	{
		if (!It->IsA(AKodSpectatorPawn::StaticClass()))
		{
			It->JumpTo(OutFocus);
			break;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("KodShotBunker Place Loc=%.0f,%.0f,%.0f"), OutFocus.X, OutFocus.Y, OutFocus.Z);
	return Id.IsValid() && FVector::Dist2D(OutFocus, FVector::ZeroVector) > 50.f;
}

bool AKodSlice0GameMode::PlaceShotShadowSquads(FVector& OutFocus)
{
	OutFocus = FVector::ZeroVector;
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const FVector Base = FindPlayerBaseActor(World) ? FindPlayerBaseActor(World)->GetActorLocation() : FVector(-7500.f, -7500.f, 80.f);
	AActor* Wall = nullptr;
	float BestSpan = 0.f;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* Actor = *It;
		if (!Actor || Actor->ActorHasTag(FName(TEXT("KodGround"))) || Actor->ActorHasTag(FName(TEXT("KodGeneratedGround")))
			|| Actor->ActorHasTag(FName(TEXT("KodBaseSocket"))))
		{
			continue;
		}
		const FBox Bounds = Actor->GetComponentsBoundingBox(true);
		if (!Bounds.IsValid)
		{
			continue;
		}
		const FVector Size = Bounds.GetSize();
		const float Span = FMath::Max(Size.X, Size.Y);
		if (Span < 1200.f || Size.Z < 200.f)
		{
			continue;
		}
		if (FVector::Dist2D(Bounds.GetCenter(), Base) > 12000.f)
		{
			continue;
		}
		if (!Wall || Span > BestSpan)
		{
			Wall = Actor;
			BestSpan = Span;
		}
	}
	FVector SunForward(1.f, 0.f, 0.f);
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (*It && !It->IsHidden())
		{
			SunForward = It->GetActorRotation().Vector().GetSafeNormal2D();
			if (!SunForward.IsNearlyZero())
			{
				break;
			}
		}
	}
	if (SunForward.IsNearlyZero())
	{
		SunForward = FVector(1.f, 0.f, 0.f);
	}
	const FVector WallCenter = Wall ? Wall->GetActorLocation() : Base + FVector(2000.f, 0.f, 0.f);
	const FVector ShadowAt = WallCenter + SunForward * 700.f;
	const FVector LitAt = WallCenter - SunForward * 600.f;
	UKodUnitDefinition* Marine = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_Marine), this);
	TArray<FKodEntityId> Blue;
	auto SpawnMarine = [this, World, Marine, &Blue](const FVector& At, int32 Team, bool bSelect)
	{
		if (!Marine)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AKodUnit* Unit = World->SpawnActor<AKodUnit>(AKodUnit::StaticClass(), FTransform(FRotator::ZeroRotator, At), Params);
		if (!Unit)
		{
			return;
		}
		Unit->TeamId = Team;
		Unit->Definition = Marine;
		Unit->ApplyDefinition(Marine);
		PlaceActorAt(Unit, At);
		if (bSelect && Unit->EntityId.IsValid())
		{
			Blue.Add(Unit->EntityId);
		}
	};
	const FVector Across(-SunForward.Y, SunForward.X, 0.f);
	auto AtHeight = [Base](const FVector& Point) -> FVector
	{
		return FVector(Point.X, Point.Y, Base.Z);
	};
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FVector Step = Across * (static_cast<float>(Index) - 2.f) * 140.f;
		SpawnMarine(AtHeight(ShadowAt + Step), 0, false);
		SpawnMarine(AtHeight(ShadowAt + Step + Across * 160.f), 1, true);
	}
	SpawnMarine(AtHeight(LitAt), 0, false);
	SpawnMarine(AtHeight(LitAt + Across * 180.f), 1, false);
	OutFocus = ShadowAt;
	OutFocus.Z = Base.Z;
	for (TActorIterator<AKodRTSCameraPawn> It(World); It; ++It)
	{
		if (!It->IsA(AKodSpectatorPawn::StaticClass()))
		{
			It->JumpTo(OutFocus);
			break;
		}
	}
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (AKodPlayerController* KodPC = Cast<AKodPlayerController>(PC))
		{
			KodPC->SetLocalSelectionFromIds(Blue, false);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("KodShotShadowSquads Wall=%s Shadow=%.0f,%.0f Lit=%.0f,%.0f Blue=%d"),
		Wall ? *Wall->GetActorNameOrLabel() : TEXT("None"),
		ShadowAt.X,
		ShadowAt.Y,
		LitAt.X,
		LitAt.Y,
		Blue.Num());
	return Blue.Num() == 5;
}

void AKodSlice0GameMode::SetShotBuildFraction(float Fraction)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return;
	}
	for (TActorIterator<AKodBuilding> It(World); It; ++It)
	{
		AKodBuilding* Building = *It;
		if (!Building || !Building->ActorHasTag(FName(TEXT("KodShotLabel"))))
		{
			continue;
		}
		FKodEntityId Id = Building->EntityId;
		if (!Id.IsValid())
		{
			Id = Sim->FindIdForActor(Building);
		}
		if (Id.IsValid())
		{
			Sim->SetStructureBuildFraction(Id, Fraction);
		}
	}
}

void AKodSlice0GameMode::RegisterCommandCenterDropOff()
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodEcon DropOff Miss"));
		return;
	}

	AActor* Anchor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsSlice0CommandCenter(Actor))
		{
			continue;
		}
		if (!Anchor || Actor->GetName() < Anchor->GetName())
		{
			Anchor = Actor;
		}
	}
	if (!Anchor)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && Actor->ActorHasTag(FName(TEXT("KodBuildingCommandCenter"))))
			{
				Anchor = Actor;
				break;
			}
		}
	}
	if (!Anchor)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodEcon DropOff Miss"));
		return;
	}

	UKodBuildingDefinition* CCDef = UKodSlice0Bootstrap::ResolveBuilding(FName(KodWardenPaths::Id_CommandCenter), this);
	FKodStructureSpec Spec;
	Spec.DefinitionId = FName(KodWardenPaths::Id_CommandCenter);
	Spec.TeamId = 0;
	Spec.SupplyProvided = CCDef ? CCDef->SupplyProvided : KodEconomyDefaults::SupplyPerHq;
	Spec.bIsDropOff = true;
	Spec.DisplayId = FName(TEXT("CC"));
	Spec.FootprintX = CCDef ? FMath::Max(1, CCDef->FootprintX) : 5;
	Spec.FootprintY = CCDef ? FMath::Max(1, CCDef->FootprintY) : 5;
	Spec.MaxHealth = CCDef ? CCDef->MaxHealth : 2500.f;
	Spec.Trains.Add(FName(KodWardenPaths::Id_Dozer));
	Spec.DropOffStandUU = DropOffStandUU > 0 ? DropOffStandUU : KodEconomyDefaults::DropOffStandUU;
	ApplyPrototypeStructureMesh(Anchor, CCDef);
	const FKodEntityId Id = Sim->RegisterStructure(Anchor, Spec);
	if (!Id.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("KodEcon DropOff Miss Anchor=%s"), *Anchor->GetActorNameOrLabel());
		return;
	}
	EnableResourceSelectCollision(Anchor);
}

void AKodSlice0GameMode::SpawnWorkersIfNeeded()
{
	if (!bSpawnWorkers)
	{
		return;
	}
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodEcon Workers Spawned=0 Reason=NoSim"));
		return;
	}

	AActor* Anchor = FindPlayerBaseActor(World);

	const FVector AnchorLocation = Anchor ? Anchor->GetActorLocation() : SmokeRangerOffset;
	int32 Nearby = 0;
	for (TActorIterator<AKodUnit> It(World); It; ++It)
	{
		AKodUnit* Unit = *It;
		if (!Unit || Unit->TeamId != 0)
		{
			continue;
		}
		bool bWorker = Unit->ActorHasTag(FName(TEXT("KodUnitDozer")));
		if (!bWorker)
		{
			if (const UKodUnitDefinition* Def = Unit->GetDefinition())
			{
				bWorker = Def->bCanGather || Def->DefinitionId == FName(KodWardenPaths::Id_Dozer);
			}
		}
		if (bWorker && FVector::Dist2D(Unit->GetActorLocation(), AnchorLocation) < 1500.f)
		{
			++Nearby;
		}
	}
	const int32 Want = FMath::Clamp(WorkerSpawnCount, 1, 12);
	if (Nearby >= Want)
	{
		UE_LOG(LogTemp, Log, TEXT("KodEcon Workers Spawned=0 Reason=Placed Count=%d"), Nearby);
		return;
	}
	FVector FieldDir = FVector(1.f, 0.f, 0.f);
	{
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (TActorIterator<AKodResourceNode> It(World); It; ++It)
		{
			if (AKodResourceNode* Node = *It)
			{
				Sum += Node->GetActorLocation();
				++Count;
			}
		}
		if (Count > 0)
		{
			const FVector Mid = Sum / static_cast<float>(Count);
			FVector Dir = Mid - AnchorLocation;
			Dir.Z = 0.f;
			if (Dir.SizeSquared2D() > 1.f)
			{
				FieldDir = Dir.GetSafeNormal2D();
			}
		}
	}

	const int32 Spawned = SpawnWorkerFan(AnchorLocation, FieldDir, /*TeamId*/ 0, Want - Nearby, Anchor);

	UE_LOG(LogTemp, Log, TEXT("KodEcon Workers Spawned=%d Anchor=%s"),
		Spawned,
		Anchor ? *Anchor->GetActorNameOrLabel() : TEXT("fallback"));
}

void AKodSlice0GameMode::SpawnAiOpponent()
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim || !Sim->IsAiEnabled())
	{
		return;
	}

	int32 CmdAi = 1;
	if (FParse::Value(FCommandLine::Get(), TEXT("kod.AI="), CmdAi) && CmdAi == 0)
	{
		Sim->SetAiEnabled(false);
		UE_LOG(LogTemp, Log, TEXT("KodAI Enabled=0"));
		return;
	}
	int32 CmdDiff = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("kod.AIDifficulty="), CmdDiff))
	{
		Sim->SetAiDifficulty(CmdDiff);
	}

	AActor* PlayerCc = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsSlice0CommandCenter(Actor))
		{
			continue;
		}
		if (!PlayerCc || Actor->GetName() < PlayerCc->GetName())
		{
			PlayerCc = Actor;
		}
	}
	const FVector PlayerLoc = PlayerCc ? PlayerCc->GetActorLocation() : SmokeRangerOffset;
	Sim->SetEnemyStart(PlayerLoc);

	FVector Seed = FVector::ZeroVector;
	const TCHAR* How = TEXT("Corner");
	bool bHave = false;
	FString StartArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("KodAIStart="), StartArg))
	{
		TArray<FString> Parts;
		StartArg.ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.Num() >= 2)
		{
			Seed.X = FCString::Atof(*Parts[0]);
			Seed.Y = FCString::Atof(*Parts[1]);
			Seed.Z = Parts.Num() >= 3 ? FCString::Atof(*Parts[2]) : PlayerLoc.Z;
			bHave = true;
			How = TEXT("Arg");
		}
	}
	if (!bHave)
	{
		AActor* Tagged = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor || !Actor->ActorHasTag(FName(TEXT("KodAIStart"))))
			{
				continue;
			}
			if (!Tagged || Actor->GetName() < Tagged->GetName())
			{
				Tagged = Actor;
			}
		}
		if (Tagged)
		{
			Seed = Tagged->GetActorLocation();
			bHave = true;
			How = TEXT("Tag");
		}
	}
	if (!bHave)
	{
		Seed = FVector(8300.f, 8300.f, PlayerLoc.Z);
		bHave = true;
		How = TEXT("Script");
	}

	TArray<AActor*> Ignored;
	const float FloorZ = ResolveResourceFloorZ(World, Sim, Seed.X, Seed.Y, Seed.Z, Ignored);
	Seed.Z = FloorZ;

	UKodBuildingDefinition* CCDef = UKodSlice0Bootstrap::ResolveBuilding(FName(KodWardenPaths::Id_CommandCenter), this);
	const FTransform Xform(FRotator::ZeroRotator, Seed);
	AKodBuilding* Building = World->SpawnActorDeferred<AKodBuilding>(
		AKodBuilding::StaticClass(),
		Xform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Building)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodAI Seed Miss"));
		return;
	}
	Building->SetActorLocation(Seed);
	Building->TeamId = 1;
	if (CCDef)
	{
		Building->Definition = CCDef;
	}

	FKodStructureSpec Spec;
	Spec.DefinitionId = FName(KodWardenPaths::Id_CommandCenter);
	Spec.TeamId = 1;
	Spec.SupplyProvided = CCDef ? CCDef->SupplyProvided : KodEconomyDefaults::SupplyPerHq;
	Spec.bIsDropOff = true;
	Spec.DisplayId = FName(TEXT("CC"));
	Spec.FootprintX = CCDef ? FMath::Max(1, CCDef->FootprintX) : 5;
	Spec.FootprintY = CCDef ? FMath::Max(1, CCDef->FootprintY) : 5;
	Spec.MaxHealth = CCDef ? CCDef->MaxHealth : 2500.f;
	Spec.Trains.Add(FName(KodWardenPaths::Id_Dozer));
	Spec.DropOffStandUU = DropOffStandUU > 0 ? DropOffStandUU : KodEconomyDefaults::DropOffStandUU;
	const FKodEntityId Id = Sim->RegisterStructure(Building, Spec);
	if (!Id.IsValid())
	{
		Building->Destroy();
		UE_LOG(LogTemp, Warning, TEXT("KodAI Seed Miss"));
		return;
	}
	Building->EntityId = Id;
	Building->FinishSpawning(Xform);

	const int32 Difficulty = FMath::Max(1, Sim->GetAiDifficulty());
	const int32 Credits = 50 + (Difficulty - 1) * 100;
	Sim->SetTeamBank(1, Credits, 0);

	FVector FieldDir = Seed - PlayerLoc;
	FieldDir.Z = 0.f;
	if (FieldDir.SizeSquared2D() < 1.f)
	{
		FieldDir = FVector(1.f, 0.f, 0.f);
	}
	else
	{
		FieldDir = FieldDir.GetSafeNormal2D();
	}
	PlaceFieldAt(Seed, FieldDir);
	const int32 Spawned = SpawnWorkerFan(Seed, FieldDir, /*TeamId*/ 1, FMath::Clamp(WorkerSpawnCount, 1, 12), Building);

	auto SnapYaw = [](const FVector& From, const FVector& To)
	{
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X));
		int32 Snapped = FMath::RoundToInt(Yaw / 90.f) * 90;
		while (Snapped < 0)
		{
			Snapped += 360;
		}
		return Snapped % 360;
	};
	Sim->SetTeamFacing(0, SnapYaw(PlayerLoc, Seed));
	Sim->SetTeamFacing(1, SnapYaw(Seed, PlayerLoc));
	Sim->SetAiSeeded(true);
	UE_LOG(LogTemp, Log, TEXT("KodAI Seed How=%s Loc=%.0f,%.0f,%.0f Workers=%d Credits=%d"),
		How,
		Seed.X,
		Seed.Y,
		Seed.Z,
		Spawned,
		Credits);
}
