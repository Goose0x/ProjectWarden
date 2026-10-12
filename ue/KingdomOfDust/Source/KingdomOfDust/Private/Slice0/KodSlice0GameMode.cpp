#include "Slice0/KodSlice0GameMode.h"
#include "Slice0/KodSlice0PlayerController.h"
#include "Game/KodHUD.h"
#include "Game/KodRTSCameraPawn.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "Actors/KodUnit.h"
#include "Actors/KodBuilding.h"
#include "Actors/KodResourceNode.h"
#include "Data/KodResourceNodeDefinition.h"
#include "Data/KodAbilityDefinition.h"
#include "Data/KodUnitDefinition.h"
#include "Data/KodBuildingDefinition.h"
#include "Warden/KodWardenPaths.h"
#include "Sim/KodSimSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/CommandLine.h"
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
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.bBlockingHit)
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
}

AKodSlice0GameMode::AKodSlice0GameMode()
{
	PlayerControllerClass = AKodSlice0PlayerController::StaticClass();
	DefaultPawnClass = AKodRTSCameraPawn::StaticClass();
	// Parent sets this too. Repeat it here so a map or config cannot leave Slice 0 without AKodHUD.
	HUDClass = AKodHUD::StaticClass();
	WorkerSpawnCount = KodEconomyDefaults::WorkerSpawnCount;
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

void AKodSlice0GameMode::StartPlay()
{
	Super::StartPlay();
	if (GetWorld() && GetWorld()->URL.HasOption(TEXT("Menu")))
	{
		if (UKodSimSubsystem* Sim = GetWorld()->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->SetMenuMode(true);
		}
	}

	// Placed greybox has already run BeginPlay. Mute before the smoke Ranger spawn.
	MuteGreyboxProxyCollision();
	ApplyStartingResources();

	UKodSlice0Bootstrap::EnsureCatalog(this);

	// Touch Resolve so soft-path miss path is exercised in logs / debugger.
	UKodSlice0Bootstrap::ResolveWeapon(FName(KodWardenPaths::Id_RangerRifle), this);
	UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_Ranger), this);
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
				Spec.bHealer = Def->bHealer;
				Spec.CargoSize = Def->CargoSize;
				Spec.BaseArmor = FMath::Max(0, FMath::RoundToInt(Def->Armor));
				Spec.EnergyMax = Def->EnergyMax;
				Spec.EnergyStart = Def->EnergyStart;
				Spec.RequiresId = Def->RequiresId;
				Sim->RegisterTrainSpec(Spec);
			};
			if (const UKodUnitDefinition* Amp = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_Dozer), this))
			{
				RegisterUnit(Amp, FName(KodWardenPaths::Id_Dozer), FName(TEXT("AMP")));
			}
			if (const UKodUnitDefinition* Marine = UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_Ranger), this))
			{
				RegisterUnit(Marine, FName(KodWardenPaths::Id_Ranger), FName(TEXT("Marine")));
				FKodTrainSpec Spec;
				Spec.UnitId = Marine->DefinitionId.IsNone() ? FName(KodWardenPaths::Id_Ranger) : Marine->DefinitionId;
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
			};
			TArray<FName> NoTrains;
			TArray<FName> MarineTrain;
			MarineTrain.Add(FName(KodWardenPaths::Id_Ranger));
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
			StampRoster(FName(KodWardenPaths::Id_Ranger), true, true, false, 1, 0, 0, FName());
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
		}
	}

	if (bSpawnSmokeRanger && GetWorld())
	{
		const FTransform T(FRotator::ZeroRotator, SmokeRangerOffset);
		UKodSlice0Bootstrap::SpawnRanger(GetWorld(), T, 0);
	}

	SpawnHostileTestTarget();
	// After both Marines so combat entity ids stay 1 then 2. Nodes are not attackable.
	// The drop-off is registered at the end of the field, then the workers.
	SpawnResourceField();
	SpawnWorkersIfNeeded();
	PublishPlayableBounds();
	SpawnAiOpponent();
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->EnsureBalanceTables();
			Sim->ApplyBalance();
		}
	}
}

