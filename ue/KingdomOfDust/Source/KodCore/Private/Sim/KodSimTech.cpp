#include "Sim/KodSimSubsystem.h"

#include "Sim/KodDepotClips.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	constexpr int32 HealRangeUU = 4 * KodBuildGrid::CellUU;
	constexpr int32 BunkerRangeBonusUU = KodBuildGrid::CellUU;
	constexpr int32 EnergyRegenNumer = 63;
	constexpr int32 EnergyRegenDenom = 1280;
	constexpr int32 HealNumer = 63;
	constexpr int32 HealDenom = 80;
}

void UKodSimSubsystem::RaiseCue(FName CueId, FVector Location, bool bAtLocation)
{
	if (bTestIsolation || CueId.IsNone())
	{
		return;
	}
	OnCue.Broadcast(CueId, Location, bAtLocation);
}

void UKodSimSubsystem::SetTeamFacing(int32 TeamId, int32 YawDegrees)
{
	int32 Yaw = YawDegrees % 360;
	if (Yaw < 0)
	{
		Yaw += 360;
	}
	const int32 Quad = FMath::RoundToInt(static_cast<float>(Yaw) / 90.f) * 90;
	TeamFacingYaw.Add(TeamId, Quad % 360);
}

int32 UKodSimSubsystem::GetTeamFacing(int32 TeamId) const
{
	if (const int32* Found = TeamFacingYaw.Find(TeamId))
	{
		return *Found;
	}
	return 90;
}

bool UKodSimSubsystem::HasCompletedStructure(int32 TeamId, FName TypeId) const
{
	if (TypeId.IsNone())
	{
		return true;
	}
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (Structure && Structure->TeamId == TeamId && Structure->bComplete && Structure->DefinitionId == TypeId)
		{
			return true;
		}
	}
	return false;
}

bool UKodSimSubsystem::IsCellTerrainBlocked(int32 CellX, int32 CellY) const
{
	// Height layer hook. Negative height is reserved for unbuildable terrain.
	return GetMapCellHeight(CellX, CellY) < 0 || IsDressBlockCell(CellX, CellY);
}

bool UKodSimSubsystem::CellHasUnit(int32 CellX, int32 CellY) const
{
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || !State->bMobile || State->Health <= 0.f || State->InsideBunker.IsValid())
		{
			continue;
		}
		if (KodBuildGrid::CellOf(State->Position.X) == CellX && KodBuildGrid::CellOf(State->Position.Y) == CellY)
		{
			return true;
		}
	}
	return false;
}

void UKodSimSubsystem::ApplyRoster(FKodEntityId Id)
{
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State)
	{
		return;
	}
	if (const FKodTrainSpec* Spec = TrainSpecs.Find(State->DefinitionId))
	{
		State->bBiological = Spec->bBiological;
		State->bInfantry = Spec->bInfantry;
		State->bLight = Spec->bLight;
		State->bFlyer = Spec->bFlyer;
		State->bHealer = Spec->bHealer;
		State->CargoSize = FMath::Max(0, Spec->CargoSize);
		State->ArmorPoints = FMath::Max(0, Spec->BaseArmor);
		State->EnergyMax = FMath::Max(0, Spec->EnergyMax);
		State->Energy = FMath::Clamp(Spec->EnergyStart, 0, State->EnergyMax);
		State->bAutocastHeal = Spec->bHealer;
		State->GatherRatePermille = FMath::Max(1, Spec->GatherRatePermille > 0 ? Spec->GatherRatePermille : 1000);
	}
	if (const FKodBuildSpec* Build = BuildSpecs.Find(State->DefinitionId))
	{
		if (Build->Armor > 0)
		{
			State->ArmorPoints = Build->Armor;
		}
		if (Build->AttackDamage > 0)
		{
			State->WeaponDamage = static_cast<float>(Build->AttackDamage);
			State->WeaponRange = static_cast<float>(Build->AttackRangeUU);
			State->WeaponCooldownTicks = 16;
			State->WeaponBackswingTicks = 2;
		}
	}
}

void UKodSimSubsystem::RegisterUpgradeSpec(const FKodUpgradeSpec& Spec)
{
	if (Spec.UpgradeId.IsNone())
	{
		return;
	}
	UpgradeSpecs.Add(Spec.UpgradeId, Spec);
}

bool UKodSimSubsystem::TryGetUpgradeSpec(FName UpgradeId, FKodUpgradeSpec& OutSpec) const
{
	if (const FKodUpgradeSpec* Found = UpgradeSpecs.Find(UpgradeId))
	{
		OutSpec = *Found;
		return true;
	}
	return false;
}

