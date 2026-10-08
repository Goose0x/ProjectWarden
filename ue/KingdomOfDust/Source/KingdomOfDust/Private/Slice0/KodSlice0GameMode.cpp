#include "Slice0/KodSlice0GameMode.h"
#include "Slice0/KodSlice0PlayerController.h"
#include "Game/KodRTSCameraPawn.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "Actors/KodUnit.h"
#include "Warden/KodWardenPaths.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

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
	// Same Ranger DA, enemy team, cube + red tint. No AI controller and no initial order.
	// Sim auto-acquire engages an enemy in range, or retaliates if shot while idle.
	AKodUnit* Hostile = UKodSlice0Bootstrap::SpawnRanger(World, Transform, HostileTeamId, /*bForceCubeBody*/ true);
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
	UE_LOG(LogTemp, Log, TEXT("Slice0 Hostile spawned Name=%s Team=%d Loc=%.0f,%.0f,%.0f Anchor=%s"),
		*Hostile->GetName(),
		Hostile->TeamId,
		Spawned.X,
		Spawned.Y,
		Spawned.Z,
		Anchor);
}
