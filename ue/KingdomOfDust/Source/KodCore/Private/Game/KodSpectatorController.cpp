#include "Game/KodSpectatorController.h"

#include "Engine/World.h"
#include "Game/KodHUD.h"
#include "GameFramework/PlayerController.h"

AKodSpectatorController::AKodSpectatorController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	bAutoManageActiveCameraTarget = true;
	HUDClass = AKodHUD::StaticClass();
}

void AKodSpectatorController::BeginPlay()
{
	Super::BeginPlay();

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;
}
