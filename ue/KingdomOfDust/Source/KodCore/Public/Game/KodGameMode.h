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

	/** GameMode default, or the `Spectate=1` URL option. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	bool bStartAsSpectator = false;

	/** Cheats run only in Campaign and OfflineSkirmish. URL `Match=ranked` overrides this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Match")
	EKodMatchMode MatchMode = EKodMatchMode::OfflineSkirmish;

	TSubclassOf<APlayerController> GetSavedPlayerControllerClass() const { return KodSavedPlayerControllerClass; }
	TSubclassOf<APawn> GetSavedDefaultPawnClass() const { return KodSavedDefaultPawnClass; }

private:
	TSubclassOf<APlayerController> KodSavedPlayerControllerClass;
	TSubclassOf<APawn> KodSavedDefaultPawnClass;
	bool bKodSpectatorSession = false;
};