bool UKodSimSubsystem::EnqueueResearch(FKodEntityId StructureId, FName UpgradeId)
{
	FKodStructureState* Structure = Structures.Find(StructureId.Value);
	const FKodUpgradeSpec* Spec = UpgradeSpecs.Find(UpgradeId);
	if (!Structure || !Spec || !Structure->bComplete || Structure->bAirborne || Structure->DefinitionId != Spec->BuildingId)
	{
		return false;
	}
	FKodTeamTech& Tech = TeamTech.FindOrAdd(Structure->TeamId);
	if ((Spec->WeaponBonus > 0 && Tech.InfantryWeapons >= 1) || (Spec->ArmorBonus > 0 && Tech.InfantryArmor >= 1))
	{
		return false;
	}
	if (Spec->SatelliteFeed > 0 && Tech.SatelliteFeed >= 1)
	{
		return false;
	}
	for (const FKodProductionSlot& Slot : Structure->Research)
	{
		if (Slot.UnitId == UpgradeId)
		{
			return false;
		}
	}
	if (Spec->AttackDamage > 0 && Structure->bTurret)
	{
		return false;
	}
	FKodResourceCost Price;
	Price.Jadeite = FMath::Max(0, Spec->Credits);
	Price.Luminene = FMath::Max(0, Spec->Luminene);
	if (!Spend(Structure->TeamId, Price))
	{
		return false;
	}
	FKodProductionSlot Slot;
	Slot.UnitId = Spec->UpgradeId;
	Slot.DisplayId = Spec->DisplayId.IsNone() ? Spec->UpgradeId : Spec->DisplayId;
	Slot.Credits = Price.Jadeite;
	Slot.Luminene = Price.Luminene;
	Slot.BuildTime = FMath::Max(1, Spec->BuildTicks);
	Structure->Research.Add(Slot);
	if (Structure->Research.Num() == 1)
	{
		Structure->Research[0].bStarted = true;
		Structure->ResearchBuildTime = Slot.BuildTime;
		Structure->ResearchTicksRemaining = Slot.BuildTime;
	}
	UE_LOG(LogTemp, Log, TEXT("KodResearch Enqueue Struct=%d Upgrade=%s"), StructureId.Value, *UpgradeId.ToString());
	return true;
}

