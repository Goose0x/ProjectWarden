#include "Slice0/KodSlice0GameMode.h"
#include "Slice0/KodSlice0PlayerController.h"
#include "Game/KodRTSCameraPawn.h"
#include "Slice0/KodSlice0Bootstrap.h"
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

	bool IsSlice0GreyboxProxy(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}
		if (IsSlice0GreyboxProxyName(Actor->GetName()))
		{
			return true;
		}
		// Outliner label when it differs from the object name (StaticMeshActor_* labeled PROXY_*).
		return IsSlice0GreyboxProxyName(Actor->GetActorNameOrLabel());
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
		UE_LOG(LogTemp, Log, TEXT("Slice0 PROXY collision muted Count=0"));
		return;
	}

	int32 Muted = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsSlice0GreyboxProxy(Actor))
		{
			continue;
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
		if (bMutedActor)
		{
			++Muted;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Slice0 PROXY collision muted Count=%d"), Muted);
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
}
