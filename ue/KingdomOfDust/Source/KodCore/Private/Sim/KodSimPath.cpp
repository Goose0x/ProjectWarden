#include "Sim/KodSimSubsystem.h"

#include "Algo/Reverse.h"

namespace
{
	int64 PackCell(int32 X, int32 Y)
	{
		return (static_cast<int64>(static_cast<uint32>(X)) << 32) | static_cast<uint32>(Y);
	}

	int32 Octile(int32 X0, int32 Y0, int32 X1, int32 Y1)
	{
		const int32 DX = FMath::Abs(X1 - X0);
		const int32 DY = FMath::Abs(Y1 - Y0);
		return 10 * (DX + DY) + 4 * FMath::Min(DX, DY);
	}
}

bool UKodSimSubsystem::IsPathCellClear(int32 X0, int32 Y0, int32 X1, int32 Y1, int32 TeamId) const
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
		const int32 Twice = Err * 2;
		if (Twice > -DY)
		{
			Err -= DY;
			X += StepX;
		}
		if (Twice < DX)
		{
			Err += DX;
			Y += StepY;
		}
		if ((X != X1 || Y != Y1) && IsCellBlocked(X, Y, TeamId, true))
		{
			return false;
		}
	}
	return true;
}

int32 UKodSimSubsystem::PlanMovePath(FVector From, FVector To, int32 TeamId, bool bStore)
{
	int32 StartX = KodBuildGrid::CellOf(From.X);
	int32 StartY = KodBuildGrid::CellOf(From.Y);
	int32 GoalX = KodBuildGrid::CellOf(To.X);
	int32 GoalY = KodBuildGrid::CellOf(To.Y);
	if (StartX == GoalX && StartY == GoalY)
	{
		return 0;
	}
	if (IsPathCellClear(StartX, StartY, GoalX, GoalY, TeamId))
	{
		return 0;
	}

	if (IsCellBlocked(StartX, StartY, TeamId, true))
	{
		bool bFound = false;
		for (int32 Radius = 1; Radius <= 3 && !bFound; ++Radius)
		{
			for (int32 X = StartX - Radius; X <= StartX + Radius && !bFound; ++X)
			{
				for (int32 Y = StartY - Radius; Y <= StartY + Radius; ++Y)
				{
					if (!IsCellBlocked(X, Y, TeamId, true))
					{
						StartX = X;
						StartY = Y;
						bFound = true;
						break;
					}
				}
			}
		}
	}
	if (IsCellBlocked(GoalX, GoalY, TeamId, true))
	{
		bool bFound = false;
		int32 Best = MAX_int32;
		int32 BestX = GoalX;
		int32 BestY = GoalY;
		for (int32 Radius = 1; Radius <= 4; ++Radius)
		{
			for (int32 X = GoalX - Radius; X <= GoalX + Radius; ++X)
			{
				for (int32 Y = GoalY - Radius; Y <= GoalY + Radius; ++Y)
				{
					if (IsCellBlocked(X, Y, TeamId, true))
					{
						continue;
					}
					const int32 Score = FMath::Abs(X - GoalX) + FMath::Abs(Y - GoalY);
					if (!bFound || Score < Best || (Score == Best && (X < BestX || (X == BestX && Y < BestY))))
					{
						bFound = true;
						Best = Score;
						BestX = X;
						BestY = Y;
					}
				}
			}
			if (bFound)
			{
				break;
			}
		}
		GoalX = BestX;
		GoalY = BestY;
	}

	struct FNode
	{
		int32 X = 0;
		int32 Y = 0;
		int32 G = 0;
		int32 F = 0;
		int32 Parent = INDEX_NONE;
	};

	TArray<FNode> Nodes;
	TArray<int32> Heap;
	TMap<int64, int32> BestG;
	auto Worse = [&Nodes](int32 A, int32 B)
	{
		const FNode& Left = Nodes[A];
		const FNode& Right = Nodes[B];
		if (Left.F != Right.F)
		{
			return Left.F > Right.F;
		}
		if (Left.G != Right.G)
		{
			return Left.G > Right.G;
		}
		if (Left.X != Right.X)
		{
			return Left.X > Right.X;
		}
		return Left.Y > Right.Y;
	};
	auto HeapPush = [&Heap, &Worse](int32 Index)
	{
		Heap.Add(Index);
		int32 Slot = Heap.Num() - 1;
		while (Slot > 0)
		{
			const int32 Parent = (Slot - 1) / 2;
			if (!Worse(Heap[Parent], Heap[Slot]))
			{
				break;
			}
			Swap(Heap[Parent], Heap[Slot]);
			Slot = Parent;
		}
	};
	auto HeapPop = [&Heap, &Worse]()
	{
		const int32 Top = Heap[0];
		Heap[0] = Heap.Last();
		Heap.Pop(EAllowShrinking::No);
		int32 Slot = 0;
		while (true)
		{
			const int32 Left = Slot * 2 + 1;
			const int32 Right = Left + 1;
			int32 Best = Slot;
			if (Left < Heap.Num() && Worse(Heap[Best], Heap[Left]))
			{
				Best = Left;
			}
			if (Right < Heap.Num() && Worse(Heap[Best], Heap[Right]))
			{
				Best = Right;
			}
			if (Best == Slot)
			{
				break;
			}
			Swap(Heap[Slot], Heap[Best]);
			Slot = Best;
		}
		return Top;
	};
	FNode Start;
	Start.X = StartX;
	Start.Y = StartY;
	Start.G = 0;
	Start.F = Octile(StartX, StartY, GoalX, GoalY);
	Start.Parent = INDEX_NONE;
	Nodes.Add(Start);
	HeapPush(0);
	BestG.Add(PackCell(StartX, StartY), 0);

	constexpr int32 MaxExpand = 16384;
	int32 Expanded = 0;
	int32 Found = INDEX_NONE;
	const int32 Neighbors[8][3] = {
		{1, 0, 10}, {-1, 0, 10}, {0, 1, 10}, {0, -1, 10},
		{1, 1, 14}, {1, -1, 14}, {-1, 1, 14}, {-1, -1, 14}
	};

	while (Heap.Num() > 0 && Expanded < MaxExpand)
	{
		const int32 Current = HeapPop();
		const FNode Here = Nodes[Current];
		if (const int32* Known = BestG.Find(PackCell(Here.X, Here.Y)))
		{
			if (*Known < Here.G)
			{
				continue;
			}
		}
		++Expanded;
		if (Here.X == GoalX && Here.Y == GoalY)
		{
			Found = Current;
			break;
		}
		for (int32 NeighborIndex = 0; NeighborIndex < 8; ++NeighborIndex)
		{
			const int32 NextX = Here.X + Neighbors[NeighborIndex][0];
			const int32 NextY = Here.Y + Neighbors[NeighborIndex][1];
			const int32 StepCost = Neighbors[NeighborIndex][2];
			if (IsCellBlocked(NextX, NextY, TeamId, true))
			{
				continue;
			}
			if (StepCost == 14
				&& (IsCellBlocked(Here.X + Neighbors[NeighborIndex][0], Here.Y, TeamId, true)
					|| IsCellBlocked(Here.X, Here.Y + Neighbors[NeighborIndex][1], TeamId, true)))
			{
				continue;
			}
			const int32 NextG = Here.G + StepCost;
			const int64 Key = PackCell(NextX, NextY);
			if (const int32* Known = BestG.Find(Key))
			{
				if (*Known <= NextG)
				{
					continue;
				}
			}
			BestG.Add(Key, NextG);
			FNode Next;
			Next.X = NextX;
			Next.Y = NextY;
			Next.G = NextG;
			Next.F = NextG + Octile(NextX, NextY, GoalX, GoalY);
			Next.Parent = Current;
			const int32 Index = Nodes.Add(Next);
			HeapPush(Index);
		}
	}

	if (Found == INDEX_NONE)
	{
		UE_LOG(LogTemp, Log, TEXT("KodPath Miss FromCell=%d,%d ToCell=%d,%d From=%.0f,%.0f To=%.0f,%.0f"),
			StartX,
			StartY,
			GoalX,
			GoalY,
			From.X,
			From.Y,
			To.X,
			To.Y);
		return -1;
	}
	if (!bStore)
	{
		return 0;
	}

	TArray<int32> RawX;
	TArray<int32> RawY;
	for (int32 Cursor = Found; Cursor != INDEX_NONE; Cursor = Nodes[Cursor].Parent)
	{
		RawX.Add(Nodes[Cursor].X);
		RawY.Add(Nodes[Cursor].Y);
	}
	Algo::Reverse(RawX);
	Algo::Reverse(RawY);

	TArray<int32> SmoothX;
	TArray<int32> SmoothY;
	int32 Anchor = 0;
	SmoothX.Add(RawX[0]);
	SmoothY.Add(RawY[0]);
	while (Anchor < RawX.Num() - 1)
	{
		int32 Farthest = Anchor + 1;
		for (int32 Index = RawX.Num() - 1; Index > Anchor; --Index)
		{
			if (IsPathCellClear(RawX[Anchor], RawY[Anchor], RawX[Index], RawY[Index], TeamId))
			{
				Farthest = Index;
				break;
			}
		}
		SmoothX.Add(RawX[Farthest]);
		SmoothY.Add(RawY[Farthest]);
		Anchor = Farthest;
	}

	while (StoredPaths.Num() >= 256)
	{
		int32 Worst = 0;
		for (int32 Index = 1; Index < StoredPaths.Num(); ++Index)
		{
			const FKodStoredPath& Candidate = StoredPaths[Index];
			const FKodStoredPath& Current = StoredPaths[Worst];
			if (Candidate.LastUsedTick < Current.LastUsedTick
				|| (Candidate.LastUsedTick == Current.LastUsedTick && Candidate.Id < Current.Id))
			{
				Worst = Index;
			}
		}
		StoredPaths.RemoveAt(Worst);
	}
	FKodStoredPath Path;
	Path.LastUsedTick = SimTickIndex;
	Path.Id = NextPathId++;
	if (NextPathId < 1)
	{
		NextPathId = 1;
	}
	Path.CellX = SmoothX;
	Path.CellY = SmoothY;
	StoredPaths.Add(Path);
	UE_LOG(LogTemp, Log, TEXT("KodPath Plan From=%d,%d To=%d,%d Len=%d Id=%d"),
		StartX,
		StartY,
		GoalX,
		GoalY,
		SmoothX.Num(),
		Path.Id);
	return Path.Id;
}

