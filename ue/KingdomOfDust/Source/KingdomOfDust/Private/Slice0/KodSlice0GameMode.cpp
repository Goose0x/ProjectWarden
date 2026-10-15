#include "Slice0/KodSlice0GameMode.h"
#include "Game/KodCollision.h"
#include "Game/KodMeshSockets.h"
#include "Slice0/KodFaultline.h"
#include "Slice0/KodMapLayout.h"
#include "Slice0/KodMapLife.h"
#include "Slice0/KodSlice0PlayerController.h"
#include "Game/KodHUD.h"
#include "Game/KodSkirmishSetup.h"
#include "Engine/Light.h"
#include "Game/KodUiArt.h"
#include "Game/KodRTSCameraPawn.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "Actors/KodUnit.h"
#include "Actors/KodBuilding.h"
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
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
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
	ADirectionalLight* KeptSun = Suns.Num() > 0 ? Suns[0] : nullptr;
	bool bSpawned = false;
	if (!KeptSun)
	{
		const FRotator SpawnRotation(-37.f, 150.f, 0.f);
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
			// Light travels toward screen top-left (about 10 o'clock) so shadows fall that way.
			// A small source angle keeps the shadow edge sharp. Low sky fill keeps the ground dark.
			KeptSun->SetActorRotation(FRotator(-37.f, 150.f, 0.f));
			const float Authored = KeptComponent->Intensity;
			const float FinalIntensity = FMath::Clamp(FMath::Max(Authored, 6.5f), 6.5f, 10.f);
			KeptComponent->SetIntensity(FinalIntensity);
			KeptComponent->SetLightColor(FLinearColor::FromSRGBColor(FColor(0xC8, 0xBE, 0xA8)));
			KeptComponent->SetTemperature(5200.f);
			KeptComponent->SetUseTemperature(true);
			KeptComponent->SetLightSourceAngle(0.35f);
			KeptComponent->SetDynamicShadowDistanceMovableLight(30000.f);
			UE_LOG(LogTemp, Log, TEXT("KodSun Intensity=%.2f Authored=%.2f"), FinalIntensity, Authored);
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
	bool bHaveSky = false;
	bool bPromotedPlacedSky = false;
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		bHaveSky = true;
		if (USkyLightComponent* Sky = It->GetLightComponent())
		{
			if (Sky->Mobility == EComponentMobility::Static)
			{
				UE_LOG(LogTemp, Log, TEXT("KodLight PlacedStatic Name=%s"), *It->GetName());
				Sky->SetMobility(EComponentMobility::Movable);
				if (Sky->Intensity < 0.05f)
				{
					Sky->SetIntensity(0.4f);
				}
				Sky->RecaptureSky();
				bPromotedPlacedSky = true;
				UE_LOG(LogTemp, Log, TEXT("KodLight SkyPromoted Name=%s"), *It->GetName());
			}
			else
			{
				Sky->SetMobility(EComponentMobility::Movable);
				Sky->SetIntensity(0.15f);
				Sky->SetLightColor(FLinearColor(0.55f, 0.62f, 0.72f));
			}
		}
		break;
	}
	if (!bHaveSky)
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
	if (!bHaveFog)
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
	if (!bTraining)
	{
		return;
	}
	const FLinearColor SkyColor = FLinearColor::FromSRGBColor(FColor(0x9F, 0xB4, 0xD9));
	const FLinearColor HazeColor = FLinearColor::FromSRGBColor(FColor(0xD9, 0xA6, 0x75));
	ASkyLight* SkyActor = nullptr;
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		SkyActor = *It;
		break;
	}
	if (!SkyActor)
	{
		SkyActor = World->SpawnActor<ASkyLight>();
	}
	if (SkyActor)
	{
		if (USkyLightComponent* Sky = SkyActor->GetLightComponent())
		{
			if (Sky->Mobility == EComponentMobility::Static || bPromotedPlacedSky)
			{
				if (Sky->Mobility == EComponentMobility::Static)
				{
					UE_LOG(LogTemp, Log, TEXT("KodLight PlacedStatic Name=%s"), *SkyActor->GetName());
					Sky->SetMobility(EComponentMobility::Movable);
					if (Sky->Intensity < 0.05f)
					{
						Sky->SetIntensity(0.8f);
					}
					Sky->SetRealTimeCapture(false);
					Sky->RecaptureSky();
					UE_LOG(LogTemp, Log, TEXT("KodLight SkyPromoted Name=%s"), *SkyActor->GetName());
				}
				else if (bShotAfter)
				{
					Sky->RecaptureSky();
				}
			}
			else
			{
				Sky->SetMobility(EComponentMobility::Movable);
				Sky->SetIntensity(0.15f);
				Sky->SetLightColor(SkyColor);
				Sky->SetRealTimeCapture(false);
				if (bShotAfter)
				{
					Sky->RecaptureSky();
				}
			}
		}
	}
	AExponentialHeightFog* FogActor = nullptr;
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		FogActor = *It;
		break;
	}
	if (!FogActor)
	{
		FogActor = World->SpawnActor<AExponentialHeightFog>();
	}
	if (FogActor && FogActor->GetComponent())
	{
		UExponentialHeightFogComponent* FogComp = FogActor->GetComponent();
		FogComp->SetFogDensity(0.002f);
		FogComp->SetFogInscatteringColor(HazeColor);
		FogComp->SetStartDistance(4000.f);
		FogComp->SetVolumetricFog(false);
	}
	UE_LOG(LogTemp, Log, TEXT("KodFaultline Sun single directional tutorial=%d"), bTraining ? 1 : 0);
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
				else if (Spec.TypeId == FName(KodWardenPaths::Id_Bunker) && Spec.CargoCap <= 0)
				{
					Spec.CargoCap = 4;
					Spec.MaxHealth = 400.f;
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
		return;
	}
	constexpr float Base = 8300.f;
	TArray<AActor*> Ignored;
	Ignored.Add(Anchor);
	const float FloorZ = ResolveResourceFloorZ(World, Sim, -Base, -Base, Anchor->GetActorLocation().Z, Ignored);
	if (USceneComponent* AnchorRoot = Anchor->GetRootComponent())
	{
		AnchorRoot->SetMobility(EComponentMobility::Movable);
	}
	TInlineComponentArray<UPrimitiveComponent*> Primitives;
	Anchor->GetComponents(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive)
		{
			Primitive->SetMobility(EComponentMobility::Movable);
		}
	}
	Anchor->SetActorLocation(FVector(-Base, -Base, FloorZ), false, nullptr, ETeleportType::TeleportPhysics);
	const FVector Seated = Anchor->GetActorLocation();
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

