#include "Sim/KodSimSubsystem.h"

#include "Algo/BinarySearch.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	int64 PackRamp(int32 X, int32 Y)
	{
		return (static_cast<int64>(X) << 32) | static_cast<uint32>(Y);
	}
}

void UKodSimSubsystem::SetGeneralPowers(bool bEnabled)
{
	if (bCompetitive && bEnabled)
	{
		bGeneralPowers = false;
		UE_LOG(LogTemp, Log, TEXT("KodPowers Refused Competitive"));
		return;
	}
	bGeneralPowers = bEnabled;
	UE_LOG(LogTemp, Log, TEXT("KodPowers Enabled=%d"), bGeneralPowers ? 1 : 0);
}

int32 UKodSimSubsystem::GetPowerPoints(int32 TeamId) const
{
	return TeamId == 0 ? PowerPoints0 : PowerPoints1;
}

int32 UKodSimSubsystem::GetPowerCooldown(int32 TeamId, FName PowerId) const
{
	const bool bTeam0 = TeamId == 0;
	if (PowerId == FName(KodWardenPaths::Id_ArtilleryStrike))
	{
		return bTeam0 ? ArtilleryCd0 : ArtilleryCd1;
	}
	if (PowerId == FName(KodWardenPaths::Id_ReconSweep))
	{
		return bTeam0 ? ReconCd0 : ReconCd1;
	}
	if (PowerId == FName(KodWardenPaths::Id_FieldRepair))
	{
		return bTeam0 ? RepairCd0 : RepairCd1;
	}
	return 0;
}

bool UKodSimSubsystem::TryGetPowerSpec(FName PowerId, FKodPowerSpec& OutSpec) const
{
	if (const FKodPowerSpec* Found = PowerSpecs.Find(PowerId))
	{
		OutSpec = *Found;
		return true;
	}
	return false;
}

void UKodSimSubsystem::CopyArtilleryTelegraphs(int32 TeamId, TArray<int32>& OutX, TArray<int32>& OutY, TArray<int32>& OutRadius) const
{
	OutX.Reset();
	OutY.Reset();
	OutRadius.Reset();
	TArray<int32> Order;
	for (int32 Index = 0; Index < ScheduledPowers.Num(); ++Index)
	{
		Order.Add(Index);
	}
	Order.Sort([this](int32 A, int32 B)
	{
		const FKodScheduledPower& Left = ScheduledPowers[A];
		const FKodScheduledPower& Right = ScheduledPowers[B];
		if (Left.X != Right.X)
		{
			return Left.X < Right.X;
		}
		return Left.Y < Right.Y;
	});
	for (int32 Index : Order)
	{
		const FKodScheduledPower& Power = ScheduledPowers[Index];
		if (Power.TeamId != TeamId || Power.PowerId != FName(KodWardenPaths::Id_ArtilleryStrike) || Power.Phase != 0)
		{
			continue;
		}
		OutX.Add(Power.X);
		OutY.Add(Power.Y);
		OutRadius.Add(Power.RadiusUU);
	}
}