bool UKodSimSubsystem::HasMoveRoute(FVector From, FVector To, int32 TeamId)
{
	return PlanMovePath(From, To, TeamId, false) >= 0;
}

void UKodSimSubsystem::InvalidatePathsOverFootprint(int32 CellX, int32 CellY, int32 SizeX, int32 SizeY)
{
	for (int32 Index = StoredPaths.Num() - 1; Index >= 0; --Index)
	{
		const FKodStoredPath& Path = StoredPaths[Index];
		bool bCrosses = false;
		for (int32 Step = 0; Step < Path.CellX.Num(); ++Step)
		{
			const int32 X = Path.CellX[Step];
			const int32 Y = Path.CellY[Step];
			if (X >= CellX && X < CellX + SizeX && Y >= CellY && Y < CellY + SizeY)
			{
				bCrosses = true;
				break;
			}
		}
		if (bCrosses)
		{
			UE_LOG(LogTemp, Log, TEXT("KodPath Invalidate Id=%d"), Path.Id);
			StoredPaths.RemoveAt(Index);
		}
	}
}

FVector UKodSimSubsystem::GetSteeredGoal(FKodSimEntityState& State)
{
	if (State.PathId == 0)
	{
		return State.MoveTarget;
	}
	FKodStoredPath* Path = nullptr;
	for (FKodStoredPath& Stored : StoredPaths)
	{
		if (Stored.Id == State.PathId)
		{
			Path = &Stored;
			break;
		}
	}
	if (!Path || Path->CellX.Num() == 0)
	{
		State.PathId = FMath::Max(0, PlanMovePath(State.Position, State.MoveTarget, State.TeamId));
		State.PathCursor = 0;
		UE_LOG(LogTemp, Log, TEXT("KodPath Replan Unit=%d Path=%d"), State.Id.Value, State.PathId);
		if (State.PathId == 0)
		{
			return State.MoveTarget;
		}
		for (FKodStoredPath& Stored : StoredPaths)
		{
			if (Stored.Id == State.PathId)
			{
				Path = &Stored;
				break;
			}
		}
		if (!Path)
		{
			return State.MoveTarget;
		}
	}
	Path->LastUsedTick = SimTickIndex;
	while (State.PathCursor < Path->CellX.Num())
	{
		const float X = KodBuildGrid::CellCenter(Path->CellX[State.PathCursor]) + static_cast<float>(State.PathOffsetX);
		const float Y = KodBuildGrid::CellCenter(Path->CellY[State.PathCursor]) + static_cast<float>(State.PathOffsetY);
		const float DX = X - State.Position.X;
		const float DY = Y - State.Position.Y;
		if (DX * DX + DY * DY > FMath::Square(120.f))
		{
			return FVector(X, Y, State.Position.Z);
		}
		++State.PathCursor;
	}
	State.PathId = 0;
	return State.MoveTarget;
}
