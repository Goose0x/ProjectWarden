#include "Game/KodGameMode.h"
#include "Game/KodGameState.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "Game/KodHUD.h"
#include "Game/KodRTSCameraPawn.h"
#include "Camera/PlayerCameraManager.h"
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

void AKodGameMode::ClearMatchCameraFade()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* Manager = PC ? PC->PlayerCameraManager : nullptr;
	if (!Manager)
	{
		return;
	}
	const float Before = Manager->FadeAmount;
	const int32 WasFading = Manager->bEnableFading ? 1 : 0;
	Manager->StopCameraFade();
	Manager->SetManualCameraFade(0.f, FLinearColor::Black, false);
	Manager->FadeAmount = 0.f;
	Manager->bEnableFading = false;
	if (Before > 0.001f || WasFading != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("KodFaction FadeClear Before=%.3f Fading=%d"), Before, WasFading);
	}
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
		UE_LOG(LogTemp, Log, TEXT("KodFaction Apply Index=%d Name=%s"), Index, Index == 1 ? TEXT("Scinder") : TEXT("Vanguard"));
		return;
	}
	ClearMatchCameraFade();
	if (AKodHUD* HUD = Cast<AKodHUD>(PC->GetHUD()))
	{
		HUD->SetHudSkin(Index);
		HUD->CloseFactionPrompt();
		HUD->ClearFullScreenOverlay();
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

bool AKodGameMode::PlaceShotBuildings(FVector& OutFocus)
{
	OutFocus = FVector::ZeroVector;
	return false;
}

bool AKodGameMode::PlaceShotRiteWell(FVector& OutFocus, int32& OutStructureId)
{
	OutFocus = FVector::ZeroVector;
	OutStructureId = 0;
	return false;
}

bool AKodGameMode::PlaceShotBunker(FVector& OutFocus)
{
	OutFocus = FVector::ZeroVector;
	return false;
}

void AKodGameMode::SetShotBuildFraction(float Fraction)
{
	(void)Fraction;
}
