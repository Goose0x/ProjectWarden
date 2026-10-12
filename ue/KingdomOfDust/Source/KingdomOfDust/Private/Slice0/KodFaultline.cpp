#include "Slice0/KodFaultline.h"

#include "Actors/KodBuilding.h"
#include "Actors/KodResourceNode.h"
#include "Actors/KodUnit.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/KodBuildingDefinition.h"
#include "Data/KodResourceNodeDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Game/KodHUD.h"
#include "GameFramework/PlayerController.h"
#include "Sim/KodSimSubsystem.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "Slice0/KodSlice0GameMode.h"
#include "Slice0/KodWorldSettings.h"
#include "Warden/KodWardenPaths.h"

namespace
{

	bool LoadJson(const TCHAR* FileName, TSharedPtr<FJsonObject>& OutRoot)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("KodDress/faultline_training"), FileName);
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			UE_LOG(LogTemp, Warning, TEXT("KodFaultline File Miss %s"), *Path);
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
	}

	FVector RectCenter(const TArray<TSharedPtr<FJsonValue>>* Rect, float Z)
	{
		int32 X = 0;
		int32 Y = 0;
		int32 W = 1;
		int32 H = 1;
		if (Rect && Rect->Num() >= 4)
		{
			X = FMath::RoundToInt((*Rect)[0]->AsNumber());
			Y = FMath::RoundToInt((*Rect)[1]->AsNumber());
			W = FMath::Max(1, FMath::RoundToInt((*Rect)[2]->AsNumber()));
			H = FMath::Max(1, FMath::RoundToInt((*Rect)[3]->AsNumber()));
		}
		return FVector((static_cast<float>(X) + static_cast<float>(W) * 0.5f) * 200.f - 11200.f,
			(static_cast<float>(Y) + static_cast<float>(H) * 0.5f) * 200.f - 11200.f,
			Z);
	}

	void SpawnOctagon(UWorld* World, const FVector& Center)
	{
		UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (!Cylinder)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Pad = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Center), Params);
		if (!Pad)
		{
			return;
		}
		Pad->Tags.Add(FName(TEXT("KodBaseSocket")));
		UStaticMeshComponent* Mesh = Pad->GetStaticMeshComponent();
		if (!Mesh)
		{
			return;
		}
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Cylinder);
		Mesh->SetWorldScale3D(FVector(18.f, 18.f, 0.4f));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
	}

	void PlaceResources(UWorld* World, UKodSimSubsystem* Sim)
	{
		TSharedPtr<FJsonObject> Root;
		if (!LoadJson(TEXT("layout.json"), Root))
		{
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Resources = nullptr;
		if (!Root->TryGetArrayField(TEXT("resources"), Resources) || !Resources)
		{
			return;
		}
		UKodSlice0Bootstrap::EnsureCatalog(World->GetAuthGameMode());
		UKodResourceNodeDefinition* JadeiteDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_JadeiteNode), World->GetAuthGameMode());
		UKodResourceNodeDefinition* LumineneDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_LumineneVent), World->GetAuthGameMode());
		for (const TSharedPtr<FJsonValue>& Value : *Resources)
		{
			const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Row.IsValid())
			{
				continue;
			}
			FString Type;
			FString Base;
			Row->TryGetStringField(TEXT("type"), Type);
			Row->TryGetStringField(TEXT("base"), Base);
			const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
			Row->TryGetArrayField(TEXT("rect"), Rect);
			const float Z = Base.Contains(TEXT("PLAYER_MAIN")) ? 300.f : 0.f;
			const FVector At = RectCenter(Rect, Z);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKodResourceNode* Node = World->SpawnActor<AKodResourceNode>(AKodResourceNode::StaticClass(), FTransform(At), Params);
			if (!Node)
			{
				continue;
			}
			const bool bVent = Type.Contains(TEXT("LUM")) || Type.Contains(TEXT("VENT"));
			const UKodResourceNodeDefinition* Def = bVent ? LumineneDef : JadeiteDef;
			if (Def)
			{
				Node->ApplyDefinition(Def);
			}
			else
			{
				Node->ApplyPlaceholder(bVent ? EKodResourceType::Luminene : EKodResourceType::Jadeite);
			}
			FKodResourceNodeSpec Spec;
			Spec.Type = bVent ? EKodResourceType::Luminene : EKodResourceType::Jadeite;
			double Amount = bVent ? 2250.0 : 1200.0;
			Row->TryGetNumberField(TEXT("amount"), Amount);
			Spec.Amount = FMath::RoundToInt(Amount);
			Spec.HarvestPerTrip = bVent ? 4 : 5;
			Spec.DefinitionId = FName(bVent ? KodWardenPaths::Id_LumineneVent : KodWardenPaths::Id_JadeiteNode);
			const FKodEntityId Id = Sim->RegisterResourceNode(Node, Spec);
			if (Id.IsValid())
			{
				Node->SetEntityId(Id);
				Node->ApplyJadeiteStandIn();
			}
			UE_LOG(LogTemp, Log, TEXT("KodFaultline Node %s Loc=%.0f,%.0f,%.0f"), *Type, At.X, At.Y, At.Z);
		}
	}

	void SeatOctagon(UWorld* World)
	{
		const FVector Socket(-7500.f, -7500.f, 340.f);
		SpawnOctagon(World, FVector(Socket.X, Socket.Y, 300.f));
		SpawnOctagon(World, FVector(900.f, -7900.f, 0.f));
		AActor* Anchor = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor)
			{
				continue;
			}
			const FString Label = Actor->GetActorNameOrLabel();
			if (!Label.Contains(TEXT("PROXY_CC")) && !Actor->ActorHasTag(FName(TEXT("KodStartSpot_0"))))
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
			UE_LOG(LogTemp, Warning, TEXT("KodFaultline CC Miss Socket=-7500,-7500"));
			return;
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
		if (USceneComponent* Root = Anchor->GetRootComponent())
		{
			Root->SetMobility(EComponentMobility::Movable);
		}
		Anchor->SetActorLocation(Socket, false, nullptr, ETeleportType::TeleportPhysics);
		const FVector Seated = Anchor->GetActorLocation();
		UE_LOG(LogTemp, Log, TEXT("KodFaultline CC Octagon Loc=%.0f,%.0f,%.0f"), Seated.X, Seated.Y, Seated.Z);
	}

	int32 SpawnScinder(UWorld* World, UKodSimSubsystem* Sim, FVector& WaveOrigin)
	{
		TSharedPtr<FJsonObject> Root;
		int32 Outposts = 0;
		if (!LoadJson(TEXT("map_features.json"), Root))
		{
			return 0;
		}
		const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
		if (!Root->TryGetArrayField(TEXT("features"), Features) || !Features)
		{
			return 0;
		}
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		for (const TSharedPtr<FJsonValue>& Value : *Features)
		{
			const TSharedPtr<FJsonObject> Feature = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Feature.IsValid())
			{
				continue;
			}
			FString Type;
			FString Id;
			FString Tag;
			Feature->TryGetStringField(TEXT("type"), Type);
			Feature->TryGetStringField(TEXT("id"), Id);
			Feature->TryGetStringField(TEXT("tag"), Tag);
			const TSharedPtr<FJsonObject>* Cells = nullptr;
			Feature->TryGetObjectField(TEXT("cells"), Cells);
			const TArray<TSharedPtr<FJsonValue>>* Rect = nullptr;
			if (Cells && Cells->IsValid())
			{
				(*Cells)->TryGetArrayField(TEXT("rect_xywh"), Rect);
			}
			if (Type == TEXT("objective-trigger"))
			{
				const FVector At = RectCenter(Rect, 200.f);
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AActor* Trigger = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), Params);
				if (!Trigger)
				{
					continue;
				}
				Trigger->Tags.Add(FName(TEXT("KodObjectiveTrigger")));
				Trigger->Tags.Add(FName(*Id));
				UBoxComponent* Box = NewObject<UBoxComponent>(Trigger);
				Trigger->SetRootComponent(Box);
				Box->RegisterComponent();
				int32 W = 1;
				int32 H = 1;
				if (Rect && Rect->Num() >= 4)
				{
					W = FMath::Max(1, FMath::RoundToInt((*Rect)[2]->AsNumber()));
					H = FMath::Max(1, FMath::RoundToInt((*Rect)[3]->AsNumber()));
				}
				Box->SetBoxExtent(FVector(W * 100.f, H * 100.f, 450.f));
				Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				Box->SetCollisionResponseToAllChannels(ECR_Overlap);
				continue;
			}
			if (Type != TEXT("enemy-structure"))
			{
				continue;
			}
			const FVector At = RectCenter(Rect, 80.f);
			const bool bOutpost = Tag == TEXT("KodEnemyOutpost");
			const bool bHq = Id == TEXT("ENEMY_HQ");
			if (bOutpost && Id == TEXT("OUTPOST_SPIRE"))
			{
				WaveOrigin = At;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AKodBuilding* Building = World->SpawnActor<AKodBuilding>(AKodBuilding::StaticClass(), FTransform(At), Params);
			if (!Building)
			{
				continue;
			}
			Building->Tags.Add(FName(*Tag));
			Building->TeamId = 1;
			if (UStaticMeshComponent* Mesh = Building->FindComponentByClass<UStaticMeshComponent>())
			{
				if (Cube)
				{
					Mesh->SetStaticMesh(Cube);
				}
				const float Span = bHq ? 5.f : (bOutpost && Id == TEXT("OUTPOST_SPIRE") ? 3.f : 2.f);
				Mesh->SetWorldScale3D(FVector(Span, Span, 1.5f));
				Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			}
			FKodStructureSpec Spec;
			Spec.DefinitionId = FName(*Tag);
			Spec.DisplayId = FName(*Id);
			Spec.TeamId = 1;
			Spec.MaxHealth = bOutpost ? 500.f : (bHq ? 1500.f : 750.f);
			Spec.FootprintX = bHq ? 5 : (Id == TEXT("OUTPOST_SPIRE") ? 3 : 2);
			Spec.FootprintY = Spec.FootprintX;
			const FKodEntityId Built = Sim->RegisterStructure(Building, Spec);
			Building->EntityId = Built;
			if (Built.IsValid() && bOutpost)
			{
				++Outposts;
			}
			const bool bGun = bOutpost || Id.Contains(TEXT("TOWER"));
			Sim->TuneStructureCombat(Built, 1, bGun ? 12.f : 0.f, bGun ? 700.f : 0.f);
			UE_LOG(LogTemp, Log, TEXT("KodFaultline Scinder %s HP=%.0f Loc=%.0f,%.0f"), *Id, Spec.MaxHealth, At.X, At.Y);
		}
		return Outposts;
	}

	bool MapLeafMatches(const FString& MapName, const FString& Entry)
	{
		const FString Leaf = FPaths::GetBaseFilename(Entry);
		return !Leaf.IsEmpty() && MapName.EndsWith(Leaf);
	}
}

