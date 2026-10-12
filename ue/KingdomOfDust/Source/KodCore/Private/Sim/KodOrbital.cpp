#include "Sim/KodSimSubsystem.h"

#include "Sim/KodBalanceRows.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	int32* CooldownField(FKodStructureState& Structure, FName AbilityId)
	{
		if (AbilityId == FName(KodWardenPaths::Id_OrbitalGathererDrop))
		{
			return &Structure.GathererCd;
		}
		if (AbilityId == FName(KodWardenPaths::Id_SupplyBoost))
		{
			return &Structure.SupplyCd;
		}
		if (AbilityId == FName(KodWardenPaths::Id_IsrView))
		{
			return &Structure.IsrCd;
		}
		return nullptr;
	}
}

void UKodSimSubsystem::RegisterAbilitySpec(const FKodAbilitySpec& Spec)
{
	if (Spec.AbilityId.IsNone())
	{
		return;
	}
	AbilitySpecs.Add(Spec.AbilityId, Spec);
}

void UKodSimSubsystem::FinishOrbitalSpawn(FKodEntityId Id, const FKodPendingUnitSpawn& Request)
{
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State)
	{
		return;
	}
	if (Request.GatherRatePermille > 0)
	{
		State->GatherRatePermille = FMath::Max(1, Request.GatherRatePermille);
	}
	if (Request.OrbitTicks <= 0)
	{
		return;
	}
	State->bFlyer = true;
	State->bMobile = true;
	State->bCanGather = false;
	State->WeaponDamage = 0.f;
	State->WeaponRange = 0.f;
	State->OrbitCenterX = Request.OrbitCenterX;
	State->OrbitCenterY = Request.OrbitCenterY;
	State->OrbitRadiusUU = FMath::Max(1, Request.OrbitRadiusUU);
	State->OrbitTicksRemaining = FMath::Max(1, Request.OrbitTicks);
	State->OrbitAngle = 0;
	State->Position = FVector(static_cast<float>(Request.OrbitCenterX), static_cast<float>(Request.OrbitCenterY), 600.f);
	if (const FKodUnitBalanceRow* Row = UnitTable ? UnitTable->FindRow<FKodUnitBalanceRow>(FName(KodWardenPaths::Id_IsrPlatform), TEXT("KodOrbital"), false) : nullptr)
	{
		State->MaxHealth = static_cast<float>(FMath::Max(1, Row->MaxHealth));
		State->Health = State->MaxHealth;
		State->SightRadiusUU = FMath::Max(0, Row->Sight);
		State->MoveSpeed = static_cast<float>(FMath::Max(1, Row->MoveSpeed));
	}
	QuantizePose(*State);
	PrevPositions.Add(Id.Value, State->Position);
	PrevYaws.Add(Id.Value, State->YawDegrees);
	UE_LOG(LogTemp, Log, TEXT("KodOrbital Isr Id=%d Center=%d,%d"), Id.Value, State->OrbitCenterX, State->OrbitCenterY);
}

bool UKodSimSubsystem::TryGetOwnedIsr(int32 TeamId, FVector& OutCenter, int32& OutSight, int32& OutId) const
{
	int32 BestId = 0;
	const FKodSimEntityState* Best = nullptr;
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || State->TeamId != TeamId || State->Health <= 0.f || State->OrbitRadiusUU <= 0)
		{
			continue;
		}
		if (!Best || Key > BestId)
		{
			Best = State;
			BestId = Key;
		}
	}
	if (!Best)
	{
		return false;
	}
	OutCenter = FVector(static_cast<float>(Best->OrbitCenterX), static_cast<float>(Best->OrbitCenterY), 0.f);
	OutSight = Best->SightRadiusUU;
	OutId = BestId;
	return true;
}

