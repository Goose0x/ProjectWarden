#include "Sim/KodSimSubsystem.h"

namespace
{
	constexpr uint8 VisionExplored = 1;
	constexpr uint8 VisionVisible = 2;
}

void UKodSimSubsystem::EnsureVisionStorage()
{
	if (!bHasPlayable)
	{
		return;
	}
	const int32 Width = PlayMaxX - PlayMinX + 1;
	const int32 Height = PlayMaxY - PlayMinY + 1;
	if (Width <= 0 || Height <= 0)
	{
		return;
	}
	const int64 Count64 = static_cast<int64>(Width) * static_cast<int64>(Height);
	if (Count64 <= 0 || Count64 > 4000000)
	{
		return;
	}
	if (VisionWidth == Width && VisionHeight == Height && VisionOriginX == PlayMinX && VisionOriginY == PlayMinY)
	{
		return;
	}
	VisionOriginX = PlayMinX;
	VisionOriginY = PlayMinY;
	VisionWidth = Width;
	VisionHeight = Height;
	const int32 Count = static_cast<int32>(Count64);
	TArray<int32> Teams;
	TeamVision.GetKeys(Teams);
	for (int32 TeamId : Teams)
	{
		TeamVision.FindChecked(TeamId).Init(0, Count);
	}
}

int32 UKodSimSubsystem::VisionCellIndex(int32 CellX, int32 CellY) const
{
	if (VisionWidth <= 0 || VisionHeight <= 0)
	{
		return INDEX_NONE;
	}
	const int32 LocalX = CellX - VisionOriginX;
	const int32 LocalY = CellY - VisionOriginY;
	if (LocalX < 0 || LocalY < 0 || LocalX >= VisionWidth || LocalY >= VisionHeight)
	{
		return INDEX_NONE;
	}
	return LocalY * VisionWidth + LocalX;
}

uint8 UKodSimSubsystem::GetTeamVision(int32 TeamId, float WorldX, float WorldY) const
{
	const TArray<uint8>* Grid = TeamVision.Find(TeamId);
	if (!Grid)
	{
		return 0;
	}
	const int32 Index = VisionCellIndex(KodBuildGrid::CellOf(WorldX), KodBuildGrid::CellOf(WorldY));
	if (!Grid->IsValidIndex(Index))
	{
		return 0;
	}
	return (*Grid)[Index];
}

bool UKodSimSubsystem::CanTeamSeeWorld(int32 TeamId, float WorldX, float WorldY) const
{
	if (bAiCheatVision && TeamId == 1)
	{
		return true;
	}
	if (bCheatReveal && TeamId == 0)
	{
		return true;
	}
	return GetTeamVision(TeamId, WorldX, WorldY) == VisionVisible;
}

bool UKodSimSubsystem::TryGetLastSeenPoint(int32 TeamId, int32 EntityId, FVector& OutPoint) const
{
	if (const TArray<FKodSeenStructure>* StructuresSeen = SeenStructures.Find(TeamId))
	{
		for (const FKodSeenStructure& Entry : *StructuresSeen)
		{
			if (Entry.Id == EntityId)
			{
				OutPoint = FVector(static_cast<float>(Entry.PosX), static_cast<float>(Entry.PosY), 0.f);
				return true;
			}
		}
	}
	if (const TArray<FKodSeenUnit>* UnitsSeen = SeenUnits.Find(TeamId))
	{
		for (const FKodSeenUnit& Entry : *UnitsSeen)
		{
			if (Entry.Id == EntityId)
			{
				OutPoint = FVector(static_cast<float>(Entry.PosX), static_cast<float>(Entry.PosY), 0.f);
				return true;
			}
		}
	}
	return false;
}

void UKodSimSubsystem::CopySeenStructureBlips(int32 TeamId, TArray<int32>& OutTeam, TArray<int32>& OutX, TArray<int32>& OutY, TArray<int32>& OutFootX, TArray<int32>& OutFootY) const
{
	OutTeam.Reset();
	OutX.Reset();
	OutY.Reset();
	OutFootX.Reset();
	OutFootY.Reset();
	const TArray<FKodSeenStructure>* Found = SeenStructures.Find(TeamId);
	if (!Found)
	{
		return;
	}
	for (const FKodSeenStructure& Seen : *Found)
	{
		OutTeam.Add(Seen.TeamId);
		OutX.Add(Seen.PosX);
		OutY.Add(Seen.PosY);
		OutFootX.Add(Seen.FootprintX);
		OutFootY.Add(Seen.FootprintY);
	}
}

