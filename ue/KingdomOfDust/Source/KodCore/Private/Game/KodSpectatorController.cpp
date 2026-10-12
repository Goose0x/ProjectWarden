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
}

void AKodSpectatorController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController() && !GetHUD())
	{
		ClientSetHUD(AKodHUD::StaticClass());
	}

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;
}
