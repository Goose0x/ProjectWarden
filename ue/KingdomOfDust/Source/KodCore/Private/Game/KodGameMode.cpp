#include "Game/KodGameMode.h"
#include "Game/KodGameState.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "Game/KodHUD.h"
#include "Game/KodRTSCameraPawn.h"
#include "Game/KodSpectatorController.h"
#include "Game/KodSpectatorPawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sim/KodSimSubsystem.h"

AKodGameMode::AKodGameMode()
{
	GameStateClass = AKodGameState::StaticClass();
	PlayerControllerClass = AKodPlayerController::StaticClass();
	PlayerStateClass = AKodPlayerState::StaticClass();
	HUDClass = AKodHUD::StaticClass();
	DefaultPawnClass = AKodRTSCameraPawn::StaticClass();
}

void AKodGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	const FString SpectateOpt = UGameplayStatics::ParseOption(Options, TEXT("Spectate"));
	const FString MatchOpt = UGameplayStatics::ParseOption(Options, TEXT("Match")).ToLower();
	if (MatchOpt == TEXT("campaign"))
	{
		MatchMode = EKodMatchMode::Campaign;
	}
	else if (MatchOpt == TEXT("skirmish") || MatchOpt == TEXT("offline"))
	{
		MatchMode = EKodMatchMode::OfflineSkirmish;
	}
	else if (MatchOpt == TEXT("ranked") || MatchOpt == TEXT("online"))
	{
		MatchMode = EKodMatchMode::OnlineRanked;
	}
	else if (MatchOpt == TEXT("replay"))
	{
		MatchMode = EKodMatchMode::Replay;
	}
	else if (MatchOpt == TEXT("custom"))
	{
		MatchMode = EKodMatchMode::Custom;
	}
	if (bStartAsSpectator || SpectateOpt == TEXT("1"))
	{
		KodSavedPlayerControllerClass = PlayerControllerClass;
		KodSavedDefaultPawnClass = DefaultPawnClass;
		PlayerControllerClass = AKodSpectatorController::StaticClass();
		DefaultPawnClass = AKodSpectatorPawn::StaticClass();
		bKodSpectatorSession = true;
	}
	Super::InitGame(MapName, Options, ErrorMessage);
}

void AKodGameMode::StartPlay()
{
	Super::StartPlay();
	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->SetMatchMode(MatchMode);
		}
	}
	if (bKodSpectatorSession)
	{
		UE_LOG(LogTemp, Log, TEXT("KodCam Spectate=1"));
	}
}
