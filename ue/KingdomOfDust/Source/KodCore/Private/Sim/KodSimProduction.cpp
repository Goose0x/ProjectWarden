#include "Sim/KodSimSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
	void LogSupply(int32 TeamId, const FKodSupplyState& Supply)
	{
		UE_LOG(LogTemp, Log, TEXT("KodEcon Supply Team=%d Used=%d Cap=%d"), TeamId, Supply.Used, Supply.Cap);
	}
}

void UKodSimSubsystem::RegisterTrainSpec(const FKodTrainSpec& Spec)
{
	if (Spec.UnitId.IsNone())
	{
		return;
	}
	TrainSpecs.Add(Spec.UnitId, Spec);
}

bool UKodSimSubsystem::TryGetSupply(int32 TeamId, int32& OutUsed, int32& OutCap) const
{
	OutUsed = 0;
	OutCap = 0;
	if (const FKodSupplyState* Found = Supply.Find(TeamId))
	{
		OutUsed = Found->Used;
		OutCap = Found->Cap;
		return true;
	}
	return false;
}

void UKodSimSubsystem::AddSupplyUsed(int32 TeamId, int32 Delta)
{
	if (Delta == 0)
	{
		return;
	}
	FKodSupplyState& Row = Supply.FindOrAdd(TeamId);
	const int64 Next = static_cast<int64>(Row.Used) + static_cast<int64>(Delta);
	Row.Used = static_cast<int32>(FMath::Clamp(Next, static_cast<int64>(0), static_cast<int64>(MAX_int32)));
	LogSupply(TeamId, Row);
}

void UKodSimSubsystem::RecalcSupplyCap(int32 TeamId)
{
	int32 Sum = 0;
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (Structure && Structure->TeamId == TeamId)
		{
			Sum += FMath::Max(0, Structure->SupplyProvided);
		}
	}
	Sum = FMath::Min(Sum, KodEconomyDefaults::SupplyHardCap);
	FKodSupplyState& Row = Supply.FindOrAdd(TeamId);
	if (Row.Cap == Sum)
	{
		return;
	}
	Row.Cap = Sum;
	LogSupply(TeamId, Row);
}

void UKodSimSubsystem::AccountUnitSupply(FKodEntityId Id, int32 Cost, bool bAlreadyPaid)
{
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State || State->bSupplyCounted)
	{
		return;
	}
	State->SupplyCost = FMath::Max(0, Cost);
	State->bSupplyCounted = true;
	if (!bAlreadyPaid && State->SupplyCost > 0)
	{
		AddSupplyUsed(State->TeamId, State->SupplyCost);
	}
}

void UKodSimSubsystem::ReleaseCountedSupply(FKodEntityId Id)
{
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State || !State->bSupplyCounted || State->SupplyCost <= 0)
	{
		return;
	}
	const int32 Cost = State->SupplyCost;
	const int32 TeamId = State->TeamId;
	State->bSupplyCounted = false;
	State->SupplyCost = 0;
	AddSupplyUsed(TeamId, -Cost);
}

FKodEntityId UKodSimSubsystem::RegisterStructure(AActor* Actor, const FKodStructureSpec& Spec)
{
	FKodEntityId Id;
	if (!Actor)
	{
		return Id;
	}

	Id = FindIdForActor(Actor);
	if (Id.IsValid() && States.Contains(Id.Value))
	{
		return FKodEntityId();
	}
	if (!Id.IsValid())
	{
		Id.Value = NextId++;
		Entities.Add(Id.Value, Actor);
	}

	FKodStructureState& Structure = Structures.FindOrAdd(Id.Value);
	Structure.Id = Id;
	Structure.TeamId = Spec.TeamId;
	Structure.DefinitionId = Spec.DefinitionId;
	Structure.Position = Actor->GetActorLocation();
	QuantizeResourcePosition(Structure.Position);
	FVector Forward = Actor->GetActorForwardVector();
	Forward.Z = 0.f;
	if (Forward.SizeSquared() < 0.01f)
	{
		Forward = FVector(1.f, 0.f, 0.f);
	}
	Forward.Normalize();
	Structure.ExitPoint = Structure.Position + Forward * 320.f;
	QuantizeResourcePosition(Structure.ExitPoint);
	Structure.SupplyProvided = FMath::Max(0, Spec.SupplyProvided);
	Structure.bIsDropOff = Spec.bIsDropOff;
	if (Structure.RallyPoint.IsNearlyZero())
	{
		Structure.RallyPoint = Structure.ExitPoint;
	}

	if (Spec.bIsDropOff && !DropOffs.Contains(Id.Value))
	{
		FKodDropOffState Drop;
		Drop.Id = Id;
		Drop.TeamId = Spec.TeamId;
		Drop.Position = Structure.Position;
		Drop.StandUU = FMath::Max(1, Spec.DropOffStandUU);
		DropOffs.Add(Id.Value, Drop);
		UE_LOG(LogTemp, Log, TEXT("KodEcon DropOff Id=%d Team=%d Loc=%.0f,%.0f,%.0f"),
			Id.Value,
			Spec.TeamId,
			Drop.Position.X,
			Drop.Position.Y,
			Drop.Position.Z);
	}

	RecalcSupplyCap(Spec.TeamId);
	UE_LOG(LogTemp, Log, TEXT("KodProd Structure Id=%d Team=%d Supply=%d"),
		Id.Value,
		Spec.TeamId,
		Structure.SupplyProvided);
	return Id;
}

