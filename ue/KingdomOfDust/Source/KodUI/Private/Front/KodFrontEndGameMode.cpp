#include "Front/KodFrontEndGameMode.h"

#include "Front/KodFrontEndWidget.h"
#include "GameFramework/HUD.h"
#include "Sim/KodSimSubsystem.h"

AKodFrontEndGameMode::AKodFrontEndGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = AHUD::StaticClass();
	PlayerControllerClass = AKodFrontEndPlayerController::StaticClass();
	bStartPlayersAsSpectators = false;
}

void AKodFrontEndGameMode::StartPlay()
{
	Super::StartPlay();
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->SetMenuMode(true);
			UE_LOG(LogTemp, Log, TEXT("KodLobby Menu paused"));
		}
	}
}

AKodFrontEndPlayerController::AKodFrontEndPlayerController()
{
	bShowMouseCursor = true;
}

void AKodFrontEndPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	if (UKodFrontEndWidget* Menu = CreateWidget<UKodFrontEndWidget>(this, UKodFrontEndWidget::StaticClass()))
	{
		Menu->AddToViewport(10);
		Menu->ActivateWidget();
	}
}
