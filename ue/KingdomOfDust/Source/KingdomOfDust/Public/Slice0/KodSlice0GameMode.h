#pragma once

#include "CoreMinimal.h"
#include "Game/KodGameMode.h"
#include "KodSlice0GameMode.generated.h"

/**
 * L_Slice0 PIE wiring: RTS camera pawn, Slice0 PC, ensure Warden bootstrap catalog.
 * StartPlay mutes greybox collision so ECC_Pawn traces reach KodUnit.
 * Tag KodGreybox mutes. Tag KodGround keeps BlockAll (move traces and the pawn capsule).
 * Actors with neither tag still use the PROXY_ / PROXY_GROUND label rules.
 * The smoke Rangers at (400, 0) and the hostile anchor are not spawned.
 * Resource nodes and workers seat from the command center after it moves.
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
	virtual void ApplyStartFaction(int32 FactionIndex) override;
	virtual bool PlaceShotBuildings(FVector& OutFocus) override;
	virtual bool PlaceShotRiteWell(FVector& OutFocus, int32& OutStructureId) override;
	virtual bool PlaceShotBunker(FVector& OutFocus) override;
	virtual bool PlaceShotShadowSquads(FVector& OutFocus) override;
	virtual void SetShotBuildFraction(float Fraction) override;

	/** Level PROXY_CC even while it is still at the origin. Seating moves this actor. */
	static AActor* FindBaseToSeat(UWorld* World);
	/** Player command center after it has left the origin, else the start spot. */
	static AActor* FindPlayerBaseActor(UWorld* World);

	/** Leaves one atmosphere directional light. Spawned suns are movable. Returns the enabled count. */
	static int32 KeepSingleDirectionalSun(UWorld* World, bool bTutorialLook);

	/** One unbound manual exposure for PIE, -game, and shots. kod.Exposure is the EV100 bias. */
	static void ApplyMatchExposure(UWorld* World);

	/** SM_general when it loads, otherwise a cube whose footprint is 1000 cm. */
	static void ApplyCommandCenterMesh(AActor* Actor);

	/** Moves an actor even when its root was static or missing. False if it stays put. */
	static bool PlaceActorAt(AActor* Actor, const FVector& Dest);

	/** When true, spawn one Ranger at origin+offset for walk gate smoke. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Slice0")
	bool bSpawnSmokeRanger = false;

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
	int32 WorkerSpawnCount = 8;

	/** How close a hauler must get to PROXY_CC before the bank takes the cargo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 DropOffStandUU = 360;

	/** Playable box in build cells, centred on the origin. KodGround wins only when it is larger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Map")
	int32 PlayableCells = 128;

	/**
	 * Short map names that run Faultline Training. A PIE prefix such as UEDPIE_0_ still matches.
	 * AKodWorldSettings::bFaultlineTraining also turns the tutorial on.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Kod|Map")
	TArray<FString> FaultlineMaps;

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

	/** Crystals on a 1160 arc and gas on the diagonal, relative to one CC. */
	void PlaceFieldAt(const FVector& AnchorLocation, const FVector& FieldDir);

	/** Workers between the CC and the field. They spawn idle. */
	int32 SpawnWorkerFan(const FVector& AnchorLocation, const FVector& FieldDir, int32 TeamId, int32 Count, AActor* Anchor = nullptr);

	/** Eight Dozers between the CC and the field when no team-0 Dozer is already placed. */
	void SpawnWorkersIfNeeded();

	/** Code box of PlayableCells, widened only when a placed KodGround is larger. Spawns the runtime floor. */
	void PublishPlayableBounds();

	/** Move the player command center onto the southwest base point. */
	void SeatPlayerBase();

	/** Mineral and gas sets for the two natural expansions. */
	void PlaceExpansionFields();

	/** Look-at the player command center after the bases have moved. */
	void FocusOpeningCamera();

	/** Team 1 command center and eight AMPs. Skipped when kod.AI is 0. */
	void SpawnAiOpponent();
};