void AKodSlice0GameMode::PublishPlayableBounds()
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return;
	}
	FBox Combined(ForceInit);
	bool bAny = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
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
		Combined += Bounds;
		bAny = true;
	}
	if (bAny)
	{
		Sim->SetPlayableWorldBox(Combined);
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
	// Same Ranger definition as the player Marine (skeletal mesh + AnimBP when they load).
	// Cube only if that mesh soft ref misses. TeamColor tints team 1 red. No AI controller.
	AKodUnit* Hostile = UKodSlice0Bootstrap::SpawnRanger(World, Transform, HostileTeamId, /*bForceCubeBody*/ false);
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

	const float FacingDeg = FMath::RadiansToDegrees(FMath::Atan2(FieldDir.Y, FieldDir.X));
	UE_LOG(LogTemp, Log, TEXT("KodEcon Field Anchor=%s Loc=%.0f,%.0f,%.0f Facing=%.0f"),
		AnchorActor ? *AnchorActor->GetActorNameOrLabel() : TEXT("fallback"),
		AnchorLocation.X,
		AnchorLocation.Y,
		AnchorLocation.Z,
		FacingDeg);

	// Chord between neighbours is 120 cm (inside 110–130). The arc faces the anchor.
	constexpr float ArcRadius = 720.f;
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

	// 52 degrees past the +end of the arc (inside 45–60), then pushed until it is 400 cm from every crystal.
	const float EndAngle = 2.5f * StepDeg;
	const float VentAngle = EndAngle + 52.f;
	const FVector VentDir = FieldDir.RotateAngleAxis(VentAngle, FVector::UpVector).GetSafeNormal2D();
	float VentRadius = ArcRadius;
	FVector VentLocation = AnchorLocation + VentDir * VentRadius;
	for (int32 Push = 0; Push < 8; ++Push)
	{
		float Nearest = MAX_flt;
		for (const FNodePlacement& Spot : Placements)
		{
			Nearest = FMath::Min(Nearest, FVector::Dist2D(VentLocation, Spot.Location));
		}
		if (Nearest >= 400.f)
		{
			break;
		}
		VentRadius += 80.f;
		VentLocation = AnchorLocation + VentDir * VentRadius;
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

	RegisterCommandCenterDropOff();
	UE_LOG(LogTemp, Log, TEXT("KodEcon Nodes Jadeite=%d Luminene=%d Source=Default"), JadeiteCount, LumineneCount);
}

static void ApplyPrototypeStructureMesh(AActor* Actor, const UKodBuildingDefinition* Def)
{
	if (!Actor)
	{
		return;
	}
	TSoftObjectPtr<UStaticMesh> MeshRef = (Def && !Def->Mesh.IsNull())
		? Def->Mesh
		: TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(KodWardenPaths::ProtoHqMesh));
	UStaticMesh* Mesh = MeshRef.LoadSynchronous();
	if (!Mesh)
	{
		return;
	}
	UMaterialInterface* Material = nullptr;
	if (Def && !Def->Material.IsNull())
	{
		Material = Def->Material.LoadSynchronous();
	}
	const FVector Scale = Def ? Def->MeshScale : FVector::OneVector;
	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	Actor->GetComponents(Meshes);
	for (UStaticMeshComponent* Comp : Meshes)
	{
		if (!Comp)
		{
			continue;
		}
		Comp->SetStaticMesh(Mesh);
		if (!Scale.Equals(FVector::OneVector, 0.01f))
		{
			Comp->SetRelativeScale3D(Scale);
		}
		if (Material)
		{
			Comp->SetMaterial(0, Material);
		}
		break;
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
	const FKodEntityId Id = Sim->RegisterStructure(Anchor, Spec);
	if (!Id.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("KodEcon DropOff Miss Anchor=%s"), *Anchor->GetActorNameOrLabel());
		return;
	}
	EnableResourceSelectCollision(Anchor);
	ApplyPrototypeStructureMesh(Anchor, CCDef);
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

	int32 Placed = 0;
	for (TActorIterator<AKodUnit> It(World); It; ++It)
	{
		AKodUnit* Unit = *It;
		if (!Unit || Unit->TeamId != 0)
		{
			continue;
		}
		if (Unit->ActorHasTag(FName(TEXT("KodUnitDozer"))))
		{
			++Placed;
			continue;
		}
		if (const UKodUnitDefinition* Def = Unit->GetDefinition())
		{
			if (Def->bCanGather || Def->DefinitionId == FName(KodWardenPaths::Id_Dozer))
			{
				++Placed;
			}
		}
	}
	if (Placed > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("KodEcon Workers Spawned=0 Reason=Placed Count=%d"), Placed);
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

	const FVector AnchorLocation = Anchor ? Anchor->GetActorLocation() : SmokeRangerOffset;
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

	const int32 Count = FMath::Clamp(WorkerSpawnCount, 1, 8);
	int32 Spawned = 0;
	TArray<AActor*> Ignored;
	int32 HalfSpan = (5 * KodBuildGrid::CellUU) / 2;
	if (Anchor)
	{
		FKodStructureState CcStructure;
		if (Sim->TryGetStructure(Sim->FindIdForActor(Anchor), CcStructure))
		{
			HalfSpan = (FMath::Max(CcStructure.FootprintX, CcStructure.FootprintY) * KodBuildGrid::CellUU) / 2;
		}
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = (static_cast<float>(Index) - (static_cast<float>(Count) - 1.f) * 0.5f) * 28.f;
		const FVector Dir = FieldDir.RotateAngleAxis(Angle, FVector::UpVector);
		float Radius = static_cast<float>(HalfSpan + KodBuildGrid::CellUU);
		FVector Spot = AnchorLocation + Dir * Radius;
		for (int32 Step = 0; Step < 8 && Sim->IsCellBlocked(KodBuildGrid::CellOf(Spot.X), KodBuildGrid::CellOf(Spot.Y)); ++Step)
		{
			Radius += static_cast<float>(KodBuildGrid::CellUU);
			Spot = AnchorLocation + Dir * Radius;
		}
		const float FloorZ = ResolveResourceFloorZ(World, Sim, Spot.X, Spot.Y, AnchorLocation.Z, Ignored);
		const FTransform Xform(FRotator::ZeroRotator, FVector(Spot.X, Spot.Y, FloorZ + 100.f));
		AKodUnit* Worker = UKodSlice0Bootstrap::SpawnDozer(World, Xform, /*TeamId*/ 0);
		if (!Worker)
		{
			continue;
		}
		Ignored.Add(Worker);
		++Spawned;
		const FVector At = Worker->GetActorLocation();
		UE_LOG(LogTemp, Log, TEXT("KodEcon Worker Spawn Id=%d Loc=%.0f,%.0f,%.0f"),
			Worker->EntityId.Value,
			At.X,
			At.Y,
			At.Z);
	}

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
		int32 MinX = 0;
		int32 MinY = 0;
		int32 MaxX = 0;
		int32 MaxY = 0;
		if (Sim->TryGetPlayableCellBounds(MinX, MinY, MaxX, MaxY))
		{
			const int32 Inset = 3;
			const int32 Left = MinX + Inset;
			const int32 Right = FMath::Max(Left, MaxX - Inset);
			const int32 Bottom = MinY + Inset;
			const int32 Top = FMath::Max(Bottom, MaxY - Inset);
			const int32 Corners[4][2] = {
				{Left, Bottom},
				{Left, Top},
				{Right, Bottom},
				{Right, Top}
			};
			int32 BestX = Right;
			int32 BestY = Top;
			int64 BestDist = -1;
			for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
			{
				const int32 CellX = Corners[CornerIndex][0];
				const int32 CellY = Corners[CornerIndex][1];
				const float X = KodBuildGrid::CellCenter(CellX);
				const float Y = KodBuildGrid::CellCenter(CellY);
				const int64 DX = static_cast<int64>(FMath::RoundToInt(X - PlayerLoc.X));
				const int64 DY = static_cast<int64>(FMath::RoundToInt(Y - PlayerLoc.Y));
				const int64 Dist = DX * DX + DY * DY;
				const bool bCloserTie = Dist == BestDist && (CellX < BestX || (CellX == BestX && CellY < BestY));
				if (Dist > BestDist || bCloserTie)
				{
					BestDist = Dist;
					BestX = CellX;
					BestY = CellY;
				}
			}
			Seed = FVector(KodBuildGrid::CellCenter(BestX), KodBuildGrid::CellCenter(BestY), PlayerLoc.Z);
		}
		else
		{
			Seed = PlayerLoc + FVector(8000.f, 8000.f, 0.f);
		}
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

	const int32 HalfSpan = (FMath::Max(Spec.FootprintX, Spec.FootprintY) * KodBuildGrid::CellUU) / 2;
	int32 Spawned = 0;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const float Angle = static_cast<float>(Index) * 60.f;
		const FVector Dir = FVector(1.f, 0.f, 0.f).RotateAngleAxis(Angle, FVector::UpVector);
		float Radius = static_cast<float>(HalfSpan + KodBuildGrid::CellUU);
		FVector Spot = FVector(Seed.X, Seed.Y, Seed.Z) + Dir * Radius;
		for (int32 Step = 0; Step < 8 && Sim->IsCellBlocked(KodBuildGrid::CellOf(Spot.X), KodBuildGrid::CellOf(Spot.Y)); ++Step)
		{
			Radius += static_cast<float>(KodBuildGrid::CellUU);
			Spot = FVector(Seed.X, Seed.Y, Seed.Z) + Dir * Radius;
		}
		const float WorkerZ = ResolveResourceFloorZ(World, Sim, Spot.X, Spot.Y, Seed.Z, Ignored);
		const FTransform WorkerXform(FRotator::ZeroRotator, FVector(Spot.X, Spot.Y, WorkerZ + 100.f));
		AKodUnit* Worker = UKodSlice0Bootstrap::SpawnDozer(World, WorkerXform, /*TeamId*/ 1);
		if (!Worker)
		{
			continue;
		}
		Ignored.Add(Worker);
		++Spawned;
	}

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
