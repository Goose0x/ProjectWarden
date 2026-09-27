#include "KodControlGroupStore.h"
#include "KodSelectionManager.h"
#include "Sim/KodSimSubsystem.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"

void UKodControlGroupStore::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	for (int32 i = 0; i < NumGroups; ++i)
	{
		Groups[i].Reset();
	}
}

void UKodControlGroupStore::AssignGroup(int32 GroupIndex, const TArray<FKodEntityId>& Entities)
{
	if (GroupIndex < 0 || GroupIndex >= NumGroups)
	{
		return;
	}
	Groups[GroupIndex] = Entities;
}

void UKodControlGroupStore::AddToGroup(int32 GroupIndex, const TArray<FKodEntityId>& Entities)
{
	if (GroupIndex < 0 || GroupIndex >= NumGroups)
	{
		return;
	}
	for (const FKodEntityId& Id : Entities)
	{
		Groups[GroupIndex].AddUnique(Id);
	}
}

TArray<FKodEntityId> UKodControlGroupStore::GetGroup(int32 GroupIndex) const
{
	if (GroupIndex < 0 || GroupIndex >= NumGroups)
	{
		return {};
	}
	return Groups[GroupIndex];
}

void UKodControlGroupStore::ClearGroup(int32 GroupIndex)
{
	if (GroupIndex < 0 || GroupIndex >= NumGroups)
	{
		return;
	}
	Groups[GroupIndex].Reset();
}

void UKodControlGroupStore::RecallGroup(int32 GroupIndex, bool bAddToSelection)
{
	if (GroupIndex < 0 || GroupIndex >= NumGroups)
	{
		return;
	}
	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		return;
	}
	UKodSelectionManager* Sel = LP->GetSubsystem<UKodSelectionManager>();
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sel || !Sim)
	{
		return;
	}
	TArray<AActor*> Actors;
	for (const FKodEntityId& Id : Groups[GroupIndex])
	{
		if (AActor* A = Sim->ResolveEntity(Id))
		{
			Actors.Add(A);
		}
	}
	Sel->SelectActors(Actors, bAddToSelection);
}
