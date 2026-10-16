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
		if (!Self || !Self->bMobile || Self->Health <= 0.f || Self->InsideBunker.IsValid() || Self->OrbitRadiusUU > 0 || IsSoftCollision(*Self))
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
			if (!Other || !Other->bMobile || Other->Health <= 0.f || Other->InsideBunker.IsValid() || IsSoftCollision(*Other))
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
		if (IsCellBlocked(KodBuildGrid::CellOf(State->Position.X), KodBuildGrid::CellOf(State->Position.Y), State->TeamId, true))
		{
			SnapOutOfFootprint(*State);
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

bool UKodSimSubsystem::SnapOutOfFootprint(FKodSimEntityState& State) const
{
	const int32 CellX = KodBuildGrid::CellOf(State.Position.X);
	const int32 CellY = KodBuildGrid::CellOf(State.Position.Y);
	if (!IsCellBlocked(CellX, CellY, State.TeamId, true))
	{
		return false;
	}
	const FKodStructureState* Blocker = nullptr;
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure)
		{
			continue;
		}
		const int32 MaxX = Structure->CellX + FMath::Max(1, Structure->FootprintX);
		const int32 MaxY = Structure->CellY + FMath::Max(1, Structure->FootprintY);
		if (CellX >= Structure->CellX && CellX < MaxX && CellY >= Structure->CellY && CellY < MaxY)
		{
			Blocker = Structure;
			break;
		}
	}
	if (!Blocker)
	{
		return false;
	}
	const float Radius = static_cast<float>(FMath::Max(8, State.BodyRadiusUU));
	const float Left = static_cast<float>(Blocker->CellX * KodBuildGrid::CellUU) - Radius - 1.f;
	const float Right = static_cast<float>((Blocker->CellX + FMath::Max(1, Blocker->FootprintX)) * KodBuildGrid::CellUU) + Radius + 1.f;
	const float Bottom = static_cast<float>(Blocker->CellY * KodBuildGrid::CellUU) - Radius - 1.f;
	const float Top = static_cast<float>((Blocker->CellY + FMath::Max(1, Blocker->FootprintY)) * KodBuildGrid::CellUU) + Radius + 1.f;
	float Best = MAX_flt;
	float BestX = State.Position.X;
	float BestY = State.Position.Y;
	auto Consider = [&Best, &BestX, &BestY, &State](float X, float Y)
	{
		const float DX = X - State.Position.X;
		const float DY = Y - State.Position.Y;
		const float Dist = DX * DX + DY * DY;
		if (Dist < Best)
		{
			Best = Dist;
			BestX = X;
			BestY = Y;
		}
	};
	Consider(Left, State.Position.Y);
	Consider(Right, State.Position.Y);
	Consider(State.Position.X, Bottom);
	Consider(State.Position.X, Top);
	State.Position.X = BestX;
	State.Position.Y = BestY;
	return true;
}
