#include "Game/KodSpectatorController.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Game/KodHUD.h"
#include "Game/KodPortraitStage.h"
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

void AKodSpectatorController::BuildHiddenComponentList(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents)
{
	Super::BuildHiddenComponentList(ViewLocation, HiddenComponents);
	if (const AKodHUD* HUD = Cast<AKodHUD>(GetHUD()))
	{
		if (const AKodPortraitStage* Stage = HUD->GetPortraitStage())
		{
			Stage->AppendHiddenPrimitives(HiddenComponents);
		}
	}
}