bool UKodSimSubsystem::ToggleDepot(FKodEntityId StructureId)
{
	FKodStructureState* Structure = Structures.Find(StructureId.Value);
	if (!Structure || !Structure->bComplete || !Structure->bCanLower)
	{
		return false;
	}
	if (static_cast<EKodDepotPose>(Structure->DepotPose) == EKodDepotPose::Dying)
	{
		return false;
	}
	if (Structure->bLowered)
	{
		// SC2 push: a raise shoves occupants to the nearest free cell, or refuses and logs RaiseBlocked.
		// Refuse-only would drop the push this command already has, so the push stays.
		const int32 FootMinX = Structure->CellX;
		const int32 FootMinY = Structure->CellY;
		const int32 FootMaxX = Structure->CellX + FMath::Max(1, Structure->FootprintX);
		const int32 FootMaxY = Structure->CellY + FMath::Max(1, Structure->FootprintY);
		TArray<int32> Keys;
		States.GetKeys(Keys);
		Keys.Sort();
		TArray<int32> Occupants;
		TArray<FIntPoint> Standing;
		for (int32 Key : Keys)
		{
			const FKodSimEntityState* State = States.Find(Key);
			if (!State || !State->bMobile || State->Health <= 0.f || State->InsideBunker.IsValid())
			{
				continue;
			}
			const int32 CellX = KodBuildGrid::CellOf(State->Position.X);
			const int32 CellY = KodBuildGrid::CellOf(State->Position.Y);
			const bool bOnDepot = CellX >= FootMinX && CellX < FootMaxX && CellY >= FootMinY && CellY < FootMaxY;
			if (bOnDepot)
			{
				Occupants.Add(Key);
			}
			else
			{
				Standing.Add(FIntPoint(CellX, CellY));
			}
		}
		int32 BoundMinX = 0;
		int32 BoundMinY = 0;
		int32 BoundMaxX = 0;
		int32 BoundMaxY = 0;
		const bool bPlayable = TryGetPlayableCellBounds(BoundMinX, BoundMinY, BoundMaxX, BoundMaxY);
		TArray<int32> PushIds;
		TArray<FIntPoint> PushCells;
		TArray<FIntPoint> Reserved = Standing;
		bool bCanPush = true;
		for (int32 OccupantId : Occupants)
		{
			const FKodSimEntityState* State = States.Find(OccupantId);
			if (!State)
			{
				bCanPush = false;
				break;
			}
			const int32 FromX = KodBuildGrid::CellOf(State->Position.X);
			const int32 FromY = KodBuildGrid::CellOf(State->Position.Y);
			bool bFound = false;
			int32 BestX = 0;
			int32 BestY = 0;
			int64 BestDist = MAX_int64;
			constexpr int32 Search = 8;
			for (int32 X = FromX - Search; X <= FromX + Search; ++X)
			{
				for (int32 Y = FromY - Search; Y <= FromY + Search; ++Y)
				{
					if (X >= FootMinX && X < FootMaxX && Y >= FootMinY && Y < FootMaxY)
					{
						continue;
					}
					if (bPlayable && (X < BoundMinX || Y < BoundMinY || X > BoundMaxX || Y > BoundMaxY))
					{
						continue;
					}
					if (IsCellBlocked(X, Y, State->TeamId, true) || IsCellTerrainBlocked(X, Y))
					{
						continue;
					}
					bool bTaken = false;
					for (const FIntPoint& Cell : Reserved)
					{
						if (Cell.X == X && Cell.Y == Y)
						{
							bTaken = true;
							break;
						}
					}
					if (bTaken)
					{
						continue;
					}
					const int64 DX = static_cast<int64>(X - FromX);
					const int64 DY = static_cast<int64>(Y - FromY);
					const int64 Dist = DX * DX + DY * DY;
					if (!bFound || Dist < BestDist || (Dist == BestDist && (X < BestX || (X == BestX && Y < BestY))))
					{
						bFound = true;
						BestDist = Dist;
						BestX = X;
						BestY = Y;
					}
				}
			}
			if (!bFound)
			{
				bCanPush = false;
				break;
			}
			Reserved.Add(FIntPoint(BestX, BestY));
			PushIds.Add(OccupantId);
			PushCells.Add(FIntPoint(BestX, BestY));
		}
		if (!bCanPush)
		{
			RaiseCue(FName(TEXT("depot_raise_blocked")), Structure->Position, false);
			UE_LOG(LogTemp, Log, TEXT("KodBuild Depot Id=%d RaiseBlocked=1"), StructureId.Value);
			return false;
		}
		for (int32 Index = 0; Index < PushIds.Num(); ++Index)
		{
			FKodSimEntityState* State = States.Find(PushIds[Index]);
			if (!State)
			{
				continue;
			}
			State->Position.X = KodBuildGrid::CellCenter(PushCells[Index].X);
			State->Position.Y = KodBuildGrid::CellCenter(PushCells[Index].Y);
			QuantizePose(*State);
		}
		if (PushIds.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("KodBuild Depot Id=%d Pushed=%d"), StructureId.Value, PushIds.Num());
		}
		BeginDepotCommand(*Structure, false);
		InvalidatePathsOverFootprint(Structure->CellX, Structure->CellY, Structure->FootprintX, Structure->FootprintY);
	}
	else
	{
		BeginDepotCommand(*Structure, true);
		InvalidatePathsOverFootprint(Structure->CellX, Structure->CellY, Structure->FootprintX, Structure->FootprintY);
	}
	UE_LOG(LogTemp, Log, TEXT("KodBuild Depot Id=%d Lowered=%d"), StructureId.Value, Structure->bLowered ? 1 : 0);
	return true;
}

void UKodSimSubsystem::SetDepotWalkable(FKodStructureState& Structure, bool bWalkable)
{
	if (Structure.bDepotWalkable == bWalkable)
	{
		return;
	}
	Structure.bDepotWalkable = bWalkable;
	InvalidatePathsOverFootprint(Structure.CellX, Structure.CellY, Structure.FootprintX, Structure.FootprintY);
	UE_LOG(LogTemp, Log, TEXT("KodDepot Nav Id=%d Walkable=%d"), Structure.Id.Value, bWalkable ? 1 : 0);
}

