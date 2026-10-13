#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "KodFrontEndGameMode.generated.h"

class AKodFrontEndPlayerController;

/** Menu world. Does not start a match. L_MainMenu should use this game mode. */
UCLASS()
class KODUI_API AKodFrontEndGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AKodFrontEndGameMode();

	virtual void StartPlay() override;
};

UCLASS()
class KODUI_API AKodFrontEndPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKodFrontEndPlayerController();

	virtual void BeginPlay() override;
};
