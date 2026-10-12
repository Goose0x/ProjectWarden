#include "Engine/EngineTypes.h"
#include "Engine/Player.h"
#include "Engine/World.h"
#include "Game/KodGameMode.h"
#include "Sim/KodSimSubsystem.h"
#include "Game/KodPlayerController.h"
#include "Game/KodRTSCameraPawn.h"
#include "Game/KodSpectatorController.h"
#include "Game/KodSpectatorPawn.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TWeakObjectPtr<APlayerController> GKodSavedPC;
	TWeakObjectPtr<APawn> GKodSavedPawn;

	void ApplyGameAndUI(APlayerController* PC)
	{
		if (!PC)
		{
			return;
		}
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}

	APlayerController* SpawnController(UWorld* World, TSubclassOf<APlayerController> Class, const FTransform& Xform)
	{
		if (!World || !Class)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<APlayerController>(Class, Xform, Params);
	}

	void KodSpectate(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const int32 Mode = (Args.Num() > 0) ? FCString::Atoi(*Args[0]) : 1;
		APlayerController* Current = World->GetFirstPlayerController();
		if (!Current)
		{
			UE_LOG(LogTemp, Warning, TEXT("KodCam Spectate=%d Miss=NoPC"), Mode == 0 ? 0 : 1);
			return;
		}

		if (Mode == 0)
		{
			APawn* CurrentPawn = Current->GetPawn();
			AKodSpectatorController* SpecPC = Cast<AKodSpectatorController>(Current);
			APlayerController* RestorePC = GKodSavedPC.Get();
			APawn* RestorePawn = GKodSavedPawn.Get();
			if (!RestorePC)
			{
				if (AKodGameMode* GameMode = World->GetAuthGameMode<AKodGameMode>())
				{
					const TSubclassOf<APlayerController> SavedClass = GameMode->GetSavedPlayerControllerClass();
					const TSubclassOf<APawn> SavedPawn = GameMode->GetSavedDefaultPawnClass();
					const FTransform Xform = CurrentPawn ? CurrentPawn->GetActorTransform() : FTransform::Identity;
					if (SavedPawn)
					{
						FActorSpawnParameters Params;
						Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						RestorePawn = World->SpawnActor<APawn>(SavedPawn, Xform, Params);
					}
					if (SavedClass)
					{
						RestorePC = SpawnController(World, SavedClass, Xform);
					}
				}
			}
			if (!RestorePC)
			{
				RestorePC = Current;
			}
			if (SpecPC && RestorePC && RestorePC != SpecPC)
			{
				UPlayer* Player = SpecPC->Player;
				APlayerState* PS = SpecPC->PlayerState;
				SpecPC->UnPossess();
				if (PS)
				{
					RestorePC->PlayerState = PS;
					PS->SetOwner(RestorePC);
					SpecPC->PlayerState = nullptr;
				}
				RestorePC->SetPlayer(Player);
				if (RestorePawn)
				{
					RestorePC->Possess(RestorePawn);
				}
				ApplyGameAndUI(RestorePC);
				SpecPC->Destroy();
			}
			else if (RestorePawn && CurrentPawn != RestorePawn)
			{
				Current->Possess(RestorePawn);
			}
			if (AKodSpectatorPawn* SpecPawn = Cast<AKodSpectatorPawn>(CurrentPawn))
			{
				SpecPawn->Destroy();
			}
			GKodSavedPC = nullptr;
			GKodSavedPawn = nullptr;
			UE_LOG(LogTemp, Log, TEXT("KodCam Spectate=0"));
			return;
		}

		if (Cast<AKodSpectatorController>(Current) || Cast<AKodSpectatorPawn>(Current->GetPawn()))
		{
			UE_LOG(LogTemp, Log, TEXT("KodCam Spectate=1"));
			return;
		}

		const FTransform Xform = Current->GetPawn()
			? Current->GetPawn()->GetActorTransform()
			: FTransform::Identity;
		AKodSpectatorPawn* SpecPawn = World->SpawnActorDeferred<AKodSpectatorPawn>(
			AKodSpectatorPawn::StaticClass(),
			Xform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!SpecPawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("KodCam Spectate=1 Miss=NoPawn"));
			return;
		}
		if (const AKodRTSCameraPawn* OldCam = Cast<AKodRTSCameraPawn>(Current->GetPawn()))
		{
			SpecPawn->CopyViewFrom(OldCam);
		}
		SpecPawn->FinishSpawning(Xform);

		GKodSavedPC = Current;
		GKodSavedPawn = Current->GetPawn();

		AKodSpectatorController* NewPC = World->SpawnActor<AKodSpectatorController>(
			AKodSpectatorController::StaticClass(),
			Xform);
		if (!NewPC)
		{
			Current->Possess(SpecPawn);
			UE_LOG(LogTemp, Log, TEXT("KodCam Spectate=1"));
			return;
		}

		UPlayer* Player = Current->Player;
		APlayerState* PS = Current->PlayerState;
		Current->UnPossess();
		if (PS)
		{
			NewPC->PlayerState = PS;
			PS->SetOwner(NewPC);
			Current->PlayerState = nullptr;
		}
		NewPC->SetPlayer(Player);
		NewPC->Possess(SpecPawn);
		NewPC->SetViewTarget(SpecPawn);
		ApplyGameAndUI(NewPC);
		UE_LOG(LogTemp, Log, TEXT("KodCam Spectate=1"));
	}

	FAutoConsoleCommandWithWorldAndArgs GKodSpectate(
		TEXT("kod.Spectate"),
		TEXT("Spectator camera. Usage: kod.Spectate 1 or kod.Spectate 0"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodSpectate));

	void KodExposed(const TArray<FString>& Args, UWorld* World)
	{
		UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
		if (!Sim)
		{
			return;
		}
		const int32 Value = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		Sim->SetExposedRule(Value != 0);
	}

	void KodExposedTime(const TArray<FString>& Args, UWorld* World)
	{
		UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
		if (!Sim || Args.Num() == 0)
		{
			return;
		}
		Sim->SetExposedTimeSeconds(FCString::Atoi(*Args[0]));
	}

	FAutoConsoleCommandWithWorldAndArgs GKodExposed(
		TEXT("kod.Exposed"),
		TEXT("Exposed rule. Usage: kod.Exposed 0 or kod.Exposed 1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodExposed));

	FAutoConsoleCommandWithWorldAndArgs GKodExposedTime(
		TEXT("kod.ExposedTime"),
		TEXT("Sim seconds before a one-CC player is exposed. Usage: kod.ExposedTime 360"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodExposedTime));
}
