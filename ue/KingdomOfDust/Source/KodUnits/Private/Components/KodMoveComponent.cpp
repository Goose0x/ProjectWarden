#include "Components/KodMoveComponent.h"
#include "Sim/KodSimSubsystem.h"
#include "Sim/KodEntityId.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UKodMoveComponent::UKodMoveComponent()
{
	// Presentation tick optional later; sim owns integration.
	PrimaryComponentTick.bCanEverTick = false;
}

bool UKodMoveComponent::RequestMoveTo(FVector WorldLocation)
{
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Owner)
	{
		return false;
	}

	FKodEntityId Id = Sim->FindIdForActor(Owner);
	if (!Id.IsValid())
	{
		return false;
	}

	// NavMesh FORBIDDEN this slice — direct sim seek only.
	Sim->IssueMove(Id, WorldLocation);
	return true;
}

void UKodMoveComponent::StopMovement()
{
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Owner)
	{
		return;
	}
	FKodEntityId Id = Sim->FindIdForActor(Owner);
	if (Id.IsValid())
	{
		Sim->IssueStop(Id);
	}
}

bool UKodMoveComponent::IsMoveActive() const
{
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Owner)
	{
		return false;
	}
	FKodEntityId Id = Sim->FindIdForActor(Owner);
	FKodSimEntityState State;
	if (Sim->TryGetState(Id, State))
	{
		return State.IsMoving();
	}
	return false;
}