void UKodSimSubsystem::BeginDepotCommand(FKodStructureState& Structure, bool bWantLowered)
{
	Structure.bLowered = bWantLowered;
	Structure.DepotPoseTicks = 0;
	if (bWantLowered)
	{
		Structure.DepotPose = static_cast<uint8>(EKodDepotPose::Lowering);
	}
	else
	{
		Structure.DepotPose = static_cast<uint8>(EKodDepotPose::Raising);
		if (FKodDepotClipTable::Get().RaiseBlockTick() <= 0)
		{
			SetDepotWalkable(Structure, false);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("KodDepot Pose Id=%d Pose=%d Ticks=%d Walkable=%d"),
		Structure.Id.Value,
		static_cast<int32>(Structure.DepotPose),
		Structure.DepotPoseTicks,
		Structure.bDepotWalkable ? 1 : 0);
}

void UKodSimSubsystem::StepDepotPoses()
{
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	TArray<int32> Handoff;
	for (int32 Key : Keys)
	{
		FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || !Structure->bCanLower)
		{
			continue;
		}
		const EKodDepotPose Pose = static_cast<EKodDepotPose>(Structure->DepotPose);
		if (!Structure->bComplete && Pose != EKodDepotPose::Dying)
		{
			continue;
		}
		if (Pose == EKodDepotPose::Dying)
		{
			++Structure->DepotPoseTicks;
			const int32 DeathLength = FKodDepotClipTable::Get().ClipTicks(KodDepotActiveClip(*Structure));
			if (Structure->DepotPoseTicks >= DeathLength)
			{
				Handoff.Add(Key);
			}
			continue;
		}
		if (Pose == EKodDepotPose::Lowering)
		{
			++Structure->DepotPoseTicks;
			if (Structure->DepotPoseTicks >= FKodDepotClipTable::Get().WalkableTick())
			{
				SetDepotWalkable(*Structure, true);
			}
			const int32 LowerLength = FKodDepotClipTable::Get().ClipTicks(FName(KodDepotClipId::Lower));
			if (Structure->DepotPoseTicks >= LowerLength)
			{
				Structure->DepotPose = static_cast<uint8>(EKodDepotPose::Lowered);
				Structure->DepotPoseTicks = 0;
			}
			continue;
		}
		if (Pose == EKodDepotPose::Raising)
		{
			++Structure->DepotPoseTicks;
			const int32 BlockAt = FKodDepotClipTable::Get().RaiseBlockTick();
			if (BlockAt > 0 && Structure->DepotPoseTicks >= BlockAt)
			{
				SetDepotWalkable(*Structure, false);
			}
			const int32 RaiseLength = FKodDepotClipTable::Get().ClipTicks(FName(KodDepotClipId::Raise));
			if (Structure->DepotPoseTicks >= RaiseLength)
			{
				Structure->DepotPose = static_cast<uint8>(EKodDepotPose::Raised);
				Structure->DepotPoseTicks = 0;
			}
			continue;
		}
		FName IdleClip = FName(KodDepotClipId::Idle);
		if (Pose == EKodDepotPose::Lowered)
		{
			IdleClip = FName(KodDepotClipId::LoweredIdle);
		}
		++Structure->DepotPoseTicks;
		const int32 LoopLength = FKodDepotClipTable::Get().ClipTicks(IdleClip);
		if (LoopLength > 0 && Structure->DepotPoseTicks >= LoopLength)
		{
			Structure->DepotPoseTicks = 0;
		}
	}
	for (int32 Key : Handoff)
	{
		FKodSimEntityState* State = States.Find(Key);
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!State || !Structure)
		{
			continue;
		}
		const int32 Killer = Structure->DepotDeathKiller;
		FinishEntityDeath(*State, Killer);
	}
}

bool UKodSimSubsystem::LoadUnitIntoBunker(FKodEntityId UnitId, FKodEntityId BunkerId)
{
	FKodSimEntityState* Unit = States.Find(UnitId.Value);
	FKodStructureState* Bunker = Structures.Find(BunkerId.Value);
	if (!Unit || !Bunker || !Bunker->bComplete || Bunker->CargoCap <= 0 || Unit->CargoSize <= 0 || Unit->InsideBunker.IsValid())
	{
		return false;
	}
	if (Unit->TeamId != Bunker->TeamId || !Unit->bInfantry)
	{
		return false;
	}
	int32 Used = 0;
	for (int32 CargoId : Bunker->Cargo)
	{
		if (const FKodSimEntityState* Rider = States.Find(CargoId))
		{
			Used += FMath::Max(1, Rider->CargoSize);
		}
	}
	if (Used + Unit->CargoSize > Bunker->CargoCap)
	{
		return false;
	}
	const int32 HalfSpan = (FMath::Max(Bunker->FootprintX, Bunker->FootprintY) * KodBuildGrid::CellUU) / 2;
	const float LoadReach = static_cast<float>(HalfSpan + KodBuildGrid::CellUU * 2);
	const float DistSq = FVector::DistSquared2D(Unit->Position, Bunker->Position);
	if (DistSq > FMath::Square(LoadReach))
	{
		if (Unit->LoadTarget.Value != BunkerId.Value)
		{
			IssueMove(UnitId, Bunker->Position);
			if (FKodSimEntityState* Moved = States.Find(UnitId.Value))
			{
				Moved->LoadTarget = BunkerId;
			}
		}
		return true;
	}
	Unit->InsideBunker = BunkerId;
	Unit->LoadTarget = FKodEntityId();
	Unit->Order = EKodSimOrderType::None;
	Unit->AttackTarget = FKodEntityId();
	Unit->Position = Bunker->Position;
	QuantizePose(*Unit);
	Bunker->Cargo.Add(UnitId.Value);
	Bunker->Cargo.Sort();
	UE_LOG(LogTemp, Log, TEXT("KodBunker Load Bunker=%d Unit=%d"), BunkerId.Value, UnitId.Value);
	return true;
}

