#pragma once

#include "CoreMinimal.h"
#include "Game/KodGameMode.h"
#include "KodSlice0GameMode.generated.h"

/**
 * L_Slice0 PIE wiring: RTS camera pawn, Slice0 PC, ensure Warden bootstrap catalog.
 * StartPlay mutes greybox collision so ECC_Pawn traces reach KodUnit.
 * Tag KodGreybox mutes. Tag KodGround keeps BlockAll (move traces and the pawn capsule).
 * Actors with neither tag still use the PROXY_ / PROXY_GROUND label rules.
 * After the smoke Ranger, one hostile Ranger is spawned from the same definition.
 * It is a red-team Marine stand-in (skeletal mesh when that asset loads, cube otherwise).
 * Sim auto-acquire makes it fight back when an enemy is in weapon range.
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

private:
	/**
	 * NoCollision on greybox primitives. Tags win: KodGround keeps, KodGreybox mutes.
	 * Untagged actors fall back to the PROXY_ label rules. Actors stay placed and visible.
	 */
	void MuteGreyboxProxyCollision();

	/** One enemy Ranger (team 1) at the KodHostileAnchor / PROXY_HOSTILE label. */
	void SpawnHostileTestTarget();
};
