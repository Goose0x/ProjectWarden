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
	case EKodCommandType::Gather:      ExecuteGather(Command); break;
	case EKodCommandType::Train:       ExecuteTrain(Command); break;
	case EKodCommandType::CancelTrain: ExecuteCancelTrain(Command); break;
	case EKodCommandType::SetRally:    ExecuteSetRally(Command); break;
	case EKodCommandType::PlaceBuild:  ExecutePlaceBuild(Command); break;
	case EKodCommandType::ResumeBuild: ExecuteResumeBuild(Command); break;
	case EKodCommandType::CancelBuild: ExecuteCancelBuild(Command); break;
	case EKodCommandType::UseAbility:  ExecuteUseAbility(Command); break;
	case EKodCommandType::AttackMove: ExecuteAttackMove(Command); break;
	case EKodCommandType::Hold: ExecuteHold(Command); break;
	case EKodCommandType::Patrol: ExecutePatrol(Command); break;
	case EKodCommandType::Follow: ExecuteFollow(Command); break;
	case EKodCommandType::Guard: ExecuteGuard(Command); break;
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
	IssueGroundOrders(Command, EKodSimOrderType::Move);
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
	const FVector Point = Target ? Target->GetActorLocation() : Command.TargetLocation;
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		Sim->IssueOrder(Id, EKodSimOrderType::Attack, Point, Command.TargetEntity, Command.bQueued);
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
		Sim->IssueStop(Id);
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

void UKodCommandSubsystem::ExecuteTrain(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		Sim->EnqueueTrain(Id, Command.PayloadName);
	}
}

void UKodCommandSubsystem::ExecuteCancelTrain(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		if (Command.PayloadIndex >= 0)
		{
			Sim->CancelTrainAt(Id, Command.PayloadIndex);
		}
		else
		{
			Sim->CancelLastTrain(Id);
		}
	}
}

void UKodCommandSubsystem::ExecuteSetRally(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		Sim->SetStructureRally(Id, Command.TargetLocation, Command.TargetEntity);
	}
}

void UKodCommandSubsystem::ExecutePlaceBuild(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || Command.SourceEntities.Num() <= 0)
	{
		return;
	}
	int32 TeamId = 0;
	FKodSimEntityState WorkerState;
	if (Sim->TryGetState(Command.SourceEntities[0], WorkerState))
	{
		TeamId = WorkerState.TeamId;
	}
	Sim->TryPlaceStructure(TeamId, Command.PayloadName, Command.TargetLocation, Command.SourceEntities[0]);
}

void UKodCommandSubsystem::ExecuteResumeBuild(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || Command.SourceEntities.Num() <= 0)
	{
		return;
	}
	Sim->ResumeConstruction(Command.TargetEntity, Command.SourceEntities[0]);
}

void UKodCommandSubsystem::ExecuteCancelBuild(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		Sim->CancelConstruction(Id);
	}
}

void UKodCommandSubsystem::ExecuteUseAbility(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		Sim->UseAbility(Id, Command.PayloadName);
	}
}

void UKodCommandSubsystem::ExecuteAttackMove(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	IssueGroundOrders(Command, EKodSimOrderType::AttackMove);
}

void UKodCommandSubsystem::IssueGroundOrders(const FKodCommand& Command, EKodSimOrderType Kind)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	int32 SharedPath = 0;
	if (Command.SourceEntities.Num() > 1)
	{
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const FKodEntityId& Id : Command.SourceEntities)
		{
			FKodSimEntityState State;
			if (Sim->TryGetState(Id, State))
			{
				Sum += State.Position;
				++Count;
			}
		}
		if (Count > 0)
		{
			SharedPath = Sim->PlanMovePath(Sum / static_cast<float>(Count), Command.TargetLocation);
		}
	}
	const int32 Total = Command.SourceEntities.Num();
	int32 Slot = 0;
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		const int32 Offset = (Slot - (Total - 1) / 2) * 80;
		Sim->IssueOrder(Id, Kind, Command.TargetLocation, FKodEntityId(), Command.bQueued, SharedPath, 0, Offset);
		++Slot;
	}
}

void UKodCommandSubsystem::ExecuteHold(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		Sim->IssueOrder(Id, EKodSimOrderType::Hold, FVector::ZeroVector, FKodEntityId(), Command.bQueued);
	}
}

void UKodCommandSubsystem::ExecutePatrol(const FKodCommand& Command)
{
	IssueGroundOrders(Command, EKodSimOrderType::Patrol);
}

void UKodCommandSubsystem::ExecuteFollow(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		Sim->IssueOrder(Id, EKodSimOrderType::Follow, Command.TargetLocation, Command.TargetEntity, Command.bQueued);
	}
}

void UKodCommandSubsystem::ExecuteGuard(const FKodCommand& Command)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : Command.SourceEntities)
	{
		Sim->IssueOrder(Id, EKodSimOrderType::Guard, Command.TargetLocation, Command.TargetEntity, Command.bQueued);
	}
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
		if (Command.bQueued)
		{
			Sim->IssueOrder(Id, EKodSimOrderType::Gather, Command.TargetLocation, Command.TargetEntity, true);
		}
		else
		{
			Sim->IssueGather(Id, Command.TargetEntity);
		}
	}
}