bool KodFaultline::IsTrainingMap(const UWorld* World)
{
	if (!World)
	{
		return false;
	}
	if (const AKodWorldSettings* Settings = Cast<AKodWorldSettings>(World->GetWorldSettings()))
	{
		if (Settings->bFaultlineTraining)
		{
			return true;
		}
	}
	UWorld* MutableWorld = const_cast<UWorld*>(World);
	const AKodSlice0GameMode* Mode = Cast<AKodSlice0GameMode>(MutableWorld->GetAuthGameMode());
	if (!Mode)
	{
		return false;
	}
	const FString MapName = FPaths::GetBaseFilename(World->GetMapName());
	for (const FString& Entry : Mode->FaultlineMaps)
	{
		if (MapLeafMatches(MapName, Entry))
		{
			return true;
		}
	}
	return false;
}

void KodFaultline::Begin(UWorld* World)
{
	if (!IsTrainingMap(World))
	{
		return;
	}
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	if (!Sim)
	{
		return;
	}
	Sim->SetMatchMode(EKodMatchMode::Campaign);
	Sim->SetAiEnabled(false);
	if (APlayerController* Controller = World->GetFirstPlayerController())
	{
		if (const AKodHUD* HUD = Cast<AKodHUD>(Controller->GetHUD()))
		{
			Sim->SetPlayerFaction(HUD->GetHudSkin());
		}
	}
	PlaceResources(World, Sim);
	SeatOctagon(World);
	FVector WaveOrigin(1500.f, -300.f, 80.f);
	const int32 Outposts = SpawnScinder(World, Sim, WaveOrigin);
	Sim->ArmFaultlineWave(
		Outposts,
		FMath::RoundToInt(WaveOrigin.X),
		FMath::RoundToInt(WaveOrigin.Y),
		FMath::RoundToInt(WaveOrigin.Z),
		-7500,
		-7500,
		340);
	UE_LOG(LogTemp, Log, TEXT("KodFaultline Begin Map=%s Outposts=%d"), *World->GetMapName(), Outposts);
}
