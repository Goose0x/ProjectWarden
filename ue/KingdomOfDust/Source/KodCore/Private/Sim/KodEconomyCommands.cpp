#include "Sim/KodSimSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
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

	void KodGiveResources(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.GiveResources <J> <L>"));
			return;
		}
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.GiveResources: no sim (start PIE)"));
			return;
		}

		FKodResourceCost Gain;
		Gain.Jadeite = FMath::Max(0, FCString::Atoi(*Args[0]));
		Gain.Luminene = FMath::Max(0, FCString::Atoi(*Args[1]));
		Sim->Deposit(0, Gain);
	}

	void KodHarvestNode(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.HarvestNode <NodeId|nearest> <amount>"));
			return;
		}
		UKodSimSubsystem* Sim = FindPlaySim(World);
		if (!Sim)
		{
			UE_LOG(LogTemp, Warning, TEXT("kod.HarvestNode: no sim (start PIE)"));
			return;
		}

		FKodEntityId Id;
		if (Args[0].Equals(TEXT("nearest"), ESearchCase::IgnoreCase))
		{
			FVector Origin = FVector::ZeroVector;
			if (UWorld* SimWorld = Sim->GetWorld())
			{
				if (APlayerController* PC = SimWorld->GetFirstPlayerController())
				{
					if (const APawn* Pawn = PC->GetPawn())
					{
						Origin = Pawn->GetActorLocation();
					}
				}
			}
			Id = Sim->FindNearestResourceNode(Origin);
		}
		else
		{
			Id.Value = FCString::Atoi(*Args[0]);
		}

		const int32 Amount = FCString::Atoi(*Args[1]);
		Sim->HarvestNode(Id, Amount, /*TeamId*/ 0);
	}

	FAutoConsoleCommandWithWorldAndArgs GKodGiveResources(
		TEXT("kod.GiveResources"),
		TEXT("Deposit Jadeite and Luminene into team 0. Usage: kod.GiveResources <J> <L>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodGiveResources));

	FAutoConsoleCommandWithWorldAndArgs GKodHarvestNode(
		TEXT("kod.HarvestNode"),
		TEXT("Take <amount> from a node into team 0. Usage: kod.HarvestNode <NodeId|nearest> <amount>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&KodHarvestNode));
}