bool UKodSimSubsystem::CastPower(int32 TeamId, FName PowerId, FVector Target)
{
	if (!bGeneralPowers || bCompetitive)
	{
		UE_LOG(LogTemp, Log, TEXT("KodPowers Reject Reason=Off"));
		return false;
	}
	const FKodPowerSpec* Spec = PowerSpecs.Find(PowerId);
	if (!Spec)
	{
		return false;
	}
	int32& Points = TeamId == 0 ? PowerPoints0 : PowerPoints1;
	int32* Cooldown = nullptr;
	if (PowerId == FName(KodWardenPaths::Id_ArtilleryStrike))
	{
		Cooldown = TeamId == 0 ? &ArtilleryCd0 : &ArtilleryCd1;
	}
	else if (PowerId == FName(KodWardenPaths::Id_ReconSweep))
	{
		Cooldown = TeamId == 0 ? &ReconCd0 : &ReconCd1;
	}
	else if (PowerId == FName(KodWardenPaths::Id_FieldRepair))
	{
		Cooldown = TeamId == 0 ? &RepairCd0 : &RepairCd1;
	}
	if (!Cooldown || *Cooldown > 0 || Points < Spec->PointCost)
	{
		const TCHAR* Reason = TEXT("Points");
		if (!Cooldown)
		{
			Reason = TEXT("Unknown");
		}
		else if (*Cooldown > 0)
		{
			Reason = TEXT("Cooldown");
		}
		UE_LOG(LogTemp, Log, TEXT("KodPowers Reject Id=%s Reason=%s Points=%d Need=%d Cd=%d"),
			*PowerId.ToString(),
			Reason,
			Points,
			Spec->PointCost,
			Cooldown ? *Cooldown : -1);
		return false;
	}
	Points -= Spec->PointCost;
	*Cooldown = FMath::Max(0, Spec->CooldownTicks);
	FKodScheduledPower Power;
	Power.TeamId = TeamId;
	Power.PowerId = PowerId;
	Power.X = FMath::RoundToInt(Target.X);
	Power.Y = FMath::RoundToInt(Target.Y);
	Power.TicksLeft = FMath::Max(0, Spec->DelayTicks);
	Power.Phase = 0;
	Power.RadiusUU = FMath::Max(1, Spec->RadiusUU);
	Power.Amount = Spec->Amount;
	Power.DurationTicks = FMath::Max(0, Spec->DurationTicks);
	ScheduledPowers.Add(Power);
	ScheduledPowers.Sort([](const FKodScheduledPower& A, const FKodScheduledPower& B)
	{
		if (A.TeamId != B.TeamId)
		{
			return A.TeamId < B.TeamId;
		}
		if (A.X != B.X)
		{
			return A.X < B.X;
		}
		return A.Y < B.Y;
	});
	UE_LOG(LogTemp, Log, TEXT("KodPowers Cast Id=%s Team=%d X=%d Y=%d"), *PowerId.ToString(), TeamId, Power.X, Power.Y);
	return true;
}

void UKodSimSubsystem::AwardPowerPoints(int32 TeamId, int32 Amount)
{
	if (!bGeneralPowers || Amount <= 0)
	{
		return;
	}
	if (TeamId == 0)
	{
		PowerPoints0 += Amount;
	}
	else if (TeamId == 1)
	{
		PowerPoints1 += Amount;
	}
}

void UKodSimSubsystem::CopyReconPulses(int32 TeamId, TArray<int32>& OutX, TArray<int32>& OutY, TArray<int32>& OutRadius, TArray<int32>& OutTicksLeft, TArray<int32>& OutDuration) const
{
	OutX.Reset();
	OutY.Reset();
	OutRadius.Reset();
	OutTicksLeft.Reset();
	OutDuration.Reset();
	TArray<int32> Order;
	for (int32 Index = 0; Index < ScheduledPowers.Num(); ++Index)
	{
		Order.Add(Index);
	}
	Order.Sort([this](int32 A, int32 B)
	{
		const FKodScheduledPower& Left = ScheduledPowers[A];
		const FKodScheduledPower& Right = ScheduledPowers[B];
		if (Left.X != Right.X)
		{
			return Left.X < Right.X;
		}
		return Left.Y < Right.Y;
	});
	for (int32 Index : Order)
	{
		const FKodScheduledPower& Power = ScheduledPowers[Index];
		if (Power.TeamId != TeamId || Power.PowerId != FName(KodWardenPaths::Id_ReconSweep) || Power.Phase != 1)
		{
			continue;
		}
		OutX.Add(Power.X);
		OutY.Add(Power.Y);
		OutRadius.Add(Power.RadiusUU);
		OutTicksLeft.Add(Power.TicksLeft);
		OutDuration.Add(FMath::Max(1, Power.DurationTicks));
	}
}