bool UKodSimSubsystem::UnloadUnit(FKodEntityId BunkerId, int32 UnitId)
{
	FKodStructureState* Bunker = Structures.Find(BunkerId.Value);
	FKodSimEntityState* Unit = States.Find(UnitId);
	if (!Bunker || !Unit || Unit->InsideBunker.Value != BunkerId.Value)
	{
		return false;
	}
	int32 BestX = Bunker->CellX - 1;
	int32 BestY = Bunker->CellY;
	int64 Best = MAX_int64;
	bool bFound = false;
	for (int32 Radius = 1; Radius <= 4 && !bFound; ++Radius)
	{
		for (int32 X = Bunker->CellX - Radius; X <= Bunker->CellX + Bunker->FootprintX + Radius - 1; ++X)
		{
			for (int32 Y = Bunker->CellY - Radius; Y <= Bunker->CellY + Bunker->FootprintY + Radius - 1; ++Y)
			{
				const bool bInside = X >= Bunker->CellX && X < Bunker->CellX + Bunker->FootprintX
					&& Y >= Bunker->CellY && Y < Bunker->CellY + Bunker->FootprintY;
				if (bInside || IsCellBlocked(X, Y, Bunker->TeamId, true) || CellHasUnit(X, Y))
				{
					continue;
				}
				const int64 Dist = static_cast<int64>(X - Bunker->CellX) * (X - Bunker->CellX)
					+ static_cast<int64>(Y - Bunker->CellY) * (Y - Bunker->CellY);
				if (!bFound || Dist < Best || (Dist == Best && (X < BestX || (X == BestX && Y < BestY))))
				{
					bFound = true;
					Best = Dist;
					BestX = X;
					BestY = Y;
				}
			}
		}
	}
	Unit->InsideBunker = FKodEntityId();
	Unit->Position = FVector(KodBuildGrid::CellCenter(BestX), KodBuildGrid::CellCenter(BestY), Bunker->Position.Z);
	QuantizePose(*Unit);
	Bunker->Cargo.Remove(UnitId);
	UE_LOG(LogTemp, Log, TEXT("KodBunker Unload Bunker=%d Unit=%d"), BunkerId.Value, UnitId);
	return true;
}

bool UKodSimSubsystem::UnloadAll(FKodEntityId BunkerId)
{
	const FKodStructureState* Bunker = Structures.Find(BunkerId.Value);
	if (!Bunker)
	{
		return false;
	}
	TArray<int32> Riders = Bunker->Cargo;
	Riders.Sort();
	bool bAny = false;
	for (int32 Rider : Riders)
	{
		bAny = UnloadUnit(BunkerId, Rider) || bAny;
	}
	return bAny;
}

void UKodSimSubsystem::EjectBunker(FKodEntityId BunkerId)
{
	UnloadAll(BunkerId);
}

bool UKodSimSubsystem::IssueSalvage(FKodEntityId StructureId)
{
	FKodStructureState* Structure = Structures.Find(StructureId.Value);
	if (!Structure || !Structure->bComplete || Structure->bAirborne || Structure->SalvageTicksRemaining > 0)
	{
		return false;
	}
	if (Structure->DefinitionId != FName(KodWardenPaths::Id_Bunker))
	{
		return false;
	}
	Structure->SalvageTicksRemaining = 5 * 16;
	UE_LOG(LogTemp, Log, TEXT("KodBunker Salvage Start Id=%d Ticks=%d"), StructureId.Value, Structure->SalvageTicksRemaining);
	return true;
}

void UKodSimSubsystem::FinishSalvage(int32 StructureId)
{
	FKodStructureState* Structure = Structures.Find(StructureId);
	if (!Structure)
	{
		return;
	}
	int32 Credits = Structure->PaidCredits;
	int32 Gas = Structure->PaidLuminene;
	if (Credits <= 0 && Gas <= 0)
	{
		if (const FKodBuildSpec* Spec = BuildSpecs.Find(Structure->DefinitionId))
		{
			Credits = FMath::Max(0, Spec->Cost.Credits);
			Gas = FMath::Max(0, Spec->Cost.Luminene);
		}
	}
	const int32 RefundCredits = (FMath::Max(0, Credits) * 3) / 4;
	const int32 RefundGas = (FMath::Max(0, Gas) * 3) / 4;
	const int32 TeamId = Structure->TeamId;
	TArray<int32> Workers;
	States.GetKeys(Workers);
	Workers.Sort();
	for (int32 WorkerKey : Workers)
	{
		if (FKodSimEntityState* Worker = States.Find(WorkerKey))
		{
			if (Worker->BuildSite.Value == StructureId)
			{
				Worker->BuildSite = FKodEntityId();
			}
		}
	}
	FKodEntityId Id;
	Id.Value = StructureId;
	if (RefundCredits > 0 || RefundGas > 0)
	{
		FKodResourceCost Refund;
		Refund.Jadeite = RefundCredits;
		Refund.Luminene = RefundGas;
		Deposit(TeamId, Refund);
	}
	UE_LOG(LogTemp, Log, TEXT("KodBunker Salvage Refund Id=%d Credits=%d Luminene=%d"), StructureId, RefundCredits, RefundGas);
	UnregisterEntity(Id);
}

