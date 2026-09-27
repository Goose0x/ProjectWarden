#include "KodUnitAIController.h"
#include "KodCommandSubsystem.h"
#include "Sim/KodSimSubsystem.h"
#include "Actors/KodUnit.h"

AKodUnitAIController::AKodUnitAIController()
{
	bWantsPlayerState = false;
}

void AKodUnitAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

void AKodUnitAIController::IssueMoveCommand(FVector WorldLocation)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UKodCommandSubsystem* Cmd = World->GetSubsystem<UKodCommandSubsystem>();
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	AKodUnit* Unit = Cast<AKodUnit>(GetPawn());
	if (!Cmd || !Sim || !Unit)
	{
		return;
	}
	FKodCommand C;
	C.Type = EKodCommandType::Move;
	C.SourceEntities.Add(Unit->EntityId.IsValid() ? Unit->EntityId : Sim->FindIdForActor(Unit));
	C.TargetLocation = WorldLocation;
	Cmd->Enqueue(C);
}

void AKodUnitAIController::IssueAttackCommand(AActor* Target)
{
	UWorld* World = GetWorld();
	if (!World || !Target)
	{
		return;
	}
	UKodCommandSubsystem* Cmd = World->GetSubsystem<UKodCommandSubsystem>();
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	AKodUnit* Unit = Cast<AKodUnit>(GetPawn());
	if (!Cmd || !Sim || !Unit)
	{
		return;
	}
	FKodCommand C;
	C.Type = EKodCommandType::Attack;
	C.SourceEntities.Add(Unit->EntityId.IsValid() ? Unit->EntityId : Sim->FindIdForActor(Unit));
	C.TargetEntity = Sim->FindIdForActor(Target);
	C.TargetLocation = Target->GetActorLocation();
	Cmd->Enqueue(C);
}
