#include "Game/KodGameMode.h"
#include "Game/KodGameState.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "Game/KodHUD.h"
#include "Game/KodRTSCameraPawn.h"
#include "Game/KodSpectatorController.h"
#include "Game/KodSpectatorPawn.h"
#include "Game/KodSkirmishSetup.h"
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
	PendingOptions = Options;
	FKodSkirmishSetup Lobby;
	if (FKodSkirmishSetup::TryParse(Options, Lobby))
	{
		MatchMode = Lobby.Mode;
	}
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
			Sim->SetOnlineSession(FKodSkirmishSetup::OptionsMarkOnline(PendingOptions));
			FKodSkirmishSetup Lobby;
			if (FKodSkirmishSetup::TryParse(PendingOptions, Lobby))
			{
				const int32 PlayerIndex = Lobby.ResolvePlayerFactionIndex();
				const int32 AiIndex = Lobby.ResolveRosterIndex(Lobby.AiFaction, Lobby.Seed + 17);
				Sim->SetAiEnabled(Lobby.bEnableAi);
				Sim->SetAiDifficulty(FKodSkirmishSetup::ToAiDifficulty(Lobby.Difficulty));
				Sim->SetPlayerFaction(PlayerIndex);
				Sim->SetScoreTeamColor(0, Lobby.PlayerColor);
				Sim->SetScoreTeamColor(1, Lobby.AiColor);
				UE_LOG(LogTemp, Log, TEXT("KodLobby Apply Mode=%d Map=%s Faction=%d AiFaction=%d Ai=%d Diff=%d PStart=%d AStart=%d PColor=%d AColor=%d Cheats=%d"),
					static_cast<int32>(Lobby.Mode),
					*Lobby.MapId.ToString(),
					PlayerIndex,
					AiIndex,
					Lobby.bEnableAi ? 1 : 0,
					FKodSkirmishSetup::ToAiDifficulty(Lobby.Difficulty),
					Lobby.PlayerStart,
					Lobby.AiStart,
					Lobby.PlayerColor,
					Lobby.AiColor,
					Sim->AreCheatsAllowed() ? 1 : 0);
			}
		}
	}
	if (bKodSpectatorSession)
	{
		UE_LOG(LogTemp, Log, TEXT("KodCam Spectate=1"));
	}
}

void AKodGameMode::SetAwaitFactionPrompt(bool bAwait)
{
	bAwaitFactionPrompt = bAwait;
}

void AKodGameMode::ApplyStartFaction(int32 FactionIndex)
{
	const int32 Index = FactionIndex == 1 ? 1 : 0;
	bAwaitFactionPrompt = false;
	UE_LOG(LogTemp, Log, TEXT("KodFaction Picked=%d"), Index);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
	{
		Sim->SetPlayerFaction(Index);
		Sim->SetSimPaused(false);
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	if (AKodHUD* HUD = Cast<AKodHUD>(PC->GetHUD()))
	{
		HUD->SetHudSkin(Index);
		HUD->CloseFactionPrompt();
	}
	if (AKodRTSCameraPawn* CameraPawn = Cast<AKodRTSCameraPawn>(PC->GetPawn()))
	{
		CameraPawn->SnapToPlayerStart();
	}
	if (AKodPlayerController* KodPC = Cast<AKodPlayerController>(PC))
	{
		KodPC->SelectPlayerCommandCenter();
	}
	UE_LOG(LogTemp, Log, TEXT("KodFaction Apply Index=%d Name=%s"), Index, Index == 1 ? TEXT("Scinder") : TEXT("Vanguard"));
}
