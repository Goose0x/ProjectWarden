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
 * Resource nodes are registered after those two Marines (combat ids stay 1 then 2).
 * They are selectable gather sources, not attack targets.
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

	/** Team 0 match start. Jadeite is the internal id; the HUD labels it Credits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 StartingJadeite = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 StartingLuminene = 0;

	/** Spawn this many Dozers at the CC when the map has no team-0 worker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	bool bSpawnWorkers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 WorkerSpawnCount = 3;

	/** How close a hauler must get to PROXY_CC before the bank takes the cargo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 DropOffStandUU = 360;

private:
	/**
	 * NoCollision on greybox primitives. Tags win: KodGround keeps, KodGreybox mutes.
	 * Untagged actors fall back to the PROXY_ label rules. Actors stay placed and visible.
	 */
	void MuteGreyboxProxyCollision();

	/** One enemy Ranger (team 1) at the KodHostileAnchor / PROXY_HOSTILE label. */
	void SpawnHostileTestTarget();

	/** Team 0 bank. Logs KodEcon Bank Team=0 Jadeite=.. Luminene=.. */
	void ApplyStartingResources();

	/**
	 * Tag KodResourceJadeite / KodResourceLuminene (KodResourceOil still counts as a vent),
	 * or a default 6+1 field near PROXY_CC / PlayerStart.
	 * Runs after the two Marines so their entity ids stay 1 and 2.
	 * Registers PROXY_CC as the drop-off before returning.
	 */
	void SpawnResourceField();

	/** PROXY_CC, else the first Command Center tag, else nothing. */
	void RegisterCommandCenterDropOff();

	/** Three Dozers around the CC when no team-0 Dozer is already placed. */
	void SpawnWorkersIfNeeded();
};
