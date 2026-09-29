#pragma once

#include "CoreMinimal.h"
#include "Game/KodGameMode.h"
#include "KodSlice0GameMode.generated.h"

/**
 * L_Slice0 PIE wiring: RTS camera pawn, Slice0 PC, ensure Warden bootstrap catalog.
 *
 * World Settings class path (module KingdomOfDust, not the .uproject file name):
 * /Script/KingdomOfDust.KodSlice0GameMode
 */
UCLASS(Blueprintable)
class KINGDOMOFDUST_API AKodSlice0GameMode : public AKodGameMode
{
	GENERATED_BODY()

public:
	AKodSlice0GameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;

	/** When true, spawn one Ranger at origin+offset for walk gate smoke. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Slice0")
	bool bSpawnSmokeRanger = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Slice0")
	FVector SmokeRangerOffset = FVector(400.f, 0.f, 100.f);
};
