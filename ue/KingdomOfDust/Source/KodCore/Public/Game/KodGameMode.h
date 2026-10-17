#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Sim/KodCheatRows.h"
#include "KodGameMode.generated.h"

/**
 * Default RTS game mode. Wires PlayerController / GameState / HUD / PlayerState
 * to Kod* C++ types. Content may subclass as GM_SandboxSkirmish.
 */
UCLASS(Blueprintable)
class KODCORE_API AKodGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKodGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;

	/** Apply Vanguard (0) or Scinder (1), unpause, and look at the player base. */
	virtual void ApplyStartFaction(int32 FactionIndex);

	/**
	 * -kod.ShotBuildings=1. Place the faction's extra meshes next to the base.
	 * OutFocus is the camera look-at. Returns false when this mode has nothing to place.
	 */
	virtual bool PlaceShotBuildings(FVector& OutFocus);

	/**
	 * -kod.ShotRiteWell=1. Place a RiteWell beside the player base.
	 * OutStructureId is the sim id so the shot can step build progress.
	 */
	virtual bool PlaceShotRiteWell(FVector& OutFocus, int32& OutStructureId);

	/** -kod.ShotBunker=1. Place a bunker beside the player base and frame it. */
	virtual bool PlaceShotBunker(FVector& OutFocus);

	/** Drive every KodShotLabel building through a construction fraction. 1 completes it. */
	virtual void SetShotBuildFraction(float Fraction);

	/** Drop a faction-select camera fade so the view is not held black or tinted. */
	void ClearMatchCameraFade();

	/** PIE waits on the faction prompt until the player picks. Cleared by ApplyStartFaction. */
	void SetAwaitFactionPrompt(bool bAwait);
	bool IsAwaitingFactionPrompt() const { return bAwaitFactionPrompt; }

	/** GameMode default, or the `Spectate=1` URL option. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	bool bStartAsSpectator = false;

	/** Cheats run only in Campaign and OfflineSkirmish. URL `Match=ranked` overrides this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Match")
	EKodMatchMode MatchMode = EKodMatchMode::OfflineSkirmish;

	FString GetMatchOptions() const { return PendingOptions; }
	TSubclassOf<APlayerController> GetSavedPlayerControllerClass() const { return KodSavedPlayerControllerClass; }
	TSubclassOf<APawn> GetSavedDefaultPawnClass() const { return KodSavedDefaultPawnClass; }

private:
	TSubclassOf<APlayerController> KodSavedPlayerControllerClass;
	TSubclassOf<APawn> KodSavedDefaultPawnClass;
	bool bKodSpectatorSession = false;
	bool bAwaitFactionPrompt = false;
	FString PendingOptions;
};
