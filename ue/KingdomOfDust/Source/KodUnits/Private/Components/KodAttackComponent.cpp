#include "Components/KodAttackComponent.h"
#include "Sim/KodSimSubsystem.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UKodAttackComponent::UKodAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UKodAttackComponent::RequestAttack(AActor* Target)
{
	CurrentTarget = Target;
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Owner || !Target)
	{
		return Target != nullptr;
	}

	const FKodEntityId SelfId = Sim->FindIdForActor(Owner);
	const FKodEntityId TargetId = Sim->FindIdForActor(Target);
	if (!SelfId.IsValid() || !TargetId.IsValid())
	{
		return false;
	}

	Sim->IssueAttack(SelfId, TargetId);
	return true;
}

void UKodAttackComponent::ClearTarget()
{
	CurrentTarget = nullptr;
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Owner)
	{
		return;
	}
	const FKodEntityId SelfId = Sim->FindIdForActor(Owner);
	if (SelfId.IsValid())
	{
		Sim->IssueStop(SelfId);
	}
}
