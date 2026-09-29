#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
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
};
