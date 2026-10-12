#include "Sim/KodSimSubsystem.h"

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
	if (CueId.IsNone())
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
	return GetMapCellHeight(CellX, CellY) < 0;
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
		State->bFlyer = Spec->bFlyer;
		State->bHealer = Spec->bHealer;
		State->CargoSize = FMath::Max(0, Spec->CargoSize);
		State->ArmorPoints = FMath::Max(0, Spec->BaseArmor);
		State->EnergyMax = FMath::Max(0, Spec->EnergyMax);
		State->Energy = FMath::Clamp(Spec->EnergyStart, 0, State->EnergyMax);
		State->bAutocastHeal = Spec->bHealer;
	}
	if (const FKodBuildSpec* Build = BuildSpecs.Find(State->DefinitionId))
	{
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
	if (!Structure || !Spec || !Structure->bComplete || Structure->DefinitionId != Spec->BuildingId)
	{
		return false;
	}
	FKodTeamTech& Tech = TeamTech.FindOrAdd(Structure->TeamId);
	if ((Spec->WeaponBonus > 0 && Tech.InfantryWeapons >= 1) || (Spec->ArmorBonus > 0 && Tech.InfantryArmor >= 1))
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
	if (Structure->bLowered)
	{
		for (int32 X = Structure->CellX; X < Structure->CellX + Structure->FootprintX; ++X)
		{
			for (int32 Y = Structure->CellY; Y < Structure->CellY + Structure->FootprintY; ++Y)
			{
				if (CellHasUnit(X, Y))
				{
					RaiseCue(FName(TEXT("cant_build")), Structure->Position, false);
					UE_LOG(LogTemp, Log, TEXT("KodBuild Depot Id=%d RaiseBlocked=1"), StructureId.Value);
					return false;
				}
			}
		}
		Structure->bLowered = false;
		InvalidatePathsOverFootprint(Structure->CellX, Structure->CellY, Structure->FootprintX, Structure->FootprintY);
	}
	else
	{
		Structure->bLowered = true;
	}
	UE_LOG(LogTemp, Log, TEXT("KodBuild Depot Id=%d Lowered=%d"), StructureId.Value, Structure->bLowered ? 1 : 0);
	return true;
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
	const float DistSq = FVector::DistSquared2D(Unit->Position, Bunker->Position);
	if (DistSq > FMath::Square(static_cast<float>(KodBuildGrid::CellUU * 2)))
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
				if (bInside || IsCellBlocked(X, Y) || CellHasUnit(X, Y))
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
		if (!Structure || !Structure->bComplete)
		{
			continue;
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
					FKodTeamTech& Tech = TeamTech.FindOrAdd(Structure->TeamId);
					Tech.InfantryWeapons += FMath::Max(0, Spec->WeaponBonus);
					Tech.InfantryArmor += FMath::Max(0, Spec->ArmorBonus);
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

		if (Structure->AttackDamage <= 0)
		{
			continue;
		}
		FKodSimEntityState* Tower = States.Find(Structure->Id.Value);
		if (!Tower || Tower->Health <= 0.f)
		{
			continue;
		}
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
				TryApplyWeapon(*Tower, *Target);
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
					TryApplyWeapon(*Rider, *Target);
					if (FKodSimEntityState* Live = States.Find(RiderId))
					{
						Live->Position = Saved;
					}
				}
			}
		}
	}
}
