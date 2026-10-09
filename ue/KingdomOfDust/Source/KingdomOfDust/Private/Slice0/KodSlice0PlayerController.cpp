#include "Slice0/KodSlice0PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "KodCommandSubsystem.h"
#include "KodCommandTypes.h"
#include "Sim/KodSimSubsystem.h"

AKodSlice0PlayerController::AKodSlice0PlayerController()
{
}

void AKodSlice0PlayerController::IssueMoveToSelection_Implementation(FVector WorldLocation)
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	if (!Commands)
	{
		Super::IssueMoveToSelection_Implementation(WorldLocation);
		return;
	}

	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::Move;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.SourceEntities = GetLocalSelectedEntityIds();
	Cmd.TargetLocation = WorldLocation;
	UE_LOG(LogTemp, Log, TEXT("Move Issued Sources=%d Dest=%.0f,%.0f,%.0f"),
		Cmd.SourceEntities.Num(),
		WorldLocation.X,
		WorldLocation.Y,
		WorldLocation.Z);
	if (Cmd.SourceEntities.Num() > 0)
	{
		Commands->Enqueue(Cmd);
	}
}

void AKodSlice0PlayerController::IssueAttackToSelection_Implementation(AActor* Target)
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim || !Target)
	{
		Super::IssueAttackToSelection_Implementation(Target);
		return;
	}

	const FKodEntityId TargetId = Sim->FindIdForActor(Target);
	if (!TargetId.IsValid())
	{
		// Do not Move to the actor pivot. RMB already Moves to ImpactPoint for non-sim hits.
		UE_LOG(LogTemp, Log, TEXT("Attack Reject NonSim Target=%s"), *Target->GetName());
		return;
	}
	if (Sim->IsResourceNode(TargetId) || Sim->IsDropOff(TargetId))
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Reject Node Target=%s Id=%d"), *Target->GetName(), TargetId.Value);
		return;
	}

	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::Attack;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.SourceEntities = GetLocalSelectedEntityIds();
	Cmd.SourceEntities.Remove(TargetId);
	Cmd.TargetEntity = TargetId;
	Cmd.TargetLocation = Target->GetActorLocation();
	if (Cmd.SourceEntities.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Reject Self Target=%s Id=%d"), *Target->GetName(), TargetId.Value);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("Attack Issued Sources=%d Target=%s Id=%d"),
		Cmd.SourceEntities.Num(),
		*Target->GetName(),
		TargetId.Value);
	Commands->Enqueue(Cmd);
}

void AKodSlice0PlayerController::IssueGatherToSelection_Implementation(AActor* Node, FVector NonGathererMovePoint)
{
	UWorld* World = GetWorld();
	UKodCommandSubsystem* Commands = World ? World->GetSubsystem<UKodCommandSubsystem>() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Commands || !Sim || !Node)
	{
		Super::IssueGatherToSelection_Implementation(Node, NonGathererMovePoint);
		return;
	}

	const FKodEntityId NodeId = Sim->FindIdForActor(Node);
	if (!Sim->IsResourceNode(NodeId))
	{
		UE_LOG(LogTemp, Log, TEXT("Gather Reject NonNode Target=%s"), *Node->GetName());
		return;
	}

	FKodCommand GatherCmd;
	GatherCmd.Type = EKodCommandType::Gather;
	GatherCmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	GatherCmd.TargetEntity = NodeId;
	GatherCmd.TargetLocation = Node->GetActorLocation();

	FKodCommand MoveCmd;
	MoveCmd.Type = EKodCommandType::Move;
	MoveCmd.IssuerPlayerId = GatherCmd.IssuerPlayerId;
	MoveCmd.TargetLocation = NonGathererMovePoint;

	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		FKodSimEntityState State;
		if (!Sim->TryGetState(Id, State))
		{
			continue;
		}
		if (State.bCanGather)
		{
			GatherCmd.SourceEntities.Add(Id);
		}
		else
		{
			MoveCmd.SourceEntities.Add(Id);
		}
	}

	if (GatherCmd.SourceEntities.Num() == 0)
	{
		Super::IssueGatherToSelection_Implementation(Node, NonGathererMovePoint);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Gather Issued Sources=%d Node=%d"), GatherCmd.SourceEntities.Num(), NodeId.Value);
	Commands->Enqueue(GatherCmd);
	if (MoveCmd.SourceEntities.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Move Issued Sources=%d Dest=%.0f,%.0f,%.0f"),
			MoveCmd.SourceEntities.Num(),
			NonGathererMovePoint.X,
			NonGathererMovePoint.Y,
			NonGathererMovePoint.Z);
		Commands->Enqueue(MoveCmd);
	}
}
