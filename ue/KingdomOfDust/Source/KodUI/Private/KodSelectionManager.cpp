#include "KodSelectionManager.h"
#include "Sim/KodSimSubsystem.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"

void UKodSelectionManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ClearSelection();
}

void UKodSelectionManager::ClearSelection()
{
	SelectedActors.Reset();
	OnSelectionChanged.Broadcast();
}

void UKodSelectionManager::SelectActor(AActor* Actor, bool bAddToSelection)
{
	if (!bAddToSelection)
	{
		SelectedActors.Reset();
	}
	if (Actor)
	{
		SelectedActors.AddUnique(Actor);
	}
	OnSelectionChanged.Broadcast();
}

void UKodSelectionManager::SelectActors(const TArray<AActor*>& Actors, bool bAddToSelection)
{
	if (!bAddToSelection)
	{
		SelectedActors.Reset();
	}
	for (AActor* A : Actors)
	{
		if (A)
		{
			SelectedActors.AddUnique(A);
		}
	}
	OnSelectionChanged.Broadcast();
}

void UKodSelectionManager::DeselectActor(AActor* Actor)
{
	SelectedActors.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Ptr) { return Ptr.Get() == Actor; });
	OnSelectionChanged.Broadcast();
}

TArray<AActor*> UKodSelectionManager::GetSelectedActors() const
{
	TArray<AActor*> Actors;
	Actors.Reserve(SelectedActors.Num());
	for (const TWeakObjectPtr<AActor>& Ptr : SelectedActors)
	{
		if (AActor* Actor = Ptr.Get())
		{
			Actors.Add(Actor);
		}
	}
	return Actors;
}

TArray<FKodEntityId> UKodSelectionManager::GetSelectedEntityIds() const
{
	TArray<FKodEntityId> Ids;
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return Ids;
	}
	for (const TWeakObjectPtr<AActor>& Ptr : SelectedActors)
	{
		if (AActor* A = Ptr.Get())
		{
			FKodEntityId Id = Sim->FindIdForActor(A);
			if (Id.IsValid())
			{
				Ids.Add(Id);
			}
		}
	}
	return Ids;
}

void UKodSelectionManager::BeginBoxSelect(FVector2D ScreenPos)
{
	bBoxSelecting = true;
	BoxStart = ScreenPos;
	BoxEnd = ScreenPos;
}

void UKodSelectionManager::UpdateBoxSelect(FVector2D ScreenPos)
{
	if (bBoxSelecting)
	{
		BoxEnd = ScreenPos;
	}
}

void UKodSelectionManager::EndBoxSelect(bool bAddToSelection)
{
	bBoxSelecting = false;
	(void)bAddToSelection;
	// M1: frustum / HUD box hit-test against selectable actors — implement with PC deproject
	OnSelectionChanged.Broadcast();
}