bool UKodSimSubsystem::UseOrbitalAbility(FKodEntityId CcId, FName AbilityId, FVector Target, FKodEntityId DepotId)
{
	FKodStructureState* Cc = Structures.Find(CcId.Value);
	FKodSimEntityState* CcState = States.Find(CcId.Value);
	const FKodAbilitySpec* Spec = AbilitySpecs.Find(AbilityId);
	if (!Cc || !CcState || !Spec || !Cc->bComplete || CcState->Health <= 0.f)
	{
		return false;
	}
	if (Cc->DefinitionId != FName(KodWardenPaths::Id_CommandCenter))
	{
		return false;
	}
	const FKodTeamTech* Tech = TeamTech.Find(Cc->TeamId);
	if (!Tech || Tech->SatelliteFeed <= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("KodOrbital Reject Reason=Feed"));
		return false;
	}
	int32* Cooldown = CooldownField(*Cc, AbilityId);
	if (!Cooldown)
	{
		return false;
	}
	if (*Cooldown > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("KodOrbital Reject Reason=Cooldown"));
		return false;
	}
	const int32 EnergyCost = FMath::Max(0, Spec->EnergyCost);
	if (CcState->Energy < EnergyCost)
	{
		UE_LOG(LogTemp, Log, TEXT("KodOrbital Reject Reason=Energy"));
		return false;
	}
	CcState->Energy -= EnergyCost;
	*Cooldown = FMath::Max(0, Spec->CooldownTicks);

	if (AbilityId == FName(KodWardenPaths::Id_OrbitalGathererDrop))
	{
		FKodPendingUnitSpawn Spawn;
		Spawn.UnitId = FName(KodWardenPaths::Id_OrbitalGatherer);
		Spawn.TeamId = Cc->TeamId;
		Spawn.Location = Cc->ExitPoint.IsNearlyZero() ? Cc->Position : Cc->ExitPoint;
		Spawn.Yaw = static_cast<float>(GetTeamFacing(Cc->TeamId));
		Spawn.GatherRatePermille = FMath::Max(1, Spec->GatherRatePermille);
		Spawn.SupplyCost = 0;
		PendingSpawns.Add(Spawn);
		UE_LOG(LogTemp, Log, TEXT("KodOrbital Gatherer Cc=%d Rate=%d"), CcId.Value, Spawn.GatherRatePermille);
		return true;
	}

	if (AbilityId == FName(KodWardenPaths::Id_SupplyBoost))
	{
		FKodStructureState* Depot = Structures.Find(DepotId.Value);
		if (!Depot || Depot->TeamId != Cc->TeamId || !Depot->bComplete || Depot->DefinitionId != FName(KodWardenPaths::Id_SupplyDepot) || Depot->BonusSupply > 0)
		{
			CcState->Energy += EnergyCost;
			*Cooldown = 0;
			UE_LOG(LogTemp, Log, TEXT("KodOrbital Reject Reason=Depot"));
			return false;
		}
		Depot->BonusSupply = FMath::Max(0, Spec->SupplyBonus);
		Depot->BonusSupplyTicksRemaining = Spec->DurationTicks > 0 ? Spec->DurationTicks : -1;
		RecalcSupplyCap(Depot->TeamId);
		UE_LOG(LogTemp, Log, TEXT("KodOrbital Supply Depot=%d Bonus=%d Ticks=%d"), DepotId.Value, Depot->BonusSupply, Depot->BonusSupplyTicksRemaining);
		return true;
	}

	if (AbilityId == FName(KodWardenPaths::Id_IsrView))
	{
		FKodPendingUnitSpawn Spawn;
		Spawn.UnitId = FName(KodWardenPaths::Id_IsrPlatform);
		Spawn.TeamId = Cc->TeamId;
		Spawn.OrbitCenterX = FMath::RoundToInt(Target.X);
		Spawn.OrbitCenterY = FMath::RoundToInt(Target.Y);
		Spawn.OrbitRadiusUU = FMath::Max(200, Spec->OrbitRadiusUU);
		Spawn.OrbitTicks = FMath::Max(16, Spec->DurationTicks);
		Spawn.Location = FVector(static_cast<float>(Spawn.OrbitCenterX), static_cast<float>(Spawn.OrbitCenterY), 600.f);
		Spawn.SupplyCost = 0;
		PendingSpawns.Add(Spawn);
		UE_LOG(LogTemp, Log, TEXT("KodOrbital Isr Order Center=%d,%d"), Spawn.OrbitCenterX, Spawn.OrbitCenterY);
		return true;
	}

	CcState->Energy += EnergyCost;
	*Cooldown = 0;
	return false;
}

void UKodSimSubsystem::StepOrbital()
{
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		FKodSimEntityState* State = States.Find(Key);
		if (!State || State->OrbitRadiusUU <= 0 || State->Health <= 0.f)
		{
			continue;
		}
		if (State->OrbitTicksRemaining > 0)
		{
			--State->OrbitTicksRemaining;
		}
		if (State->OrbitTicksRemaining <= 0)
		{
			UE_LOG(LogTemp, Log, TEXT("KodOrbital Down Id=%d Reason=Expired"), Key);
			FKodEntityId Id;
			Id.Value = Key;
			UnregisterEntity(Id);
			continue;
		}
		const int32 Radius = FMath::Max(1, State->OrbitRadiusUU);
		const int32 Speed = FMath::Max(1, FMath::RoundToInt(State->MoveSpeed));
		const int32 Circum = FMath::Max(1, (6283 * Radius) / 1000);
		const int32 Degrees = FMath::Max(1, (Speed * 360) / (Circum * 16));
		State->OrbitAngle = (State->OrbitAngle + Degrees) % 360;
		const float Radians = FMath::DegreesToRadians(static_cast<float>(State->OrbitAngle));
		State->Position.X = static_cast<float>(State->OrbitCenterX) + FMath::Cos(Radians) * static_cast<float>(Radius);
		State->Position.Y = static_cast<float>(State->OrbitCenterY) + FMath::Sin(Radians) * static_cast<float>(Radius);
		State->Position.Z = 600.f;
		QuantizePose(*State);
	}

	TArray<int32> StructureKeys;
	Structures.GetKeys(StructureKeys);
	StructureKeys.Sort();
	for (int32 Key : StructureKeys)
	{
		FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure)
		{
			continue;
		}
		if (bCheatNoCooldown && Structure->TeamId == 0)
		{
			Structure->GathererCd = 0;
			Structure->SupplyCd = 0;
			Structure->IsrCd = 0;
		}
		if (Structure->GathererCd > 0)
		{
			--Structure->GathererCd;
		}
		if (Structure->SupplyCd > 0)
		{
			--Structure->SupplyCd;
		}
		if (Structure->IsrCd > 0)
		{
			--Structure->IsrCd;
		}
		if (Structure->BonusSupply > 0 && Structure->BonusSupplyTicksRemaining > 0)
		{
			--Structure->BonusSupplyTicksRemaining;
			if (Structure->BonusSupplyTicksRemaining == 0)
			{
				Structure->BonusSupply = 0;
				RecalcSupplyCap(Structure->TeamId);
				UE_LOG(LogTemp, Log, TEXT("KodOrbital Supply End Depot=%d"), Key);
			}
		}
	}
}
