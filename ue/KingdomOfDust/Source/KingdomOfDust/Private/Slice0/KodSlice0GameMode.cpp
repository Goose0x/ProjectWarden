#include "Slice0/KodSlice0GameMode.h"
#include "Slice0/KodSlice0PlayerController.h"
#include "Game/KodRTSCameraPawn.h"
#include "Slice0/KodSlice0Bootstrap.h"
#include "Warden/KodWardenPaths.h"
#include "Engine/World.h"

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

void AKodSlice0GameMode::StartPlay()
{
	Super::StartPlay();

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