void UKodSimSubsystem::StepVision()
{
	EnsureVisionStorage();
	if (VisionWidth <= 0 || VisionHeight <= 0)
	{
		return;
	}
	const int32 CellCount = VisionWidth * VisionHeight;

	TArray<int32> EntityKeys;
	States.GetKeys(EntityKeys);
	EntityKeys.Sort();

	TArray<int32> Teams;
	TeamVision.GetKeys(Teams);
	for (int32 Key : EntityKeys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (State && State->Health > 0.f)
		{
			Teams.AddUnique(State->TeamId);
		}
	}
	Teams.Sort();

	for (int32 TeamId : Teams)
	{
		TArray<uint8>& Cells = TeamVision.FindOrAdd(TeamId);
		if (Cells.Num() != CellCount)
		{
			Cells.Init(0, CellCount);
		}
		for (uint8& Value : Cells)
		{
			if (Value == VisionVisible)
			{
				Value = VisionExplored;
			}
		}
	}

	for (int32 Key : EntityKeys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || State->Health <= 0.f || State->SightRadiusUU <= 0)
		{
			continue;
		}
		TArray<uint8>* Cells = TeamVision.Find(State->TeamId);
		if (!Cells)
		{
			continue;
		}
		const int32 OriginX = KodBuildGrid::CellOf(State->Position.X);
		const int32 OriginY = KodBuildGrid::CellOf(State->Position.Y);
		const int32 ViewerHeight = GetMapCellHeight(OriginX, OriginY);
		const int32 RadiusCells = FMath::Max(1, FMath::DivideAndRoundUp(State->SightRadiusUU, KodBuildGrid::CellUU));
		const int32 RadiusSq = RadiusCells * RadiusCells;
		for (int32 DY = -RadiusCells; DY <= RadiusCells; ++DY)
		{
			for (int32 DX = -RadiusCells; DX <= RadiusCells; ++DX)
			{
				if (DX * DX + DY * DY > RadiusSq)
				{
					continue;
				}
				const int32 CellX = OriginX + DX;
				const int32 CellY = OriginY + DY;
				if (GetMapCellHeight(CellX, CellY) > ViewerHeight)
				{
					continue;
				}
				const int32 Index = VisionCellIndex(CellX, CellY);
				if (Cells->IsValidIndex(Index))
				{
					(*Cells)[Index] = VisionVisible;
				}
			}
		}
	}

	TArray<int32> StructureKeys;
	Structures.GetKeys(StructureKeys);
	StructureKeys.Sort();
	for (int32 TeamId : Teams)
	{
		if (bAiCheatVision && TeamId == 1)
		{
			continue;
		}
		TArray<FKodSeenStructure>& Seen = SeenStructures.FindOrAdd(TeamId);
		for (int32 Key : StructureKeys)
		{
			const FKodStructureState* Structure = Structures.Find(Key);
			const FKodSimEntityState* Combat = States.Find(Key);
			if (!Structure || Structure->TeamId == TeamId || !Combat || Combat->Health <= 0.f)
			{
				continue;
			}
			if (!CanTeamSeeWorld(TeamId, Structure->Position.X, Structure->Position.Y))
			{
				continue;
			}
			FKodSeenStructure* Slot = nullptr;
			for (FKodSeenStructure& Entry : Seen)
			{
				if (Entry.Id == Structure->Id.Value)
				{
					Slot = &Entry;
					break;
				}
			}
			if (!Slot)
			{
				Slot = &Seen.AddDefaulted_GetRef();
				Slot->Id = Structure->Id.Value;
			}
			Slot->TeamId = Structure->TeamId;
			Slot->DefinitionId = Structure->DefinitionId;
			Slot->PosX = FMath::RoundToInt(Structure->Position.X);
			Slot->PosY = FMath::RoundToInt(Structure->Position.Y);
			Slot->FootprintX = Structure->FootprintX;
			Slot->FootprintY = Structure->FootprintY;
			Slot->Health = FMath::RoundToInt(Combat->Health);
			Slot->MaxHealth = FMath::Max(1, FMath::RoundToInt(Combat->MaxHealth));
		}

		for (int32 Index = Seen.Num() - 1; Index >= 0; --Index)
		{
			const FKodSeenStructure& Entry = Seen[Index];
			if (GetTeamVision(TeamId, static_cast<float>(Entry.PosX), static_cast<float>(Entry.PosY)) != VisionVisible)
			{
				continue;
			}
			const FKodSimEntityState* Combat = States.Find(Entry.Id);
			if (!Combat || Combat->Health <= 0.f)
			{
				Seen.RemoveAt(Index);
			}
		}
		Seen.Sort([](const FKodSeenStructure& A, const FKodSeenStructure& B)
		{
			return A.Id < B.Id;
		});

		TArray<FKodSeenUnit>& SeenMobile = SeenUnits.FindOrAdd(TeamId);
		for (int32 Key : EntityKeys)
		{
			const FKodSimEntityState* State = States.Find(Key);
			if (!State || !State->bMobile || State->TeamId == TeamId || State->Health <= 0.f)
			{
				continue;
			}
			if (!CanTeamSeeWorld(TeamId, State->Position.X, State->Position.Y))
			{
				continue;
			}
			FKodSeenUnit* Slot = nullptr;
			for (FKodSeenUnit& Entry : SeenMobile)
			{
				if (Entry.Id == State->Id.Value)
				{
					Slot = &Entry;
					break;
				}
			}
			if (!Slot)
			{
				Slot = &SeenMobile.AddDefaulted_GetRef();
				Slot->Id = State->Id.Value;
			}
			Slot->TeamId = State->TeamId;
			Slot->PosX = FMath::RoundToInt(State->Position.X);
			Slot->PosY = FMath::RoundToInt(State->Position.Y);
		}
		for (int32 Index = SeenMobile.Num() - 1; Index >= 0; --Index)
		{
			const FKodSeenUnit& Entry = SeenMobile[Index];
			if (GetTeamVision(TeamId, static_cast<float>(Entry.PosX), static_cast<float>(Entry.PosY)) != VisionVisible)
			{
				continue;
			}
			const FKodSimEntityState* Combat = States.Find(Entry.Id);
			if (!Combat || Combat->Health <= 0.f)
			{
				SeenMobile.RemoveAt(Index);
			}
		}
		SeenMobile.Sort([](const FKodSeenUnit& A, const FKodSeenUnit& B)
		{
			return A.Id < B.Id;
		});
	}
	ApplyExposedReveal();
	ApplyReconVision();
}
