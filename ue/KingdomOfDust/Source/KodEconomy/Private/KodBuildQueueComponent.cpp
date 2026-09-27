#include "KodBuildQueueComponent.h"
#include "Data/KodUnitDefinition.h"
#include "Actors/KodUnit.h"
#include "Sim/KodBuildTicks.h"
#include "Engine/World.h"

UKodBuildQueueComponent::UKodBuildQueueComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UKodBuildQueueComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsBusy())
	{
		return;
	}
	ProgressSeconds += DeltaTime;
	if (ProgressSeconds >= RequiredSeconds)
	{
		CompleteCurrent();
	}
}

bool UKodBuildQueueComponent::Enqueue(UKodUnitDefinition* Def)
{
	if (IsBusy() || !Def)
	{
		return false;
	}
	QueuedUnit = Def;
	const int32 Ticks = Def->GetResolvedBuildTicks();
	RequiredSeconds = KodBuildTicks::BuildTicksToSeconds(Ticks);
	ProgressSeconds = 0.f;
	return true;
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
	UKodUnitDefinition* Def = QueuedUnit.LoadSynchronous();
	Cancel();
	if (!Def || !GetWorld())
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AKodUnit* Unit = GetWorld()->SpawnActor<AKodUnit>(AKodUnit::StaticClass(), GetSpawnTransform(), Params);
	if (Unit)
	{
		Unit->Definition = Def;
		Unit->ApplyDefinition(Def);
	}
}
