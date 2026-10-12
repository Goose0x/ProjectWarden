#include "KodBuildQueueComponent.h"

#include "Data/KodUnitDefinition.h"
#include "GameFramework/Actor.h"

UKodBuildQueueComponent::UKodBuildQueueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UKodBuildQueueComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool UKodBuildQueueComponent::Enqueue(UKodUnitDefinition* Def)
{
	(void)Def;
	UE_LOG(LogTemp, Log, TEXT("KodProd QueueStub Retired"));
	return false;
}

void UKodBuildQueueComponent::Cancel()
{
	QueuedUnit.Reset();
	ProgressSeconds = 0.f;
	RequiredSeconds = 0.f;
}

FTransform UKodBuildQueueComponent::GetSpawnTransform() const
{
	FTransform T = GetOwner() ? GetOwner()->GetActorTransform() : FTransform::Identity;
	T.AddToTranslation(T.GetRotation().GetForwardVector() * SpawnForwardOffset);
	return T;
}

void UKodBuildQueueComponent::CompleteCurrent()
{
	Cancel();
}
