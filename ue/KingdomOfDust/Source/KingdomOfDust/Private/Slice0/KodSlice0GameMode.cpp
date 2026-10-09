#include "Slice0/KodSlice0GameMode.h"
#include "Slice0/KodSlice0PlayerController.h"
#include "Game/KodRTSCameraPawn.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "Actors/KodUnit.h"
#include "Actors/KodResourceNode.h"
#include "Data/KodResourceNodeDefinition.h"
#include "Warden/KodWardenPaths.h"
#include "Sim/KodSimSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

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

	// Placed greybox has already run BeginPlay. Mute before the smoke Ranger spawn.
	MuteGreyboxProxyCollision();
	ApplyStartingResources();

	UKodSlice0Bootstrap::EnsureCatalog(this);

	// Touch Resolve so soft-path miss path is exercised in logs / debugger.
	UKodSlice0Bootstrap::ResolveWeapon(FName(KodWardenPaths::Id_RangerRifle), this);
	UKodSlice0Bootstrap::ResolveUnit(FName(KodWardenPaths::Id_Ranger), this);

	if (bSpawnSmokeRanger && GetWorld())
	{
		const FTransform T(FRotator::ZeroRotator, SmokeRangerOffset);
		UKodSlice0Bootstrap::SpawnRanger(GetWorld(), T, 0);
	}

	SpawnHostileTestTarget();
	// After both Marines so combat entity ids stay 1 then 2. Nodes are not attackable.
	SpawnResourceField();
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
	Sim->SetTeamBank(0, StartingJadeite, StartingOil);
	UE_LOG(LogTemp, Log, TEXT("KodEcon Bank Team=0 Jadeite=%d Oil=%d"), StartingJadeite, StartingOil);
}

void AKodSlice0GameMode::SpawnResourceField()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Log, TEXT("KodEcon Nodes Jadeite=0 Oil=0 Source=Default"));
		return;
	}
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	if (!Sim)
	{
		UE_LOG(LogTemp, Log, TEXT("KodEcon Nodes Jadeite=0 Oil=0 Source=Default"));
		return;
	}

	UKodSlice0Bootstrap::EnsureCatalog(this);
	UKodResourceNodeDefinition* JadeiteDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_JadeiteNode), this);
	UKodResourceNodeDefinition* OilDef = UKodSlice0Bootstrap::ResolveResourceNode(FName(KodWardenPaths::Id_OilSource), this);

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
		const bool bOil = Actor->ActorHasTag(FName(TEXT("KodResourceOil")));
		if (!bJadeite && !bOil)
		{
			continue;
		}
		FTaggedNode Entry;
		Entry.Actor = Actor;
		Entry.Type = bJadeite ? EKodResourceType::Jadeite : EKodResourceType::Oil;
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
		const bool bOil = Type == EKodResourceType::Oil;
		const int32 Amount = Def ? Def->Amount : (bOil ? KodEconomyDefaults::OilNodeAmount : KodEconomyDefaults::JadeiteNodeAmount);
		const int32 Trip = Def ? Def->HarvestPerTrip : (bOil ? KodEconomyDefaults::OilHarvestPerTrip : KodEconomyDefaults::JadeiteHarvestPerTrip);
		const FName DefId = (Def && !Def->DefinitionId.IsNone())
			? Def->DefinitionId
			: FName(bOil ? KodWardenPaths::Id_OilSource : KodWardenPaths::Id_JadeiteNode);

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
		const FKodEntityId Id = Sim->RegisterResourceNode(Actor, Type, Amount, Trip, DefId);
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
		int32 OilCount = 0;
		for (const FTaggedNode& Entry : Tagged)
		{
			const UKodResourceNodeDefinition* Def = Entry.Type == EKodResourceType::Oil ? OilDef : JadeiteDef;
			if (!RegisterNode(Entry.Actor, Entry.Type, Def))
			{
				continue;
			}
			if (Entry.Type == EKodResourceType::Oil)
			{
				++OilCount;
			}
			else
			{
				++JadeiteCount;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("KodEcon Nodes Jadeite=%d Oil=%d Source=Tagged"), JadeiteCount, OilCount);
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
	const float OilAngle = EndAngle + 52.f;
	const FVector OilDir = FieldDir.RotateAngleAxis(OilAngle, FVector::UpVector).GetSafeNormal2D();
	float OilRadius = ArcRadius;
	FVector OilLocation = AnchorLocation + OilDir * OilRadius;
	for (int32 Push = 0; Push < 8; ++Push)
	{
		float Nearest = MAX_flt;
		for (const FNodePlacement& Spot : Placements)
		{
			Nearest = FMath::Min(Nearest, FVector::Dist2D(OilLocation, Spot.Location));
		}
		if (Nearest >= 400.f)
		{
			break;
		}
		OilRadius += 80.f;
		OilLocation = AnchorLocation + OilDir * OilRadius;
	}
	FNodePlacement OilSpot;
	OilSpot.Type = EKodResourceType::Oil;
	OilSpot.Location = OilLocation;
	Placements.Add(OilSpot);

	int32 JadeiteCount = 0;
	int32 OilCount = 0;
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
		const UKodResourceNodeDefinition* Def = Spot.Type == EKodResourceType::Oil ? OilDef : JadeiteDef;
		if (!RegisterNode(Node, Spot.Type, Def))
		{
			Node->Destroy();
			continue;
		}
		Spawned.Add(Node);
		if (Spot.Type == EKodResourceType::Oil)
		{
			++OilCount;
		}
		else
		{
			++JadeiteCount;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("KodEcon Nodes Jadeite=%d Oil=%d Source=Default"), JadeiteCount, OilCount);
}
