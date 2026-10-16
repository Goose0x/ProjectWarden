#include "Game/KodLanding.h"
#include "Game/KodMeshSockets.h"
#include "Sim/KodSimSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	int32 CeilSqrtInt(int32 Value)
	{
		if (Value <= 0)
		{
			return 0;
		}
		int32 Root = 0;
		while (Root < 46340 && Root * Root < Value)
		{
			++Root;
		}
		return Root;
	}
}

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
		// A lifted structure still provides supply.
		if (Structure && Structure->bComplete && Structure->TeamId == TeamId)
		{
			Sum += FMath::Max(0, Structure->SupplyProvided);
			Sum += FMath::Max(0, Structure->BonusSupply);
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

void UKodSimSubsystem::EnsureStructureDropOff(const FKodStructureState& Structure, int32 StandOverride)
{
	if (!Structure.bIsDropOff || !Structure.bComplete || Structure.bAirborne || !Structure.Id.IsValid())
	{
		return;
	}
	if (DropOffs.Contains(Structure.Id.Value))
	{
		return;
	}
	FKodDropOffState Drop;
	Drop.Id = Structure.Id;
	Drop.TeamId = Structure.TeamId;
	Drop.Position = Structure.Position;
	const int32 HalfX = (Structure.FootprintX * KodBuildGrid::CellUU) / 2;
	const int32 HalfY = (Structure.FootprintY * KodBuildGrid::CellUU) / 2;
	const int32 HalfDiag = CeilSqrtInt(static_cast<int32>(FMath::Min<int64>(
		static_cast<int64>(HalfX) * HalfX + static_cast<int64>(HalfY) * HalfY,
		46340 * 46340)));
	const int32 FootprintClearance = HalfDiag + 120;
	const int32 Requested = StandOverride > 0 ? StandOverride : KodEconomyDefaults::DropOffStandUU;
	Drop.StandUU = FMath::Max(Requested, FootprintClearance);
	DropOffs.Add(Structure.Id.Value, Drop);
	UE_LOG(LogTemp, Log, TEXT("KodEcon DropOff Id=%d Team=%d Loc=%.0f,%.0f,%.0f"),
		Structure.Id.Value,
		Structure.TeamId,
		Drop.Position.X,
		Drop.Position.Y,
		Drop.Position.Z);
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
	// BeginPlay registers a bare entity. That is not a structure yet.
	// A second RegisterStructure call returns the structure that already exists.
	if (Id.IsValid() && Structures.Contains(Id.Value))
	{
		return Id;
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
	Actor->SetActorRotation(KodLanding::CanonicalRotation(), ETeleportType::TeleportPhysics);
	Structure.Position = Actor->GetActorLocation();
	QuantizeResourcePosition(Structure.Position);
	FVector Forward = Actor->GetActorForwardVector();
	Forward.Z = 0.f;
	if (Forward.SizeSquared() < 0.01f)
	{
		Forward = FVector(1.f, 0.f, 0.f);
	}
	Forward.Normalize();
	Structure.SupplyProvided = FMath::Max(0, Spec.SupplyProvided);
	Structure.bIsDropOff = Spec.bIsDropOff;
	Structure.bComplete = true;
	Structure.DisplayId = Spec.DisplayId.IsNone() ? Spec.DefinitionId : Spec.DisplayId;
	Structure.MaxHealth = FMath::Max(1.f, Spec.MaxHealth);
	Structure.FootprintX = FMath::Max(1, Spec.FootprintX);
	Structure.FootprintY = FMath::Max(1, Spec.FootprintY);
	if (const FKodBuildSpec* Build = BuildSpecs.Find(Structure.DefinitionId))
	{
		Structure.CargoCap = FMath::Max(0, Build->CargoCap);
		if (Structure.PaidCredits <= 0 && Structure.PaidLuminene <= 0)
		{
			Structure.PaidCredits = FMath::Max(0, Build->Cost.Credits);
			Structure.PaidLuminene = FMath::Max(0, Build->Cost.Luminene);
		}
	}
	Structure.CellX = KodBuildGrid::CellOf(Structure.Position.X) - (Structure.FootprintX - 1) / 2;
	Structure.CellY = KodBuildGrid::CellOf(Structure.Position.Y) - (Structure.FootprintY - 1) / 2;
	Structure.Trains = Spec.Trains;
	// Half the footprint plus one cell. A 5×5 CC exits at ~700 uu, outside the blocked cells.
	const int32 HalfSpan = (FMath::Max(Structure.FootprintX, Structure.FootprintY) * KodBuildGrid::CellUU) / 2;
	const float ExitDistance = static_cast<float>(HalfSpan + KodBuildGrid::CellUU);
	FVector SocketExit;
	if (KodMeshSockets::TryGet(Actor, KodMeshSockets::UnitExit, SocketExit)
		|| KodMeshSockets::TryGet(Actor, KodMeshSockets::Door, SocketExit)
		|| KodMeshSockets::TryGet(Actor, KodMeshSockets::Exit, SocketExit))
	{
		Structure.ExitPoint = SocketExit;
	}
	else
	{
		Structure.ExitPoint = Structure.Position + Forward * ExitDistance;
	}
	QuantizeResourcePosition(Structure.ExitPoint);
	Structure.StandPoint = Structure.ExitPoint;
	if (Structure.RallyPoint.IsNearlyZero())
	{
		Structure.RallyPoint = Structure.ExitPoint;
	}

	EnsureStructureDropOff(Structure, Spec.DropOffStandUU);

	RecalcSupplyCap(Spec.TeamId);
	if (FKodSimEntityState* Existing = States.Find(Id.Value))
	{
		Existing->DefinitionId = Structure.DefinitionId;
		Existing->Position = Structure.Position;
		Existing->YawDegrees = Actor->GetActorRotation().Yaw;
		Existing->MaxHealth = Structure.MaxHealth;
		Existing->Health = Structure.MaxHealth;
		Existing->bMobile = false;
		Existing->MoveSpeed = 0.f;
		Existing->TeamId = Structure.TeamId;
		if (const FKodBuildSpec* Build = BuildSpecs.Find(Structure.DefinitionId))
		{
			if (Build->Armor > 0)
			{
				Existing->ArmorPoints = Build->Armor;
			}
		}
		Existing->SightRadiusUU = 2000;
		QuantizePose(*Existing);
		PrevPositions.Add(Id.Value, Existing->Position);
		PrevYaws.Add(Id.Value, Existing->YawDegrees);
	}
	else
	{
		FKodSimEntityState Combat;
		Combat.Id = Id;
		Combat.DefinitionId = Structure.DefinitionId;
		Combat.Position = Structure.Position;
		Combat.YawDegrees = Actor->GetActorRotation().Yaw;
		Combat.MaxHealth = Structure.MaxHealth;
		Combat.Health = Structure.MaxHealth;
		Combat.bMobile = false;
		Combat.MoveSpeed = 0.f;
		Combat.TeamId = Structure.TeamId;
		if (const FKodBuildSpec* Build = BuildSpecs.Find(Structure.DefinitionId))
		{
			if (Build->Armor > 0)
			{
				Combat.ArmorPoints = Build->Armor;
			}
		}
		Combat.SightRadiusUU = 2000;
		QuantizePose(Combat);
		States.Add(Id.Value, Combat);
		PrevPositions.Add(Id.Value, Combat.Position);
		PrevYaws.Add(Id.Value, Combat.YawDegrees);
	}
	UE_LOG(LogTemp, Log, TEXT("KodProd Structure Id=%d Team=%d Supply=%d"),
		Id.Value,
		Spec.TeamId,
		Structure.SupplyProvided);
	return Id;
}

void UKodSimSubsystem::TuneStructureCombat(FKodEntityId Id, int32 Armor, float WeaponDamage, float WeaponRange)
{
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State)
	{
		return;
	}
	State->ArmorPoints = FMath::Max(0, Armor);
	State->bMobile = false;
	State->MoveSpeed = 0.f;
	State->WeaponDamage = 0.f;
	State->WeaponRange = 0.f;
	State->WeaponCooldownTicks = 16;
	if (FKodStructureState* Structure = Structures.Find(Id.Value))
	{
		Structure->AttackDamage = FMath::Max(0, FMath::RoundToInt(WeaponDamage));
		Structure->AttackRangeUU = FMath::Max(0, FMath::RoundToInt(WeaponRange));
	}
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
	if (Structure.bAirborne || Structure.Slots.Num() <= 0)
	{
		return;
	}
	FKodProductionSlot& Slot = Structure.Slots[0];
	const FKodSupplyState& Row = Supply.FindOrAdd(Structure.TeamId);
	const bool bIgnoreSupply = bCheatNoSupply && Structure.TeamId == 0;
	if (!bIgnoreSupply && Row.Used + FMath::Max(0, Slot.Supply) > Row.Cap)
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
	if (!Structure || !Spec || !Structure->bComplete || Structure->bAirborne)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodProd Enqueue Miss Struct=%d Unit=%s"), StructureId.Value, *UnitId.ToString());
		return false;
	}
	if (Structure->Trains.Num() > 0 && !Structure->Trains.Contains(UnitId))
	{
		UE_LOG(LogTemp, Warning, TEXT("KodProd Enqueue Miss Struct=%d Unit=%s"), StructureId.Value, *UnitId.ToString());
		return false;
	}
	if (!Spec->RequiresId.IsNone() && !HasCompletedStructure(Structure->TeamId, Spec->RequiresId))
	{
		UE_LOG(LogTemp, Log, TEXT("KodProd Enqueue Reject Struct=%d Unit=%s Reason=Requires"), StructureId.Value, *UnitId.ToString());
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
	const FKodStructureState* Structure = Structures.Find(StructureId.Value);
	if (!Structure || Structure->Slots.Num() <= 0)
	{
		return false;
	}
	return CancelTrainAt(StructureId, Structure->Slots.Num() - 1);
}

bool UKodSimSubsystem::CancelTrainAt(FKodEntityId StructureId, int32 SlotIndex)
{
	FKodStructureState* Structure = Structures.Find(StructureId.Value);
	if (!Structure || SlotIndex < 0 || SlotIndex >= Structure->Slots.Num())
	{
		return false;
	}
	const bool bFront = SlotIndex == 0;
	const FKodProductionSlot Slot = Structure->Slots[SlotIndex];
	Structure->Slots.RemoveAt(SlotIndex);
	if (bFront)
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

void UKodSimSubsystem::SetStructureRally(FKodEntityId StructureId, FVector GroundPoint, FKodEntityId NodeId, FKodEntityId UnitId)
{
	FKodStructureState* Structure = Structures.Find(StructureId.Value);
	if (!Structure)
	{
		return;
	}
	const bool bNode = NodeId.IsValid() && IsResourceNode(NodeId);
	const bool bUnit = !bNode && UnitId.IsValid() && States.Contains(UnitId.Value);
	Structure->bRallyNode = bNode;
	Structure->RallyNode = bNode ? NodeId : FKodEntityId();
	Structure->RallyUnit = bUnit ? UnitId : FKodEntityId();
	Structure->RallyPoint = GroundPoint;
	QuantizeResourcePosition(Structure->RallyPoint);
	UE_LOG(LogTemp, Log, TEXT("KodProd Rally Struct=%d Node=%d Unit=%d"),
		StructureId.Value,
		Structure->RallyNode.Value,
		Structure->RallyUnit.Value);
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
	Spawn.RallyUnit = Structure.RallyUnit;
	PendingSpawns.Add(Spawn);
	NoteUnitTrained(Structure.TeamId);
	ScoreTracker.NoteUnitTrained(Structure.TeamId, Slot.UnitId);
	OnTrainCompleted.Broadcast(Spawn.Location, Spawn.TeamId);
	FName ReadyId;
	if (Slot.UnitId == FName(TEXT("Dozer")))
	{
		ReadyId = FName(TEXT("unit_ready_amp"));
	}
	else if (Slot.UnitId == FName(KodWardenPaths::Id_Marine))
	{
		ReadyId = FName(TEXT("unit_ready_marine"));
	}
	else if (Slot.UnitId == FName(TEXT("Medic")))
	{
		ReadyId = FName(TEXT("unit_ready_medic"));
	}
	RaiseCue(ReadyId, Spawn.Location, false);

	// Count supply and start the next slot in this step. Catch-up steps must not
	// wait for the actor, or a later slot can start early and pass the cap.
	AddSupplyUsed(Structure.TeamId, Slot.Supply);
	if (Structure.Slots.Num() > 0)
	{
		TryStartProduction(Structure);
	}
}

void UKodSimSubsystem::StepProduction()
{
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->bAirborne || Structure->Slots.Num() <= 0)
		{
			continue;
		}
		if (!Structure->Slots[0].bStarted || Structure->bSupplyBlocked)
		{
			TryStartProduction(*Structure);
			continue;
		}
		if (bCheatInstantBuild && Structure->TeamId == 0 && Structure->ActiveTicksRemaining > 1)
		{
			Structure->ActiveTicksRemaining = 1;
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

bool UKodSimSubsystem::TryGetTrainSpec(FName UnitId, FKodTrainSpec& OutSpec) const
{
	if (const FKodTrainSpec* Found = TrainSpecs.Find(UnitId))
	{
		OutSpec = *Found;
		return true;
	}
	return false;
}

bool UKodSimSubsystem::TryGetBuildSpec(FName TypeId, FKodBuildSpec& OutSpec) const
{
	if (const FKodBuildSpec* Found = BuildSpecs.Find(TypeId))
	{
		OutSpec = *Found;
		return true;
	}
	return false;
}

void UKodSimSubsystem::ConsumePendingSpawns(TArray<FKodPendingUnitSpawn>& OutSpawns)
{
	OutSpawns = MoveTemp(PendingSpawns);
	PendingSpawns.Reset();
}

void UKodSimSubsystem::NotifyTrainSpawned(int32 StructureId)
{
	// The actor spawn is presentation. Supply and the next slot were decided in the step.
	(void)StructureId;
}