void UKodSimSubsystem::GrantStartingPowerIncome()
{
	if (bPowerStartGranted || bTestIsolation || !bGeneralPowers)
	{
		return;
	}
	FKodPowerSpec Income;
	if (!TryGetPowerSpec(FName(KodWardenPaths::Id_PowerIncome), Income) || Income.Amount <= 0)
	{
		return;
	}
	AwardPowerPoints(0, Income.Amount);
	AwardPowerPoints(1, Income.Amount);
	bPowerStartGranted = true;
	UE_LOG(LogTemp, Log, TEXT("KodPowers Income Start=%d"), Income.Amount);
}

void UKodSimSubsystem::StepPowers()
{
	if (bGeneralPowers && !bTestIsolation)
	{
		FKodPowerSpec Income;
		if (TryGetPowerSpec(FName(KodWardenPaths::Id_PowerIncome), Income))
		{
			if (Income.CooldownTicks > 0 && SimTickIndex > 0 && (SimTickIndex % Income.CooldownTicks) == 0)
			{
				AwardPowerPoints(0, 1);
				AwardPowerPoints(1, 1);
			}
		}
	}
	if (bCheatNoCooldown)
	{
		ArtilleryCd0 = 0;
		ReconCd0 = 0;
		RepairCd0 = 0;
	}
	if (ArtilleryCd0 > 0) { --ArtilleryCd0; }
	if (ArtilleryCd1 > 0) { --ArtilleryCd1; }
	if (ReconCd0 > 0) { --ReconCd0; }
	if (ReconCd1 > 0) { --ReconCd1; }
	if (RepairCd0 > 0) { --RepairCd0; }
	if (RepairCd1 > 0) { --RepairCd1; }

	for (int32 Index = ScheduledPowers.Num() - 1; Index >= 0; --Index)
	{
		FKodScheduledPower& Power = ScheduledPowers[Index];
		if (Power.Phase == 1)
		{
			if (Power.TicksLeft > 0)
			{
				--Power.TicksLeft;
			}
			if (Power.TicksLeft <= 0)
			{
				ScheduledPowers.RemoveAt(Index);
			}
			continue;
		}
		if (Power.TicksLeft > 0)
		{
			--Power.TicksLeft;
			continue;
		}
		const FVector Center(static_cast<float>(Power.X), static_cast<float>(Power.Y), 0.f);
		const float RadiusSq = static_cast<float>(Power.RadiusUU) * static_cast<float>(Power.RadiusUU);
		if (Power.PowerId == FName(KodWardenPaths::Id_ArtilleryStrike))
		{
			TArray<int32> Keys;
			States.GetKeys(Keys);
			Keys.Sort();
			for (int32 Key : Keys)
			{
				FKodSimEntityState* State = States.Find(Key);
				if (!State || State->Health <= 0.f || State->TeamId == Power.TeamId || State->InsideBunker.IsValid())
				{
					continue;
				}
				if (FVector::DistSquared2D(State->Position, Center) > RadiusSq)
				{
					continue;
				}
				const int32 DealtInt = FMath::Max(0, Power.Amount);
				if (DealtInt <= 0)
				{
					continue;
				}
				State->Health = FMath::Max(0.f, State->Health - static_cast<float>(DealtInt));
				if (Power.TeamId == 0)
				{
					DamageDealt0 += DealtInt;
				}
				else
				{
					DamageDealt1 += DealtInt;
				}
				if (State->TeamId == 0)
				{
					DamageTaken0 += DealtInt;
				}
				else
				{
					DamageTaken1 += DealtInt;
				}
				AActor* TargetActor = nullptr;
				if (const TWeakObjectPtr<AActor>* Found = Entities.Find(Key))
				{
					TargetActor = Found->Get();
				}
				const float HealthLeft = State->Health;
				const FVector HitAt = State->Position;
				OnUnitHit.Broadcast(nullptr, TargetActor, HealthLeft, Key);
				PushJuiceText(HitAt, DealtInt, 0);
				if (HealthLeft <= 0.f)
				{
					FinishEntityDeath(*State, Power.TeamId);
				}
			}
			UE_LOG(LogTemp, Log, TEXT("KodPowers Artillery X=%d Y=%d"), Power.X, Power.Y);
			RaiseCue(FName(TEXT("artillery_strike")), Center, true);
			ScheduledPowers.RemoveAt(Index);
			continue;
		}
		if (Power.PowerId == FName(KodWardenPaths::Id_FieldRepair))
		{
			TArray<int32> Keys;
			Structures.GetKeys(Keys);
			Keys.Sort();
			for (int32 Key : Keys)
			{
				const FKodStructureState* Structure = Structures.Find(Key);
				FKodSimEntityState* Combat = States.Find(Key);
				if (!Structure || !Combat || Structure->TeamId != Power.TeamId || Combat->Health <= 0.f)
				{
					continue;
				}
				if (FVector::DistSquared2D(Structure->Position, Center) > RadiusSq)
				{
					continue;
				}
				Combat->Health = FMath::Min(Combat->MaxHealth, Combat->Health + static_cast<float>(FMath::Max(0, Power.Amount)));
			}
			UE_LOG(LogTemp, Log, TEXT("KodPowers Repair X=%d Y=%d"), Power.X, Power.Y);
			ScheduledPowers.RemoveAt(Index);
			continue;
		}
		if (Power.PowerId == FName(KodWardenPaths::Id_ReconSweep))
		{
			Power.Phase = 1;
			Power.TicksLeft = FMath::Max(1, Power.DurationTicks);
			UE_LOG(LogTemp, Log, TEXT("KodPowers Recon X=%d Y=%d"), Power.X, Power.Y);
			RaiseCue(FName(TEXT("recon_sweep")), Center, true);
		}
	}
}

