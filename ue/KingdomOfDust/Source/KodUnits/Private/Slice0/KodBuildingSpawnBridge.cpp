#include "Slice0/KodBuildingSpawnBridge.h"

#include "Actors/KodBuilding.h"
#include "Data/KodBuildingDefinition.h"
#include "Engine/World.h"
#include "Sim/KodSimSubsystem.h"
#include "Sim/KodEntityId.h"
#include "Slice0/KodSlice0Bootstrap.h"

void UKodBuildingSpawnBridge::Tick(float DeltaTime)
{
	(void)DeltaTime;
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return;
	}
	TArray<FKodPendingBuildingSpawn> Spawns;
	Sim->ConsumePendingBuildingSpawns(Spawns);
	for (const FKodPendingBuildingSpawn& Request : Spawns)
	{
		FKodEntityId StructureId;
		StructureId.Value = Request.StructureId;
		if (!Sim->IsStructure(StructureId))
		{
			continue;
		}
		UKodBuildingDefinition* Def = UKodSlice0Bootstrap::ResolveBuilding(Request.TypeId, this);
		const FTransform Xform(FRotator(0.f, static_cast<float>(Request.YawDegrees), 0.f), Request.Location);
		AKodBuilding* Building = World->SpawnActorDeferred<AKodBuilding>(
			AKodBuilding::StaticClass(),
			Xform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Building)
		{
			continue;
		}
		Building->EntityId.Value = Request.StructureId;
		Building->TeamId = Request.TeamId;
		if (Def)
		{
			Building->Definition = Def;
		}
		FKodSimEntityState Kept;
		const bool bRestore = Request.RestoreId > 0;
		if (bRestore)
		{
			FKodEntityId Existing;
			Existing.Value = Request.StructureId;
			Sim->TryGetState(Existing, Kept);
		}
		Building->FinishSpawning(Xform);
		if (bRestore)
		{
			Sim->RestoreImportedState(Building->EntityId, Kept);
			Sim->AttachRestoredActor(Building->EntityId, Building);
			UE_LOG(LogTemp, Log, TEXT("KodLoad Spawned=%s Id=%d"), *Building->GetName(), Building->EntityId.Value);
		}
	}
}

TStatId UKodBuildingSpawnBridge::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKodBuildingSpawnBridge, STATGROUP_Tickables);
}