void AKodSlice0GameMode::ApplyCommandCenterMesh(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}
	constexpr float Footprint = 5.f * static_cast<float>(KodBuildGrid::CellUU);
	if (AKodBuilding* Building = Cast<AKodBuilding>(Actor))
	{
		if (UKodBuildingDefinition* Def = Building->GetDefinition())
		{
			Building->ApplyDefinition(Def);
			FString MeshName = TEXT("Placeholder");
			if (Building->BuildingSkel && Building->BuildingSkel->GetSkeletalMeshAsset())
			{
				MeshName = Building->BuildingSkel->GetSkeletalMeshAsset()->GetName();
			}
			else if (Building->BuildingMesh && Building->BuildingMesh->GetStaticMesh())
			{
				MeshName = Building->BuildingMesh->GetStaticMesh()->GetName();
			}
			UE_LOG(LogTemp, Log, TEXT("KodCC Mesh=%s Span=%.0f Actor=%s"),
				*MeshName,
				Footprint,
				*Actor->GetActorNameOrLabel());
			return;
		}
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
	for (UStaticMeshComponent* Comp : Meshes)
	{
		if (!Comp || Comp->ComponentHasTag(FName(TEXT("KodFx"))))
		{
			continue;
		}
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetStaticMesh(Mesh);
		Comp->SetRelativeScale3D(Scale);
		Comp->SetRelativeRotation(FRotator(0.f, Choice.Yaw, 0.f));
		Comp->SetRelativeLocation(FVector(0.f, 0.f, Choice.Z));
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("KodCC Mesh=%s Span=%.0f Actor=%s"),
		*Mesh->GetName(),
		Footprint,
		*Actor->GetActorNameOrLabel());
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
	AActor* WorkerAnchor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsSlice0CommandCenter(Actor))
		{
			continue;
		}
		const bool bStart = Actor->ActorHasTag(FName(TEXT("KodStartSpot_0")));
		const bool bAnchorStart = WorkerAnchor && WorkerAnchor->ActorHasTag(FName(TEXT("KodStartSpot_0")));
		if (!WorkerAnchor || (bStart && !bAnchorStart) || (bStart == bAnchorStart && Actor->GetName() < WorkerAnchor->GetName()))
		{
			WorkerAnchor = Actor;
		}
	}
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
	Crew.Sort([](const AKodUnit* A, const AKodUnit* B)
	{
		const int32 AId = A ? A->EntityId.Value : 0;
		const int32 BId = B ? B->EntityId.Value : 0;
		return AId < BId;
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
		Unit->SnapToGround();
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
	AActor* BaseActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsSlice0CommandCenter(Actor))
		{
			continue;
		}
		const bool bStart = Actor->ActorHasTag(FName(TEXT("KodStartSpot_0")));
		const bool bBaseStart = BaseActor && BaseActor->ActorHasTag(FName(TEXT("KodStartSpot_0")));
		if (!BaseActor || (bStart && !bBaseStart) || (bStart == bBaseStart && Actor->GetName() < BaseActor->GetName()))
		{
			BaseActor = Actor;
		}
	}
	const FVector Base = BaseActor ? BaseActor->GetActorLocation() : FVector(-7500.f, -7500.f, 80.f);
	ApplyCommandCenterMesh(BaseActor);
	const int32 Faction = Sim->GetPlayerFaction();
	const bool bScinder = Faction == 1;
	TArray<FVector> FocusPoints;
	FocusPoints.Add(Base);

	auto PlaceBuilding = [World, Sim, &FocusPoints](FName TypeId, const FVector& At, int32 FootX, int32 FootY)
	{
		UKodBuildingDefinition* Def = UKodSlice0Bootstrap::ResolveBuilding(TypeId, World);
		if (!Def)
		{
			Def = NewObject<UKodBuildingDefinition>(GetTransientPackage());
			Def->DefinitionId = TypeId;
			Def->FootprintX = FootX;
			Def->FootprintY = FootY;
			Def->MaxHealth = 1500.f;
		}
		const FTransform Xform(FRotator::ZeroRotator, At);
		AKodBuilding* Building = World->SpawnActor<AKodBuilding>(AKodBuilding::StaticClass(), Xform);
		if (!Building)
		{
			return;
		}
		Building->TeamId = 0;
		Building->Definition = Def;
		Building->Tags.AddUnique(FName(*FString::Printf(TEXT("KodShotName_%s"), *TypeId.ToString())));
		Building->Tags.AddUnique(FName(TEXT("KodShotLabel")));
		Building->ApplyDefinition(Def);
		FKodStructureSpec Spec;
		Spec.DefinitionId = TypeId;
		Spec.DisplayId = TypeId;
		Spec.TeamId = 0;
		Spec.FootprintX = FootX;
		Spec.FootprintY = FootY;
		Spec.MaxHealth = Def->MaxHealth;
		const FKodEntityId Id = Sim->RegisterStructure(Building, Spec);
		Building->EntityId = Id;
		FocusPoints.Add(Building->GetActorLocation());
		UE_LOG(LogTemp, Log, TEXT("KodShotBuildings Place Id=%s Loc=%.0f,%.0f,%.0f"),
			*TypeId.ToString(),
			At.X,
			At.Y,
			At.Z);
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
		AddBuilding(FName(TEXT("ScinderHost")), 2);
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
	for (const FShotPlace& Place : Plan)
	{
		const float Half = Place.SpanCm * 0.5f;
		const float Gap = Place.SpanCm + 200.f;
		const float Center = Edge + Gap + Half;
		const FVector At = Base + FVector(Center, 0.f, 0.f);
		const int32 Cells = FMath::Max(1, FMath::RoundToInt(Place.SpanCm / static_cast<float>(KodBuildGrid::CellUU)));
		if (Place.bBuilding)
		{
			PlaceBuilding(Place.Id, At, Cells, Cells);
		}
		else
		{
			PlaceUnit(Place.Id, At);
		}
		Edge = Center + Half;
	}

	FVector Sum = FVector::ZeroVector;
	for (const FVector& Point : FocusPoints)
	{
		Sum += Point;
	}
	OutFocus = FocusPoints.Num() > 0 ? Sum / static_cast<float>(FocusPoints.Num()) : Base;
	float Far = 0.f;
	for (const FVector& Point : FocusPoints)
	{
		Far = FMath::Max(Far, FVector::Dist2D(Point, OutFocus));
	}
	for (TActorIterator<AKodRTSCameraPawn> It(World); It; ++It)
	{
		if (It->IsA(AKodSpectatorPawn::StaticClass()))
		{
			continue;
		}
		It->FrameGroundSpan(OutFocus, (Far + 400.f) * 2.f);
		break;
	}
	UE_LOG(LogTemp, Log, TEXT("KodShotBuildings Faction=%d Focus=%.0f,%.0f,%.0f"),
		Faction,
		OutFocus.X,
		OutFocus.Y,
		OutFocus.Z);
	return true;
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

	AActor* Anchor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsSlice0CommandCenter(Actor))
		{
			continue;
		}
		const bool bStart = Actor->ActorHasTag(FName(TEXT("KodStartSpot_0")));
		const bool bAnchorStart = Anchor && Anchor->ActorHasTag(FName(TEXT("KodStartSpot_0")));
		if (!Anchor || (bStart && !bAnchorStart) || (bStart == bAnchorStart && Actor->GetName() < Anchor->GetName()))
		{
			Anchor = Actor;
		}
	}

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