void UKodSimSubsystem::ApplyReconVision()
{
	for (const FKodScheduledPower& Power : ScheduledPowers)
	{
		if (Power.Phase != 1 || Power.PowerId != FName(KodWardenPaths::Id_ReconSweep))
		{
			continue;
		}
		TArray<uint8>* Cells = TeamVision.Find(Power.TeamId);
		if (!Cells || VisionWidth <= 0)
		{
			continue;
		}
		const int32 RadiusCells = FMath::Max(1, FMath::DivideAndRoundUp(Power.RadiusUU, KodBuildGrid::CellUU));
		const int32 OriginX = KodBuildGrid::CellOf(static_cast<float>(Power.X));
		const int32 OriginY = KodBuildGrid::CellOf(static_cast<float>(Power.Y));
		for (int32 DY = -RadiusCells; DY <= RadiusCells; ++DY)
		{
			for (int32 DX = -RadiusCells; DX <= RadiusCells; ++DX)
			{
				if (DX * DX + DY * DY > RadiusCells * RadiusCells)
				{
					continue;
				}
				const int32 Index = VisionCellIndex(OriginX + DX, OriginY + DY);
				if (Cells->IsValidIndex(Index))
				{
					(*Cells)[Index] = 2;
				}
			}
		}
	}
}

int32 UKodSimSubsystem::GetMapCellHeight(int32 CellX, int32 CellY) const
{
	return RampCells.Contains(PackRamp(CellX, CellY)) ? 1 : 0;
}

void UKodSimSubsystem::RegisterRampCell(int32 CellX, int32 CellY)
{
	const int64 Key = PackRamp(CellX, CellY);
	RampCells.AddUnique(Key);
	RampCells.Sort();
	UE_LOG(LogTemp, Log, TEXT("KodRamp Cell=%d,%d"), CellX, CellY);
}

void UKodSimSubsystem::SetCliffBlockCells(TArray<int64> Cells)
{
	Cells.Sort();
	DressBlockCells.Reset();
	for (int64 Cell : Cells)
	{
		if (DressBlockCells.Num() == 0 || DressBlockCells.Last() != Cell)
		{
			DressBlockCells.Add(Cell);
		}
	}
}

bool UKodSimSubsystem::IsDressBlockCell(int32 CellX, int32 CellY) const
{
	return Algo::BinarySearch(DressBlockCells, PackRamp(CellX, CellY)) != INDEX_NONE;
}

