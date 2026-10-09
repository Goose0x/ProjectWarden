#include "KodCommandSubsystem.h"
#include "Sim/KodSimSubsystem.h"
#include "Components/KodMoveComponent.h"
#include "Components/KodAttackComponent.h"
#include "KodBuildQueueComponent.h"
#include "Data/KodUnitDefinition.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

void UKodCommandSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Pending.Reset();
	NextCommandId = 1;
}

void UKodCommandSubsystem::Deinitialize()
{
	Pending.Reset();
	Super::Deinitialize();
}

TStatId UKodCommandSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKodCommandSubsystem, STATGROUP_Tickables);
}

void UKodCommandSubsystem::Tick(float DeltaTime)
{
	(void)DeltaTime;
	if (bAutoProcess)
	{
		ProcessPending(64);
	}
}

int32 UKodCommandSubsystem::Enqueue(FKodCommand Command)
{
	Command.CommandId = NextCommandId++;
	Pending.Add(Command);
	OnCommandEnqueued.Broadcast(Command);
	return Command.CommandId;
}

int32 UKodCommandSubsystem::ProcessPending(int32 MaxCount)
{
	int32 Processed = 0;
	while (Pending.Num() > 0 && Processed < MaxCount)
	{
		const FKodCommand Cmd = Pending[0];
		Pending.RemoveAt(0);
		ExecuteCommand(Cmd);
		OnCommandProcessed.Broadcast(Cmd);
		++Processed;
	}
	return Processed;
}

void UKodCommandSubsystem::ExecuteCommand(const FKodCommand& Command)
{
	switch (Command.Type)
	{
	case EKodCommandType::Move:      ExecuteMove(Command); break;
	case EKodCommandType::Attack:    ExecuteAttack(Command); break;
	case EKodCommandType::Stop:      ExecuteStop(Command); break;
	case EKodCommandType::Build:     ExecuteBuild(Command); break;
	case EKodCommandType::CastPower: ExecuteCastPower(Command); break;
	case EKodCommandType::Gather:    ExecuteGather(Command); break;
	default: break;
	}
}

void UKodCommandSubsystem::ExecuteMove(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		AActor* Actor = Sim->ResolveEntity(Id);
		if (!Actor)
		{
			continue;
		}
		if (UKodMoveComponent* Move = Actor->FindComponentByClass<UKodMoveComponent>())
		{
			Move->RequestMoveTo(Command.TargetLocation);
		}
		else
		{
			// Direct sim seek — no NavMesh / AI MoveTo
			Sim->IssueMove(Id, Command.TargetLocation);
		}
	}
}

void UKodCommandSubsystem::ExecuteAttack(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	AActor* Target = Sim->ResolveEntity(Command.TargetEntity);
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		AActor* Actor = Sim->ResolveEntity(Id);
		if (!Actor)
		{
			continue;
		}
		// Hitscan seek-into-range lives on UKodSimSubsystem via AttackComponent.
		// Do NOT also IssueMove — that would overwrite the Attack order.
		if (UKodAttackComponent* Attack = Actor->FindComponentByClass<UKodAttackComponent>())
		{
			Attack->RequestAttack(Target);
		}
		else if (Target)
		{
			Sim->IssueAttack(Id, Command.TargetEntity);
		}
	}
}

void UKodCommandSubsystem::ExecuteStop(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		AActor* Actor = Sim->ResolveEntity(Id);
		if (!Actor)
		{
			continue;
		}
		if (UKodMoveComponent* Move = Actor->FindComponentByClass<UKodMoveComponent>())
		{
			Move->StopMovement();
		}
		if (UKodAttackComponent* Attack = Actor->FindComponentByClass<UKodAttackComponent>())
		{
			Attack->ClearTarget();
		}
	}
}

void UKodCommandSubsystem::ExecuteBuild(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || Command.SourceEntities.Num() == 0)
	{
		return;
	}
	AActor* Building = Sim->ResolveEntity(Command.SourceEntities[0]);
	if (!Building)
	{
		return;
	}
	UKodBuildQueueComponent* Queue = Building->FindComponentByClass<UKodBuildQueueComponent>();
	if (!Queue)
	{
		return;
	}
	// PayloadName expected to resolve to a UKodUnitDefinition asset short name — Content wiring later.
	UKodUnitDefinition* Def = LoadObject<UKodUnitDefinition>(nullptr, *Command.PayloadName.ToString());
	if (Def)
	{
		Queue->Enqueue(Def);
	}
}

void UKodCommandSubsystem::ExecuteCastPower(const FKodCommand& Command)
{
	// M1: general power activation is driven by GAS on the player pawn / power component.
	// Command payload carries ability/tag name; activation hook filled when KodGenerals Content exists.
	(void)Command;
}

void UKodCommandSubsystem::ExecuteGather(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Sim->IsResourceNode(Command.TargetEntity))
	{
		return;
	}
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		Sim->IssueGather(Id, Command.TargetEntity);
	}
}
