#include "Vision/KodFogOfWarSubsystem.h"
#include "GameFramework/Actor.h"

void UKodFogOfWarSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ConfigureGrid(CellSize, GridHalfExtent);
}

void UKodFogOfWarSubsystem::Deinitialize()
{
	TeamReveal.Reset();
	Super::Deinitialize();
}

void UKodFogOfWarSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	ConfigureGrid(CellSize, GridHalfExtent);
}

void UKodFogOfWarSubsystem::ConfigureGrid(float InCellSize, int32 InHalfExtent)
{
	CellSize = FMath::Max(InCellSize, 1.f);
	GridHalfExtent = FMath::Max(InHalfExtent, 1);
	GridWidth = GridHalfExtent * 2;
	TeamReveal.Reset();
}

int32 UKodFogOfWarSubsystem::CoordToIndex(int32 X, int32 Y) const
{
	const int32 LocalX = X + GridHalfExtent;
	const int32 LocalY = Y + GridHalfExtent;
	if (LocalX < 0 || LocalY < 0 || LocalX >= GridWidth || LocalY >= GridWidth)
	{
		return INDEX_NONE;
	}
	return LocalY * GridWidth + LocalX;
}

void UKodFogOfWarSubsystem::WorldToCell(FVector WorldLocation, int32& OutX, int32& OutY) const
{
	OutX = FMath::FloorToInt(WorldLocation.X / CellSize);
	OutY = FMath::FloorToInt(WorldLocation.Y / CellSize);
}

void UKodFogOfWarSubsystem::RevealAt(FVector WorldLocation, float Radius, int32 TeamId)
{
	TArray<uint8>& Bits = TeamReveal.FindOrAdd(TeamId);
	const int32 NumCells = GridWidth * GridWidth;
	if (Bits.Num() != NumCells)
	{
		Bits.SetNumZeroed(NumCells);
	}

	int32 CenterX, CenterY;
	WorldToCell(WorldLocation, CenterX, CenterY);
	const int32 CellRadius = FMath::CeilToInt(Radius / CellSize);

	for (int32 DY = -CellRadius; DY <= CellRadius; ++DY)
	{
		for (int32 DX = -CellRadius; DX <= CellRadius; ++DX)
		{
			if (DX * DX + DY * DY > CellRadius * CellRadius)
			{
				continue;
			}
			const int32 Idx = CoordToIndex(CenterX + DX, CenterY + DY);
			if (Idx != INDEX_NONE)
			{
				Bits[Idx] = 1;
			}
		}
	}
}

bool UKodFogOfWarSubsystem::IsLocationVisible(FVector WorldLocation, int32 TeamId) const
{
	const TArray<uint8>* Bits = TeamReveal.Find(TeamId);
	if (!Bits || Bits->Num() == 0)
	{
		return false;
	}
	int32 X, Y;
	WorldToCell(WorldLocation, X, Y);
	const int32 Idx = CoordToIndex(X, Y);
	return Idx != INDEX_NONE && (*Bits)[Idx] != 0;
}

bool UKodFogOfWarSubsystem::IsActorVisible(AActor* Actor, int32 ObserverTeamId) const
{
	if (!Actor)
	{
		return false;
	}
	return IsLocationVisible(Actor->GetActorLocation(), ObserverTeamId);
}

void UKodFogOfWarSubsystem::ResetVision(int32 TeamId)
{
	if (TArray<uint8>* Bits = TeamReveal.Find(TeamId))
	{
		Bits->Init(0, Bits->Num());
	}
}
