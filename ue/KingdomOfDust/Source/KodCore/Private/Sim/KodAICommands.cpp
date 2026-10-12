#include "Sim/KodSimSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	UKodSimSubsystem* FindPlaySim(UWorld* World)
	{
		if (World)
		{
			if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
			{
				return Sim;
			}
		}
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (!Candidate || Candidate->WorldType != EWorldType::PIE)
			{
				continue;
			}
			if (UKodSimSubsystem* Sim = Candidate->GetSubsystem<UKodSimSubsystem>())
			{
				return Sim;
			}
		}
		return nullptr;
	}

	void KodAI(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.AI <0|1>"));
			return;
		}
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.AI: no sim (start PIE)"));
			return;
		}
		const int32 Enabled = FCString::Atoi(*Args[0]) != 0 ? 1 : 0;
		Sim->SetAiEnabled(Enabled != 0);
		UE_LOG(LogTemp, Log, TEXT("KodAI Enabled=%d"), Enabled);
	}

	void KodAIDifficulty(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.AIDifficulty <n>"));
			return;
		}
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.AIDifficulty: no sim (start PIE)"));
			return;
		}
		const FString Name = Args[0];
		int32 Difficulty = 1;
		if (Name.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
		{
			Difficulty = 1;
		}
		else if (Name.Equals(TEXT("Medium"), ESearchCase::IgnoreCase))
		{
			Difficulty = 2;
		}
		else if (Name.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
		{
			Difficulty = 3;
		}
		else if (Name.Equals(TEXT("Elite"), ESearchCase::IgnoreCase))
		{
			Difficulty = 4;
		}
		else
		{
			Difficulty = FMath::Max(1, FCString::Atoi(*Name));
		}
		Sim->SetAiDifficulty(Difficulty);
		const TCHAR* Labels[] = { TEXT("Easy"), TEXT("Easy"), TEXT("Medium"), TEXT("Hard"), TEXT("Elite") };
		const int32 LabelIndex = FMath::Clamp(Sim->GetAiDifficulty(), 1, 4);
		UE_LOG(LogTemp, Log, TEXT("KodAI Difficulty=%d Name=%s"), Sim->GetAiDifficulty(), Labels[LabelIndex]);
	}

	FAutoConsoleCommandWithWorldAndArgs GKodAI(
		TEXT("kod.AI"),
		TEXT("1 runs the team 1 opponent. 0 stops new AI orders. Usage: kod.AI <0|1>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodAI));

	FAutoConsoleCommandWithWorldAndArgs GKodAIDifficulty(
		TEXT("kod.AIDifficulty"),
		TEXT("Wave growth and the AI starting Credits. 1 is 50 Credits. Usage: kod.AIDifficulty <n>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodAIDifficulty));
}
