#include "Sim/KodSimSubsystem.h"

namespace
{
	bool IsLocomoting(const FKodSimEntityState& State)
	{
		switch (State.Order)
		{
		case EKodSimOrderType::Move:
		case EKodSimOrderType::Attack:
		case EKodSimOrderType::AttackMove:
		case EKodSimOrderType::Patrol:
		case EKodSimOrderType::Follow:
		case EKodSimOrderType::Guard:
		case EKodSimOrderType::Gather:
			return true;
		default:
			return false;
		}
	}
}

void UKodSimSubsystem::StepSeparation()
{
	RebuildAcquireGrid();
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();

	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		FKodSimEntityState* Self = States.Find(Keys[Index]);
		if (!Self || !Self->bMobile || Self->Health <= 0.f)
		{
			continue;
		}
		const int32 SelfRadius = FMath::Max(8, Self->BodyRadiusUU);
		TArray<int32> Nearby;
		CollectAcquireCandidates(Self->Position.X, Self->Position.Y, static_cast<float>(SelfRadius + 96), Nearby);
		for (int32 OtherId : Nearby)
		{
			if (OtherId <= Self->Id.Value)
			{
				continue;
			}
			FKodSimEntityState* Other = States.Find(OtherId);
			if (!Other || !Other->bMobile || Other->Health <= 0.f)
			{
				continue;
			}
			const int32 OtherRadius = FMath::Max(8, Other->BodyRadiusUU);
			const float Needed = static_cast<float>(SelfRadius + OtherRadius);
			float DX = Other->Position.X - Self->Position.X;
			float DY = Other->Position.Y - Self->Position.Y;
			float DistSq = DX * DX + DY * DY;
			if (DistSq >= Needed * Needed)
			{
				continue;
			}
			if (DistSq < 1.f)
			{
				DX = (Self->Id.Value & 1) == 0 ? 1.f : -1.f;
				DY = (Self->Id.Value & 2) == 0 ? 0.f : 1.f;
				DistSq = DX * DX + DY * DY;
			}
			const float Dist = FMath::Sqrt(DistSq);
			const float Overlap = Needed - Dist;
			DX /= Dist;
			DY /= Dist;
			const bool bSelfMoves = IsLocomoting(*Self);
			const bool bOtherMoves = IsLocomoting(*Other);
			float SelfPush = Overlap * 0.5f;
			float OtherPush = Overlap * 0.5f;
			if (bSelfMoves && !bOtherMoves)
			{
				SelfPush = 0.f;
				OtherPush = Overlap;
			}
			else if (bOtherMoves && !bSelfMoves)
			{
				SelfPush = Overlap;
				OtherPush = 0.f;
			}
			Self->Position.X -= DX * SelfPush;
			Self->Position.Y -= DY * SelfPush;
			Other->Position.X += DX * OtherPush;
			Other->Position.Y += DY * OtherPush;
		}
	}

	for (int32 Key : Keys)
	{
		FKodSimEntityState* State = States.Find(Key);
		if (!State || !State->bMobile || State->Health <= 0.f)
		{
			continue;
		}
		if (IsWorldCellBlocked(State->Position))
		{
			const FVector Out = SteerAroundBuildings(*State, FVector(static_cast<float>(FMath::Max(State->BodyRadiusUU, 16)), 0.f, 0.f));
			State->Position += Out;
		}
		if (bHasPlayable)
		{
			const float Radius = static_cast<float>(FMath::Max(8, State->BodyRadiusUU));
			const float MinX = static_cast<float>(PlayMinX * KodBuildGrid::CellUU) + Radius;
			const float MinY = static_cast<float>(PlayMinY * KodBuildGrid::CellUU) + Radius;
			const float MaxX = static_cast<float>((PlayMaxX + 1) * KodBuildGrid::CellUU) - Radius;
			const float MaxY = static_cast<float>((PlayMaxY + 1) * KodBuildGrid::CellUU) - Radius;
			State->Position.X = FMath::Clamp(State->Position.X, MinX, MaxX);
			State->Position.Y = FMath::Clamp(State->Position.Y, MinY, MaxY);
		}
		QuantizePose(*State);
	}
}