bool UKodSimSubsystem::IsStructure(FKodEntityId Id) const
{
	return Id.IsValid() && Structures.Contains(Id.Value);
}

bool UKodSimSubsystem::TryGetStructure(FKodEntityId Id, FKodStructureState& OutState) const
{
	if (const FKodStructureState* Found = Structures.Find(Id.Value))
	{
		OutState = *Found;
		return true;
	}
	return false;
}

void UKodSimSubsystem::TryStartProduction(FKodStructureState& Structure)
{
	if (Structure.Slots.Num() <= 0)
	{
		return;
	}
	FKodProductionSlot& Slot = Structure.Slots[0];
	const FKodSupplyState& Row = Supply.FindOrAdd(Structure.TeamId);
	if (Row.Used + FMath::Max(0, Slot.Supply) > Row.Cap)
	{
		if (!Structure.bSupplyBlocked)
		{
			Structure.bSupplyBlocked = true;
			UE_LOG(LogTemp, Log, TEXT("KodProd SupplyBlocked Used=%d Cap=%d"), Row.Used, Row.Cap);
		}
		return;
	}
	Structure.bSupplyBlocked = false;
	if (Slot.bStarted)
	{
		return;
	}
	Slot.bStarted = true;
	Structure.ActiveBuildTime = FMath::Max(1, Slot.BuildTime);
	Structure.ActiveTicksRemaining = Structure.ActiveBuildTime;
	UE_LOG(LogTemp, Log, TEXT("KodProd Start Struct=%d Unit=%s"),
		Structure.Id.Value,
		Slot.DisplayId.IsNone() ? *Slot.UnitId.ToString() : *Slot.DisplayId.ToString());
}

bool UKodSimSubsystem::EnqueueTrain(FKodEntityId StructureId, FName UnitId)
{
	FKodStructureState* Structure = Structures.Find(StructureId.Value);
	const FKodTrainSpec* Spec = TrainSpecs.Find(UnitId);
	if (!Structure || !Spec)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodProd Enqueue Miss Struct=%d Unit=%s"), StructureId.Value, *UnitId.ToString());
		return false;
	}
	if (Structure->Slots.Num() >= KodEconomyDefaults::ProductionSlotCap)
	{
		UE_LOG(LogTemp, Log, TEXT("KodProd Enqueue Reject Struct=%d Reason=Full"), StructureId.Value);
		return false;
	}

	FKodResourceCost Price;
	Price.Jadeite = FMath::Max(0, Spec->Cost.Credits);
	Price.Luminene = FMath::Max(0, Spec->Cost.Luminene);
	if (!Spend(Structure->TeamId, Price))
	{
		return false;
	}

	FKodProductionSlot Slot;
	Slot.UnitId = Spec->UnitId;
	Slot.DisplayId = Spec->DisplayId.IsNone() ? Spec->UnitId : Spec->DisplayId;
	Slot.Credits = Price.Jadeite;
	Slot.Luminene = Price.Luminene;
	Slot.Supply = FMath::Max(0, Spec->Cost.Supply);
	Slot.BuildTime = FMath::Max(1, Spec->Cost.BuildTime);
	Structure->Slots.Add(Slot);
	UE_LOG(LogTemp, Log, TEXT("KodProd Enqueue Struct=%d Unit=%s Slot=%d"),
		StructureId.Value,
		*Slot.DisplayId.ToString(),
		Structure->Slots.Num() - 1);

	if (Structure->Slots.Num() == 1)
	{
		TryStartProduction(*Structure);
	}
	return true;
}