bool UKodSimSubsystem::IssueRepair(FKodEntityId WorkerId, FKodEntityId StructureId)
{
	FKodSimEntityState* Worker = States.Find(WorkerId.Value);
	FKodStructureState* Structure = Structures.Find(StructureId.Value);
	FKodSimEntityState* Combat = States.Find(StructureId.Value);
	if (!Worker || !Structure || !Combat || !Worker->bCanGather || Worker->Health <= 0.f)
	{
		return false;
	}
	if (!Structure->bComplete || Structure->bAirborne || Worker->TeamId != Structure->TeamId)
	{
		return false;
	}
	if (Combat->Health + 0.5f >= Combat->MaxHealth)
	{
		return false;
	}
	const float Dist = FVector::Dist2D(Worker->Position, Structure->Position);
	if (Dist > RepairReachUU(*Structure))
	{
		const FVector Goal = Structure->StandPoint.IsNearlyZero() ? Structure->Position : Structure->StandPoint;
		IssueMove(WorkerId, Goal);
	}
	if (FKodSimEntityState* Live = States.Find(WorkerId.Value))
	{
		Live->BuildSite = StructureId;
	}
	UE_LOG(LogTemp, Log, TEXT("KodRepair Order Worker=%d Structure=%d"), WorkerId.Value, StructureId.Value);
	return true;
}

void UKodSimSubsystem::ApplyInstalledTurret(FKodStructureState& Structure)
{
	if (!Structure.bTurret)
	{
		return;
	}
	const FKodUpgradeSpec* Spec = UpgradeSpecs.Find(FName(KodWardenPaths::Id_BunkerTurret));
	const int32 Damage = (Spec && Spec->AttackDamage > 0) ? Spec->AttackDamage : 12;
	const int32 Range = (Spec && Spec->AttackRangeUU > 0) ? Spec->AttackRangeUU : 1200;
	const bool bAir = Spec && Spec->AttacksAir != 0;
	const int32 BonusHp = (Spec && Spec->BonusHealth > 0) ? Spec->BonusHealth : 100;
	const int32 VsLight = (Spec && Spec->BonusVsLight > 0) ? Spec->BonusVsLight : 6;
	const int32 Period = (Spec && Spec->CooldownTicks > 1) ? Spec->CooldownTicks : 14;
	Structure.AttackDamage = Damage;
	Structure.AttackRangeUU = Range;
	Structure.bAttacksAir = bAir;
	Structure.BonusVsLight = VsLight;
	if (Structure.TurretHpBonus < BonusHp)
	{
		const int32 Add = BonusHp - Structure.TurretHpBonus;
		Structure.MaxHealth += static_cast<float>(Add);
		Structure.TurretHpBonus = BonusHp;
		if (FKodSimEntityState* Combat = States.Find(Structure.Id.Value))
		{
			Combat->MaxHealth += static_cast<float>(Add);
			Combat->Health += static_cast<float>(Add);
		}
	}
	if (FKodSimEntityState* Combat = States.Find(Structure.Id.Value))
	{
		Combat->WeaponDamage = static_cast<float>(Damage);
		Combat->WeaponRange = static_cast<float>(Range);
		Combat->WeaponBackswingTicks = 1;
		Combat->WeaponCooldownTicks = FMath::Max(1, Period - 1);
	}
}

void UKodSimSubsystem::SetHealAutocast(FKodEntityId UnitId, bool bEnabled)
{
	if (FKodSimEntityState* State = States.Find(UnitId.Value))
	{
		if (State->bHealer)
		{
			State->bAutocastHeal = bEnabled;
			UE_LOG(LogTemp, Log, TEXT("KodHeal Autocast Unit=%d On=%d"), UnitId.Value, bEnabled ? 1 : 0);
		}
	}
}

