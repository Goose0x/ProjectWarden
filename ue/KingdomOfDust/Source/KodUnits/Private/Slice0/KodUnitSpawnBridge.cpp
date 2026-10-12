#include "Slice0/KodUnitSpawnBridge.h"

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
	if (Def->bCanGather)
	{
		Unit->Tags.AddUnique(FName(TEXT("KodUnitDozer")));
	}
	Unit->ApplyDefinition(Def);
	UE_LOG(LogTemp, Log, TEXT("KodProd Complete Spawned=%s"), *Unit->GetName());

	if (Request.RallyUnit.IsValid())
	{
		const FVector At = Unit->GetActorLocation();
		Sim->IssueOrder(Unit->EntityId, EKodSimOrderType::Follow, At, Request.RallyUnit, false);
	}
	else if (Request.bRallyNode && Request.RallyNode.IsValid())
	{
		Sim->IssueGather(Unit->EntityId, Request.RallyNode);
	}
	else if (!Request.RallyPoint.IsNearlyZero())
	{
		Sim->IssueMove(Unit->EntityId, Request.RallyPoint);
	}
}
