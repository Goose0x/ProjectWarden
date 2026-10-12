#include "Sim/KodSimSubsystem.h"

namespace
{
	int64 PackVisionCell(int32 X, int32 Y)
	{
		return (static_cast<int64>(static_cast<uint32>(X)) << 32) | static_cast<uint32>(Y);
	}

	constexpr uint8 VisionExplored = 1;
	constexpr uint8 VisionVisible = 2;
}

uint8 UKodSimSubsystem::GetTeamVision(int32 TeamId, float WorldX, float WorldY) const
{
	const TMap<int64, uint8>* Grid = TeamVision.Find(TeamId);
	if (!Grid)
	{
		return 0;
	}
	const int64 Key = PackVisionCell(KodBuildGrid::CellOf(WorldX), KodBuildGrid::CellOf(WorldY));
	if (const uint8* Found = Grid->Find(Key))
	{
		return *Found;
	}
	return 0;
}

bool UKodSimSubsystem::CanTeamSeeWorld(int32 TeamId, float WorldX, float WorldY) const
{
	if (bAiCheatVision && TeamId == 1)
	{
		return true;
	}
	return GetTeamVision(TeamId, WorldX, WorldY) == VisionVisible;
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
	TArray<int32> EntityKeys;
	States.GetKeys(EntityKeys);
	EntityKeys.Sort();

	TMap<int32, TSet<int64>> VisibleNow;
	for (int32 Key : EntityKeys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || State->Health <= 0.f || State->SightRadiusUU <= 0)
		{
			continue;
		}
		const int32 OriginX = KodBuildGrid::CellOf(State->Position.X);
		const int32 OriginY = KodBuildGrid::CellOf(State->Position.Y);
		const int32 ViewerHeight = GetMapCellHeight(OriginX, OriginY);
		const int32 RadiusCells = FMath::Max(1, FMath::DivideAndRoundUp(State->SightRadiusUU, KodBuildGrid::CellUU));
		const int32 RadiusSq = RadiusCells * RadiusCells;
		TSet<int64>& Visible = VisibleNow.FindOrAdd(State->TeamId);
		for (int32 DX = -RadiusCells; DX <= RadiusCells; ++DX)
		{
			for (int32 DY = -RadiusCells; DY <= RadiusCells; ++DY)
			{
				if (DX * DX + DY * DY > RadiusSq)
				{
					continue;
				}
				const int32 CellX = OriginX + DX;
				const int32 CellY = OriginY + DY;
				const int32 CellHeight = GetMapCellHeight(CellX, CellY);
				if (CellHeight > ViewerHeight)
				{
					continue;
				}
				Visible.Add(PackVisionCell(CellX, CellY));
			}
		}
	}

	TArray<int32> Teams;
	for (const TPair<int32, TMap<int64, uint8>>& Pair : TeamVision)
	{
		Teams.Add(Pair.Key);
	}
	for (const TPair<int32, TSet<int64>>& Pair : VisibleNow)
	{
		Teams.AddUnique(Pair.Key);
	}
	Teams.Sort();

	for (int32 TeamId : Teams)
	{
		TMap<int64, uint8>& Grid = TeamVision.FindOrAdd(TeamId);
		const TSet<int64>* Visible = VisibleNow.Find(TeamId);
		TArray<int64> Known;
		Grid.GetKeys(Known);
		Known.Sort();
		for (int64 Cell : Known)
		{
			const bool bSeen = Visible && Visible->Contains(Cell);
			uint8& Value = Grid.FindChecked(Cell);
			if (bSeen)
			{
				Value = VisionVisible;
			}
			else if (Value == VisionVisible)
			{
				Value = VisionExplored;
			}
		}
		if (Visible)
		{
			for (int64 Cell : *Visible)
			{
				Grid.FindOrAdd(Cell) = VisionVisible;
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
	}
}
