#include "Game/KodGameMode.h"
#include "Game/KodGameState.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "Game/KodHUD.h"
#include "Game/KodRTSCameraPawn.h"

AKodGameMode::AKodGameMode()
{
	GameStateClass = AKodGameState::StaticClass();
	PlayerControllerClass = AKodPlayerController::StaticClass();
	PlayerStateClass = AKodPlayerState::StaticClass();
	HUDClass = AKodHUD::StaticClass();
	DefaultPawnClass = AKodRTSCameraPawn::StaticClass(); // RTS iso camera
}

void AKodGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
}

void AKodGameMode::StartPlay()
{
	Super::StartPlay();
}