bool UKodSimSubsystem::CastHeal(FKodEntityId UnitId)
{
	FKodSimEntityState* Medic = States.Find(UnitId.Value);
	if (!Medic || !Medic->bHealer || Medic->InsideBunker.IsValid() || Medic->Energy <= 0 || Medic->Health <= 0.f)
	{
		return false;
	}
	const FKodSimEntityState* Best = nullptr;
	int32 BestHp = 1;
	int32 BestMax = 1;
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* Other = States.Find(Key);
		if (!Other || Other->Id.Value == Medic->Id.Value || Other->TeamId != Medic->TeamId || !Other->bBiological)
		{
			continue;
		}
		if (Other->Health <= 0.f || Other->Health >= Other->MaxHealth - 0.5f || Other->InsideBunker.IsValid())
		{
			continue;
		}
		if (FVector::DistSquared2D(Medic->Position, Other->Position) > FMath::Square(static_cast<float>(HealRangeUU)))
		{
			continue;
		}
		const int32 Hp = FMath::Max(0, FMath::RoundToInt(Other->Health));
		const int32 MaxHp = FMath::Max(1, FMath::RoundToInt(Other->MaxHealth));
		const int64 Left = static_cast<int64>(Hp) * static_cast<int64>(BestMax);
		const int64 Right = static_cast<int64>(BestHp) * static_cast<int64>(MaxHp);
		if (!Best || Left < Right || (Left == Right && Other->Id.Value < Best->Id.Value))
		{
			Best = Other;
			BestHp = Hp;
			BestMax = MaxHp;
		}
	}
	if (!Best)
	{
		return false;
	}
	FKodSimEntityState* Target = States.Find(Best->Id.Value);
	if (!Target)
	{
		return false;
	}
	Medic->HealAcc += HealNumer;
	int32 Healed = 0;
	while (Medic->HealAcc >= HealDenom && Medic->Energy > 0 && Target->Health < Target->MaxHealth)
	{
		Medic->HealAcc -= HealDenom;
		Target->Health = FMath::Min(Target->MaxHealth, Target->Health + 1.f);
		--Medic->Energy;
		++Healed;
	}
	if (Healed > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("KodHeal Unit=%d Target=%d HP=%.0f Energy=%d"),
			Medic->Id.Value,
			Target->Id.Value,
			Target->Health,
			Medic->Energy);
	}
	return Healed > 0;
}

