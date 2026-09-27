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
		IssueMoveToSelection(Target->GetActorLocation());
		return;
	}

	FKodCommand Cmd;
	Cmd.Type = EKodCommandType::Attack;
	Cmd.IssuerPlayerId = GetLocalPlayer() ? GetLocalPlayer()->GetControllerId() : 0;
	Cmd.SourceEntities = GetLocalSelectedEntityIds();
	Cmd.TargetEntity = TargetId;
	Cmd.TargetLocation = Target->GetActorLocation();
	if (Cmd.SourceEntities.Num() > 0)
	{
		Commands->Enqueue(Cmd);
	}
}
