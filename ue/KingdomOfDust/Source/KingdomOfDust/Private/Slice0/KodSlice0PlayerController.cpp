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
	if (Sim->IsResourceNode(TargetId))
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