void UKodSimSubsystem::StepTech()
{
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		FKodSimEntityState* State = States.Find(Key);
		if (!State || State->Health <= 0.f)
		{
			continue;
		}
		if (State->LoadTarget.IsValid())
		{
			LoadUnitIntoBunker(State->Id, State->LoadTarget);
		}
		if (State->EnergyMax > 0 && State->Energy < State->EnergyMax)
		{
			State->EnergyRegenAcc += EnergyRegenNumer;
			while (State->EnergyRegenAcc >= EnergyRegenDenom && State->Energy < State->EnergyMax)
			{
				State->EnergyRegenAcc -= EnergyRegenDenom;
				++State->Energy;
			}
		}
		if (State->bAutocastHeal)
		{
			CastHeal(State->Id);
		}
	}

	TArray<int32> StructureKeys;
	Structures.GetKeys(StructureKeys);
	StructureKeys.Sort();
	for (int32 Key : StructureKeys)
	{
		FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || !Structure->bComplete || Structure->bAirborne)
		{
			continue;
		}
		if (Structure->SalvageTicksRemaining > 0)
		{
			const int32 SalvageId = Structure->Id.Value;
			--Structure->SalvageTicksRemaining;
			if (Structure->SalvageTicksRemaining <= 0)
			{
				FinishSalvage(SalvageId);
				continue;
			}
		}
		if (Structure->Research.Num() > 0)
		{
			FKodProductionSlot& Job = Structure->Research[0];
			if (!Job.bStarted)
			{
				Job.bStarted = true;
				Structure->ResearchBuildTime = FMath::Max(1, Job.BuildTime);
				Structure->ResearchTicksRemaining = Structure->ResearchBuildTime;
			}
			if (bCheatInstantBuild && Structure->TeamId == 0 && Structure->ResearchTicksRemaining > 1)
			{
				Structure->ResearchTicksRemaining = 1;
			}
			if (Structure->ResearchTicksRemaining > 0)
			{
				--Structure->ResearchTicksRemaining;
			}
			if (Structure->ResearchTicksRemaining <= 0)
			{
				const FName UpgradeId = Job.UnitId;
				Structure->Research.RemoveAt(0);
				if (const FKodUpgradeSpec* Spec = UpgradeSpecs.Find(UpgradeId))
				{
					if (Spec->AttackDamage > 0)
					{
						Structure->bTurret = true;
						Structure->PaidCredits += FMath::Max(0, Spec->Credits);
						Structure->PaidLuminene += FMath::Max(0, Spec->Luminene);
						ApplyInstalledTurret(*Structure);
						UE_LOG(LogTemp, Log, TEXT("KodBunker Turret Id=%d Damage=%d Range=%d Air=%d"),
							Structure->Id.Value,
							Structure->AttackDamage,
							Structure->AttackRangeUU,
							Structure->bAttacksAir ? 1 : 0);
					}
					FKodTeamTech& Tech = TeamTech.FindOrAdd(Structure->TeamId);
					Tech.InfantryWeapons += FMath::Max(0, Spec->WeaponBonus);
					Tech.InfantryArmor += FMath::Max(0, Spec->ArmorBonus);
					if (Spec->SatelliteFeed > 0)
					{
						Tech.SatelliteFeed = 1;
						const int32 EnergyMax = FMath::Max(1, Spec->EnergyMax);
						const int32 EnergyStart = FMath::Clamp(Spec->EnergyStart, 0, EnergyMax);
						TArray<int32> CcKeys;
						Structures.GetKeys(CcKeys);
						CcKeys.Sort();
						for (int32 CcKey : CcKeys)
						{
							const FKodStructureState* Cc = Structures.Find(CcKey);
							if (!Cc || Cc->TeamId != Structure->TeamId || Cc->DefinitionId != FName(KodWardenPaths::Id_CommandCenter))
							{
								continue;
							}
							if (FKodSimEntityState* Combat = States.Find(CcKey))
							{
								Combat->EnergyMax = EnergyMax;
								Combat->Energy = EnergyStart;
							}
						}
						UE_LOG(LogTemp, Log, TEXT("KodOrbital Research Team=%d"), Structure->TeamId);
					}
				}
				RaiseCue(FName(TEXT("research_complete")), Structure->Position, false);
				UE_LOG(LogTemp, Log, TEXT("KodResearch Complete Struct=%d Upgrade=%s"), Structure->Id.Value, *UpgradeId.ToString());
				if (Structure->Research.Num() > 0)
				{
					Structure->Research[0].bStarted = true;
					Structure->ResearchBuildTime = FMath::Max(1, Structure->Research[0].BuildTime);
					Structure->ResearchTicksRemaining = Structure->ResearchBuildTime;
				}
			}
		}

			if (Structure->AttackDamage > 0)
		{
		FKodSimEntityState* Tower = States.Find(Structure->Id.Value);
		if (Tower && Tower->Health > 0.f)
		{
		const float Range = static_cast<float>(Structure->AttackRangeUU);
		FKodEntityId Best;
		float BestDist = 0.f;
		bool bFound = false;
		TArray<int32> Nearby;
		CollectAcquireCandidates(Structure->Position.X, Structure->Position.Y, Range, Nearby);
		for (int32 UnitKey : Nearby)
		{
			const FKodSimEntityState* Other = States.Find(UnitKey);
			if (!Other || Other->Health <= 0.f || Other->TeamId == Structure->TeamId || Other->InsideBunker.IsValid())
			{
				continue;
			}
			if (Structure->bAttacksAir && !Other->bFlyer)
			{
				continue;
			}
			if (!CanTeamSeeWorld(Structure->TeamId, Other->Position.X, Other->Position.Y))
			{
				continue;
			}
			const float Dist = FVector::DistSquared2D(Structure->Position, Other->Position);
			if (Dist > FMath::Square(Range))
			{
				continue;
			}
			if (!bFound || Dist < BestDist)
			{
				bFound = true;
				BestDist = Dist;
				Best.Value = UnitKey;
			}
		}
			if (bFound)
			{
				if (FKodSimEntityState* Target = States.Find(Best.Value))
				{
					Tower->WeaponDamage = static_cast<float>(Structure->AttackDamage);
					Tower->WeaponRange = Range;
					if (TryApplyWeapon(*Tower, *Target))
					{
						++Structure->MuzzleSerial;
					}
				}
			}
		}
		}

		for (int32 RiderId : Structure->Cargo)
		{
			FKodSimEntityState* Rider = States.Find(RiderId);
			if (!Rider || Rider->Health <= 0.f || Rider->bHealer || Rider->WeaponDamage <= 0.f)
			{
				continue;
			}
			const float RiderRange = Rider->WeaponRange + static_cast<float>(BunkerRangeBonusUU);
			FKodEntityId Shot;
			float ShotDist = 0.f;
			bool bShot = false;
			TArray<int32> RiderNearby;
			CollectAcquireCandidates(Structure->Position.X, Structure->Position.Y, RiderRange, RiderNearby);
			for (int32 UnitKey : RiderNearby)
			{
				const FKodSimEntityState* Other = States.Find(UnitKey);
				if (!Other || Other->Health <= 0.f || Other->TeamId == Rider->TeamId || Other->InsideBunker.IsValid())
				{
					continue;
				}
				if (!CanTeamSeeWorld(Rider->TeamId, Other->Position.X, Other->Position.Y))
				{
					continue;
				}
				const float Dist = FVector::DistSquared2D(Structure->Position, Other->Position);
				if (Dist > FMath::Square(RiderRange))
				{
					continue;
				}
				if (!bShot || Dist < ShotDist)
				{
					bShot = true;
					ShotDist = Dist;
					Shot.Value = UnitKey;
				}
			}
			if (bShot)
			{
				if (FKodSimEntityState* Target = States.Find(Shot.Value))
				{
					const FVector Saved = Rider->Position;
					Rider->Position = Structure->Position;
					if (TryApplyWeapon(*Rider, *Target))
					{
						++Structure->MuzzleSerial;
					}
					if (FKodSimEntityState* Live = States.Find(RiderId))
					{
						Live->Position = Saved;
					}
				}
			}
		}
	}
}