void UKodSimSubsystem::SetVisionBlockCells(TArray<int64> Cells)
{
	Cells.Sort();
	VisionBlockCells.Reset();
	for (int64 Cell : Cells)
	{
		if (VisionBlockCells.Num() == 0 || VisionBlockCells.Last() != Cell)
		{
			VisionBlockCells.Add(Cell);
		}
	}
}

bool UKodSimSubsystem::IsVisionBlockCell(int32 CellX, int32 CellY) const
{
	return Algo::BinarySearch(VisionBlockCells, PackRamp(CellX, CellY)) != INDEX_NONE;
}

bool UKodSimSubsystem::VisionRayHitsBlock(int32 X0, int32 Y0, int32 X1, int32 Y1) const
{
	int32 X = X0;
	int32 Y = Y0;
	const int32 DX = FMath::Abs(X1 - X0);
	const int32 DY = FMath::Abs(Y1 - Y0);
	const int32 StepX = X0 < X1 ? 1 : -1;
	const int32 StepY = Y0 < Y1 ? 1 : -1;
	int32 Err = DX - DY;
	while (X != X1 || Y != Y1)
	{
		const int32 Err2 = Err * 2;
		if (Err2 > -DY)
		{
			Err -= DY;
			X += StepX;
		}
		if (Err2 < DX)
		{
			Err += DX;
			Y += StepY;
		}
		if (X == X1 && Y == Y1)
		{
			break;
		}
		if (IsVisionBlockCell(X, Y))
		{
			return true;
		}
	}
	return false;
}

void UKodSimSubsystem::SetLayoutSockets(const TArray<FVector2D>& Sockets)
{
	LayoutSockets = Sockets;
}

bool UKodSimSubsystem::LayoutSocketsMatch() const
{
	const FVector2D Wanted[] = {
		FVector2D(-8300.f, -8300.f),
		FVector2D(8300.f, 8300.f),
		FVector2D(-8300.f, -2900.f),
		FVector2D(8300.f, 2900.f),
		FVector2D(100.f, 100.f)
	};
	if (LayoutSockets.Num() < UE_ARRAY_COUNT(Wanted))
	{
		return false;
	}
	for (const FVector2D& Point : Wanted)
	{
		bool bFound = false;
		for (const FVector2D& Socket : LayoutSockets)
		{
			if (FVector2D::Distance(Socket, Point) <= 1.f)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			return false;
		}
	}
	return true;
}

bool UKodSimSubsystem::IsRampCell(int32 CellX, int32 CellY) const
{
	return RampCells.Contains(PackRamp(CellX, CellY));
}

bool UKodSimSubsystem::IsRampWalled(int32 CellX, int32 CellY, int32 ForTeam) const
{
	if (!IsRampCell(CellX, CellY))
	{
		return false;
	}
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->bAirborne || !Structure->bComplete)
		{
			continue;
		}
		const bool bWall = Structure->DefinitionId == FName(KodWardenPaths::Id_SupplyDepot)
			|| Structure->DefinitionId == FName(KodWardenPaths::Id_Barracks);
		if (!bWall)
		{
			continue;
		}
		const int32 MaxX = Structure->CellX + FMath::Max(1, Structure->FootprintX);
		const int32 MaxY = Structure->CellY + FMath::Max(1, Structure->FootprintY);
		if (CellX < Structure->CellX || CellX >= MaxX || CellY < Structure->CellY || CellY >= MaxY)
		{
			continue;
		}
		if (Structure->bLowered && Structure->bCanLower)
		{
			return false;
		}
		return true;
	}
	return false;
}

void UKodSimSubsystem::CollectTaggedRamps()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->ActorHasTag(FName(TEXT("KodRamp"))))
		{
			continue;
		}
		RegisterRampCell(KodBuildGrid::CellOf(Actor->GetActorLocation().X), KodBuildGrid::CellOf(Actor->GetActorLocation().Y));
	}
}