bool UKodSimSubsystem::CancelLastTrain(FKodEntityId StructureId)
{
	FKodStructureState* Structure = Structures.Find(StructureId.Value);
	if (!Structure || Structure->Slots.Num() <= 0)
	{
		return false;
	}
	const bool bWasActive = Structure->Slots.Num() == 1;
	const FKodProductionSlot Slot = Structure->Slots.Last();
	Structure->Slots.Pop();
	if (bWasActive)
	{
		Structure->bSupplyBlocked = false;
		Structure->ActiveTicksRemaining = 0;
		Structure->ActiveBuildTime = 0;
	}

	FKodResourceCost Refund;
	Refund.Jadeite = Slot.Credits;
	Refund.Luminene = Slot.Luminene;
	if (Refund.Jadeite > 0 || Refund.Luminene > 0)
	{
		Deposit(Structure->TeamId, Refund);
	}
	UE_LOG(LogTemp, Log, TEXT("KodProd Cancel Refund=C%d,L%d"), Refund.Jadeite, Refund.Luminene);

	if (Structure->Slots.Num() > 0 && !Structure->Slots[0].bStarted)
	{
		TryStartProduction(*Structure);
	}
	return true;
}

void UKodSimSubsystem::SetStructureRally(FKodEntityId StructureId, FVector GroundPoint, FKodEntityId NodeId)
{
	FKodStructureState* Structure = Structures.Find(StructureId.Value);
	if (!Structure)
	{
		return;
	}
	const bool bNode = NodeId.IsValid() && IsResourceNode(NodeId);
	Structure->bRallyNode = bNode;
	Structure->RallyNode = bNode ? NodeId : FKodEntityId();
	Structure->RallyPoint = GroundPoint;
	QuantizeResourcePosition(Structure->RallyPoint);
	UE_LOG(LogTemp, Log, TEXT("KodProd Rally Struct=%d Target=%s"),
		StructureId.Value,
		bNode ? *FString::FromInt(NodeId.Value) : TEXT("Ground"));
}

void UKodSimSubsystem::FinishActiveProduction(FKodStructureState& Structure)
{
	if (Structure.Slots.Num() <= 0)
	{
		return;
	}
	const FKodProductionSlot Slot = Structure.Slots[0];
	Structure.Slots.RemoveAt(0);
	Structure.ActiveTicksRemaining = 0;
	Structure.ActiveBuildTime = 0;
	Structure.bSupplyBlocked = false;

	FKodPendingUnitSpawn Spawn;
	Spawn.UnitId = Slot.UnitId;
	Spawn.TeamId = Structure.TeamId;
	Spawn.SupplyCost = Slot.Supply;
	Spawn.StructureId = Structure.Id.Value;
	Spawn.Location = Structure.ExitPoint;
	Spawn.Yaw = 0.f;
	Spawn.RallyPoint = Structure.RallyPoint;
	Spawn.RallyNode = Structure.RallyNode;
	Spawn.bRallyNode = Structure.bRallyNode;
	PendingSpawns.Add(Spawn);
}

void UKodSimSubsystem::StepProduction()
{
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->Slots.Num() <= 0)
		{
			continue;
		}
		if (!Structure->Slots[0].bStarted || Structure->bSupplyBlocked)
		{
			TryStartProduction(*Structure);
			continue;
		}
		if (Structure->ActiveTicksRemaining > 0)
		{
			--Structure->ActiveTicksRemaining;
		}
		if (Structure->ActiveTicksRemaining <= 0)
		{
			FinishActiveProduction(*Structure);
		}
	}
}

void UKodSimSubsystem::ConsumePendingSpawns(TArray<FKodPendingUnitSpawn>& OutSpawns)
{
	OutSpawns = MoveTemp(PendingSpawns);
	PendingSpawns.Reset();
}

void UKodSimSubsystem::NotifyTrainSpawned(int32 StructureId)
{
	if (FKodStructureState* Structure = Structures.Find(StructureId))
	{
		if (Structure->Slots.Num() > 0 && !Structure->Slots[0].bStarted)
		{
			TryStartProduction(*Structure);
		}
	}
}
