#include "Slice0/KodUnitSpawnBridge.h"

#include "Actors/KodMeshAttach.h"
#include "Actors/KodUnit.h"
#include "Data/KodUnitDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Sim/KodSimSubsystem.h"
#include "Slice0/KodSlice0Bootstrap.h"

void UKodUnitSpawnBridge::Tick(float DeltaTime)
{
	(void)DeltaTime;
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	TArray<FKodPendingUnitSpawn> Spawns;
	Sim->ConsumePendingSpawns(Spawns);
	for (const FKodPendingUnitSpawn& Request : Spawns)
	{
		SpawnOne(Request);
	}
}

TStatId UKodUnitSpawnBridge::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKodUnitSpawnBridge, STATGROUP_Tickables);
}

void UKodUnitSpawnBridge::SpawnOne(const FKodPendingUnitSpawn& Request)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodProd Complete Spawned=None"));
		return;
	}

	UKodUnitDefinition* Def = UKodSlice0Bootstrap::ResolveUnit(Request.UnitId, this);
	if (!Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodProd Complete Spawned=None"));
		return;
	}

	const FTransform Xform(FRotator(0.f, Request.Yaw, 0.f), Request.Location);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AKodUnit* Unit = World->SpawnActor<AKodUnit>(AKodUnit::StaticClass(), Xform, Params);
	if (!Unit)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodProd Complete Spawned=None"));
		return;
	}

	Unit->Definition = Def;
	Unit->TeamId = Request.TeamId;
	Unit->bSupplyPrepaid = true;
	if (!Request.ActorTag.IsNone())
	{
		Unit->Tags.AddUnique(Request.ActorTag);
	}
	if (Def->bCanGather)
	{
		Unit->Tags.AddUnique(FName(TEXT("KodUnitDozer")));
	}
	FKodSimEntityState Kept;
	const bool bRestore = Request.RestoreId > 0;
	if (bRestore)
	{
		FKodEntityId Existing;
		Existing.Value = Request.RestoreId;
		Sim->TryGetState(Existing, Kept);
		Unit->EntityId = Existing;
	}
	Unit->ApplyDefinition(Def);
	if (Request.GatherRatePermille > 0 || Request.OrbitTicks > 0)
	{
		Sim->FinishOrbitalSpawn(Unit->EntityId, Request);
	}
	if (bRestore)
	{
		Sim->RestoreImportedState(Unit->EntityId, Kept);
		Sim->AttachRestoredActor(Unit->EntityId, Unit);
		UE_LOG(LogTemp, Log, TEXT("KodLoad Spawned=%s Id=%d"), *Unit->GetName(), Unit->EntityId.Value);
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("KodProd Complete Spawned=%s"), *Unit->GetName());
	const bool bTrainExit = Request.RestoreId == 0
		&& Request.ScriptRole == 0
		&& Request.OrbitTicks == 0
		&& Request.GatherRatePermille == 0
		&& Request.StructureId != 0;
	if (bTrainExit)
	{
		FKodEntityId StructureId;
		StructureId.Value = Request.StructureId;
		if (AActor* Hall = Sim->ResolveEntity(StructureId))
		{
			if (UKodMeshAttachComponent* Attach = Hall->FindComponentByClass<UKodMeshAttachComponent>())
			{
				Attach->NotifyTrainExit();
				FVector ExitAt = FVector::ZeroVector;
				if (Attach->TryGetExitPoint(ExitAt))
				{
					Unit->BeginBayExitVisual(ExitAt);
				}
			}
		}
	}
	if (Request.ScriptRole != 0)
	{
		Sim->DeferScriptAdopt(Unit->EntityId, Request);
	}

	if (Request.RallyUnit.IsValid())
	{
		const FVector At = Unit->GetActorLocation();
		Sim->DeferOrder(Unit->EntityId, EKodSimOrderType::Follow, At, Request.RallyUnit, false);
	}
	else if (Request.bRallyNode && Request.RallyNode.IsValid())
	{
		Sim->DeferGather(Unit->EntityId, Request.RallyNode);
	}
	else if (Request.bAttackMove)
	{
		Sim->DeferAttackMove(Unit->EntityId, Request.RallyPoint);
	}
	else if (!Request.RallyPoint.IsNearlyZero())
	{
		Sim->DeferMove(Unit->EntityId, Request.RallyPoint);
	}
}
