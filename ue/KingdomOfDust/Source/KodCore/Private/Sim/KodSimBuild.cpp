#include "Sim/KodSimSubsystem.h"

#include "GameFramework/Actor.h"

namespace
{
	void LogPlace(const FString& Type, int32 CellX, int32 CellY, bool bValid)
	{
		UE_LOG(LogTemp, Log, TEXT("KodBuild Place Type=%s Cell=%d,%d Valid=%d"),
			*Type,
			CellX,
			CellY,
			bValid ? 1 : 0);
	}
}

void UKodSimSubsystem::RegisterBuildSpec(const FKodBuildSpec& Spec)
{
	if (Spec.TypeId.IsNone())
	{
		return;
	}
	BuildSpecs.Add(Spec.TypeId, Spec);
}

void UKodSimSubsystem::SetPlayableWorldBox(const FBox& WorldBox)
{
	if (!WorldBox.IsValid)
	{
		bHasPlayable = false;
		return;
	}
	PlayMinX = KodBuildGrid::CellOf(WorldBox.Min.X);
	PlayMinY = KodBuildGrid::CellOf(WorldBox.Min.Y);
	PlayMaxX = KodBuildGrid::CellOf(WorldBox.Max.X);
	PlayMaxY = KodBuildGrid::CellOf(WorldBox.Max.Y);
	bHasPlayable = PlayMinX <= PlayMaxX && PlayMinY <= PlayMaxY;
}

bool UKodSimSubsystem::TryGetPlayableCellBounds(int32& OutMinX, int32& OutMinY, int32& OutMaxX, int32& OutMaxY) const
{
	if (!bHasPlayable)
	{
		return false;
	}
	OutMinX = PlayMinX;
	OutMinY = PlayMinY;
	OutMaxX = PlayMaxX;
	OutMaxY = PlayMaxY;
	return true;
}

bool UKodSimSubsystem::IsCellBlocked(int32 CellX, int32 CellY) const
{
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
			return true;
		}
	}
	return false;
}

bool UKodSimSubsystem::IsWorldCellBlocked(const FVector& World) const
{
	return IsCellBlocked(KodBuildGrid::CellOf(World.X), KodBuildGrid::CellOf(World.Y));
}

FKodEntityId UKodSimSubsystem::FindCompletedRig(int32 TeamId, FKodEntityId VentId) const
{
	FKodEntityId Found;
	if (!VentId.IsValid())
	{
		return Found;
	}
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || !Structure->bExtractor || !Structure->bComplete || Structure->TeamId != TeamId)
		{
			continue;
		}
		if (Structure->VentId.Value == VentId.Value)
		{
			Found = Structure->Id;
			return Found;
		}
	}
	return Found;
}

bool UKodSimSubsystem::TryGetGatherApproach(int32 TeamId, const FKodResourceNodeState& Node, FVector& OutPoint) const
{
	if (Node.Type != EKodResourceType::Luminene)
	{
		OutPoint = Node.Position;
		return true;
	}
	const FKodEntityId RigId = FindCompletedRig(TeamId, Node.Id);
	const FKodStructureState* Rig = RigId.IsValid() ? Structures.Find(RigId.Value) : nullptr;
	if (!Rig)
	{
		return false;
	}
	OutPoint = Rig->StandPoint.IsNearlyZero() ? Rig->Position : Rig->StandPoint;
	return true;
}

FVector UKodSimSubsystem::SteerAroundBuildings(const FKodSimEntityState& State, const FVector& DesiredDelta) const
{
	if (DesiredDelta.SizeSquared2D() <= KINDA_SMALL_NUMBER || !State.bMobile)
	{
		return DesiredDelta;
	}
	const FVector Proposed = State.Position + DesiredDelta;
	if (!IsWorldCellBlocked(State.Position) && !IsWorldCellBlocked(Proposed))
	{
		return DesiredDelta;
	}

	const float Step = DesiredDelta.Size2D();
	if (IsWorldCellBlocked(State.Position))
	{
		const int32 CellX = KodBuildGrid::CellOf(State.Position.X);
		const int32 CellY = KodBuildGrid::CellOf(State.Position.Y);
		const int32 Offsets[8][2] = {
			{1, 0}, {-1, 0}, {0, 1}, {0, -1},
			{1, 1}, {1, -1}, {-1, 1}, {-1, -1}
		};
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const int32 NextX = CellX + Offsets[Index][0];
			const int32 NextY = CellY + Offsets[Index][1];
			if (IsCellBlocked(NextX, NextY))
			{
				continue;
			}
			const FVector Aim(
				KodBuildGrid::CellCenter(NextX),
				KodBuildGrid::CellCenter(NextY),
				State.Position.Z);
			FVector Dir = Aim - State.Position;
			Dir.Z = 0.f;
			if (!Dir.Normalize())
			{
				continue;
			}
			return Dir * Step;
		}
		const FKodStructureState* Occupant = nullptr;
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
				Occupant = Structure;
				break;
			}
		}
		FVector Away(1.f, 0.f, 0.f);
		if (Occupant)
		{
			Away = State.Position - Occupant->Position;
			Away.Z = 0.f;
			if (!Away.Normalize())
			{
				Away = FVector(1.f, 0.f, 0.f);
			}
		}
		return Away * Step;
	}

	FVector Dir = DesiredDelta;
	Dir.Z = 0.f;
	if (!Dir.Normalize())
	{
		return FVector::ZeroVector;
	}
	const FVector Right(-Dir.Y, Dir.X, 0.f);
	if (!IsWorldCellBlocked(State.Position + Right * Step))
	{
		return Right * Step;
	}
	if (!IsWorldCellBlocked(State.Position - Right * Step))
	{
		return -Right * Step;
	}
	return FVector::ZeroVector;
}

void UKodSimSubsystem::ClearBuildAssignment(FKodSimEntityState& State)
{
	if (!State.BuildSite.IsValid())
	{
		return;
	}
	if (FKodStructureState* Structure = Structures.Find(State.BuildSite.Value))
	{
		if (Structure->BuilderId.Value == State.Id.Value)
		{
			Structure->BuilderId = FKodEntityId();
		}
		Structure->bActivelyBuilding = false;
	}
	State.BuildSite = FKodEntityId();
}

namespace
{
	void FootprintOrigin(int32 CursorX, int32 CursorY, int32 SizeX, int32 SizeY, int32& OutX, int32& OutY)
	{
		OutX = CursorX - (FMath::Max(1, SizeX) - 1) / 2;
		OutY = CursorY - (FMath::Max(1, SizeY) - 1) / 2;
	}

	FVector FootprintCenter(int32 OriginX, int32 OriginY, int32 SizeX, int32 SizeY, float Z)
	{
		const float MinX = static_cast<float>(OriginX) * static_cast<float>(KodBuildGrid::CellUU);
		const float MinY = static_cast<float>(OriginY) * static_cast<float>(KodBuildGrid::CellUU);
		const float SpanX = static_cast<float>(FMath::Max(1, SizeX)) * static_cast<float>(KodBuildGrid::CellUU);
		const float SpanY = static_cast<float>(FMath::Max(1, SizeY)) * static_cast<float>(KodBuildGrid::CellUU);
		return FVector(MinX + SpanX * 0.5f, MinY + SpanY * 0.5f, Z);
	}

	bool FootprintInside(int32 OriginX, int32 OriginY, int32 SizeX, int32 SizeY, int32 MinX, int32 MinY, int32 MaxX, int32 MaxY)
	{
		return OriginX >= MinX
			&& OriginY >= MinY
			&& OriginX + SizeX - 1 <= MaxX
			&& OriginY + SizeY - 1 <= MaxY;
	}
}

FKodPlacementResult UKodSimSubsystem::QueryPlacement(int32 TeamId, FName TypeId, FVector CursorWorld) const
{
	FKodPlacementResult Result;
	const FKodBuildSpec* Spec = BuildSpecs.Find(TypeId);
	const int32 CursorX = KodBuildGrid::CellOf(CursorWorld.X);
	const int32 CursorY = KodBuildGrid::CellOf(CursorWorld.Y);
	if (!Spec)
	{
		Result.Reason = TEXT("Unknown");
		Result.CellX = CursorX;
		Result.CellY = CursorY;
		return Result;
	}

	const int32 SizeX = FMath::Max(1, Spec->FootprintX);
	const int32 SizeY = FMath::Max(1, Spec->FootprintY);
	int32 OriginX = 0;
	int32 OriginY = 0;
	FootprintOrigin(CursorX, CursorY, SizeX, SizeY, OriginX, OriginY);
	Result.CellX = OriginX;
	Result.CellY = OriginY;
	Result.WorldCenter = FootprintCenter(OriginX, OriginY, SizeX, SizeY, CursorWorld.Z);

	if (bHasPlayable && !FootprintInside(OriginX, OriginY, SizeX, SizeY, PlayMinX, PlayMinY, PlayMaxX, PlayMaxY))
	{
		Result.Reason = TEXT("OffMap");
		return Result;
	}

	for (int32 X = OriginX; X < OriginX + SizeX; ++X)
	{
		for (int32 Y = OriginY; Y < OriginY + SizeY; ++Y)
		{
			if (IsCellBlocked(X, Y))
			{
				Result.Reason = TEXT("Blocked");
				return Result;
			}
		}
	}

	TArray<int32> OverlapNodes;
	ResourceNodes.GetKeys(OverlapNodes);
	OverlapNodes.Sort();
	for (int32 Key : OverlapNodes)
	{
		const FKodResourceNodeState* Node = ResourceNodes.Find(Key);
		if (!Node)
		{
			continue;
		}
		const int32 NodeX = KodBuildGrid::CellOf(Node->Position.X);
		const int32 NodeY = KodBuildGrid::CellOf(Node->Position.Y);
		if (NodeX < OriginX || NodeX >= OriginX + SizeX || NodeY < OriginY || NodeY >= OriginY + SizeY)
		{
			continue;
		}
		// A Rig is allowed to cover the Luminene vent it snaps to. Any other node blocks.
		if (Spec->bExtractor && Node->Type == EKodResourceType::Luminene)
		{
			continue;
		}
		Result.Reason = TEXT("Node");
		return Result;
	}

	if (Spec->bExtractor)
	{
		FKodEntityId Vent;
		int64 BestDist = MAX_int64;
		bool bFound = false;
		TArray<int32> NodeKeys;
		ResourceNodes.GetKeys(NodeKeys);
		NodeKeys.Sort();
		for (int32 Key : NodeKeys)
		{
			const FKodResourceNodeState* Node = ResourceNodes.Find(Key);
			if (!Node || Node->Type != EKodResourceType::Luminene)
			{
				continue;
			}
			const int32 NodeX = KodBuildGrid::CellOf(Node->Position.X);
			const int32 NodeY = KodBuildGrid::CellOf(Node->Position.Y);
			if (NodeX < OriginX || NodeX >= OriginX + SizeX || NodeY < OriginY || NodeY >= OriginY + SizeY)
			{
				continue;
			}
			const int64 DX = static_cast<int64>(NodeX) - CursorX;
			const int64 DY = static_cast<int64>(NodeY) - CursorY;
			const int64 Dist = DX * DX + DY * DY;
			if (!bFound || Dist < BestDist || (Dist == BestDist && Key < Vent.Value))
			{
				bFound = true;
				BestDist = Dist;
				Vent.Value = Key;
			}
		}
		if (!Vent.IsValid())
		{
			Result.Reason = TEXT("NeedVent");
			return Result;
		}
		if (FindCompletedRig(TeamId, Vent).IsValid())
		{
			Result.Reason = TEXT("VentUsed");
			Result.VentId = Vent;
			return Result;
		}
		TArray<int32> StructureKeys;
		Structures.GetKeys(StructureKeys);
		for (int32 Key : StructureKeys)
		{
			const FKodStructureState* Structure = Structures.Find(Key);
			if (Structure && Structure->bExtractor && Structure->VentId.Value == Vent.Value)
			{
				Result.Reason = TEXT("VentUsed");
				Result.VentId = Vent;
				return Result;
			}
		}
		Result.VentId = Vent;
	}

	FKodResourceCost Price;
	Price.Jadeite = FMath::Max(0, Spec->Cost.Credits);
	Price.Luminene = FMath::Max(0, Spec->Cost.Luminene);
	if (!CanAfford(TeamId, Price))
	{
		Result.Reason = TEXT("Afford");
		return Result;
	}

	Result.bValid = true;
	Result.Reason = TEXT("Ok");
	return Result;
}

bool UKodSimSubsystem::TryPlaceStructure(int32 TeamId, FName TypeId, FVector CursorWorld, FKodEntityId WorkerId)
{
	const FKodPlacementResult Placement = QueryPlacement(TeamId, TypeId, CursorWorld);
	const FString TypeText = TypeId.ToString();
	if (!Placement.bValid)
	{
		LogPlace(TypeText, Placement.CellX, Placement.CellY, false);
		UE_LOG(LogTemp, Log, TEXT("KodBuild Invalid Reason=%s"), *Placement.Reason.ToString());
		return false;
	}

	FKodSimEntityState* Worker = States.Find(WorkerId.Value);
	if (!Worker || !Worker->bCanGather || Worker->Health <= 0.f || Worker->TeamId != TeamId)
	{
		LogPlace(TypeText, Placement.CellX, Placement.CellY, false);
		UE_LOG(LogTemp, Log, TEXT("KodBuild Invalid Reason=NoWorker"));
		return false;
	}

	const FKodBuildSpec* Spec = BuildSpecs.Find(TypeId);
	if (!Spec)
	{
		return false;
	}
	FKodResourceCost Price;
	Price.Jadeite = FMath::Max(0, Spec->Cost.Credits);
	Price.Luminene = FMath::Max(0, Spec->Cost.Luminene);
	if (!Spend(TeamId, Price))
	{
		LogPlace(TypeText, Placement.CellX, Placement.CellY, false);
		UE_LOG(LogTemp, Log, TEXT("KodBuild Invalid Reason=Afford"));
		return false;
	}

	const int32 SizeX = FMath::Max(1, Spec->FootprintX);
	const int32 SizeY = FMath::Max(1, Spec->FootprintY);
	FKodEntityId Id;
	Id.Value = NextId++;

	FVector Stand = Placement.WorldCenter;
	int64 BestDist = MAX_int64;
	bool bStand = false;
	for (int32 X = Placement.CellX - 1; X <= Placement.CellX + SizeX; ++X)
	{
		for (int32 Y = Placement.CellY - 1; Y <= Placement.CellY + SizeY; ++Y)
		{
			const bool bInside = X >= Placement.CellX && X < Placement.CellX + SizeX
				&& Y >= Placement.CellY && Y < Placement.CellY + SizeY;
			if (bInside || IsCellBlocked(X, Y))
			{
				continue;
			}
			const float WX = KodBuildGrid::CellCenter(X);
			const float WY = KodBuildGrid::CellCenter(Y);
			const int64 DX = FMath::RoundToInt(WX - Worker->Position.X);
			const int64 DY = FMath::RoundToInt(WY - Worker->Position.Y);
			const int64 Dist = DX * DX + DY * DY;
			if (!bStand || Dist < BestDist || (Dist == BestDist && (Y < KodBuildGrid::CellOf(Stand.Y) || (Y == KodBuildGrid::CellOf(Stand.Y) && X < KodBuildGrid::CellOf(Stand.X)))))
			{
				bStand = true;
				BestDist = Dist;
				Stand = FVector(WX, WY, Worker->Position.Z);
			}
		}
	}

	FKodStructureState& Structure = Structures.Add(Id.Value);
	Structure.Id = Id;
	Structure.TeamId = TeamId;
	Structure.DefinitionId = Spec->TypeId;
	Structure.DisplayId = Spec->DisplayId.IsNone() ? Spec->TypeId : Spec->DisplayId;
	Structure.Position = Placement.WorldCenter;
	QuantizeResourcePosition(Structure.Position);
	Structure.ExitPoint = Stand;
	QuantizeResourcePosition(Structure.ExitPoint);
	Structure.SupplyProvided = FMath::Max(0, Spec->SupplyProvided);
	Structure.bIsDropOff = Spec->bIsDropOff;
	Structure.CellX = Placement.CellX;
	Structure.CellY = Placement.CellY;
	Structure.FootprintX = SizeX;
	Structure.FootprintY = SizeY;
	Structure.BuildTimeTotal = FMath::Max(1, Spec->Cost.BuildTime);
	Structure.BuildTicksDone = 0;
	Structure.bComplete = false;
	Structure.bExtractor = Spec->bExtractor;
	Structure.VentId = Placement.VentId;
	Structure.PaidCredits = Price.Jadeite;
	Structure.PaidLuminene = Price.Luminene;
	Structure.MaxHealth = FMath::Max(1.f, Spec->MaxHealth);
	Structure.Trains = Spec->Trains;
	Structure.StandPoint = Stand;
	Structure.RallyPoint = Stand;
	Structure.BuilderId = WorkerId;

	IssueMove(WorkerId, Stand);
	if (FKodSimEntityState* Ordered = States.Find(WorkerId.Value))
	{
		Ordered->BuildSite = Id;
	}

	FKodPendingBuildingSpawn Spawn;
	Spawn.StructureId = Id.Value;
	Spawn.TypeId = Spec->TypeId;
	Spawn.TeamId = TeamId;
	Spawn.Location = Structure.Position;
	Spawn.FootprintX = SizeX;
	Spawn.FootprintY = SizeY;
	PendingBuildingSpawns.Add(Spawn);

	LogPlace(TypeText, Placement.CellX, Placement.CellY, true);
	return true;
}

bool UKodSimSubsystem::ResumeConstruction(FKodEntityId StructureId, FKodEntityId WorkerId)
{
	FKodStructureState* Structure = Structures.Find(StructureId.Value);
	FKodSimEntityState* Worker = States.Find(WorkerId.Value);
	if (!Structure || Structure->bComplete || !Worker || !Worker->bCanGather || Worker->TeamId != Structure->TeamId)
	{
		return false;
	}
	if (Structure->BuilderId.IsValid() && Structure->BuilderId.Value != WorkerId.Value)
	{
		if (FKodSimEntityState* Previous = States.Find(Structure->BuilderId.Value))
		{
			if (Previous->BuildSite.Value == StructureId.Value)
			{
				Previous->BuildSite = FKodEntityId();
			}
		}
	}
	Structure->BuilderId = WorkerId;
	Structure->bActivelyBuilding = false;
	IssueMove(WorkerId, Structure->StandPoint);
	if (FKodSimEntityState* Ordered = States.Find(WorkerId.Value))
	{
		Ordered->BuildSite = StructureId;
	}
	return true;
}

bool UKodSimSubsystem::CancelConstruction(FKodEntityId StructureId)
{
	FKodStructureState* Structure = Structures.Find(StructureId.Value);
	if (!Structure || Structure->bComplete)
	{
		return false;
	}
	const int32 RefundCredits = (FMath::Max(0, Structure->PaidCredits) * 75) / 100;
	const int32 RefundLuminene = (FMath::Max(0, Structure->PaidLuminene) * 75) / 100;
	const int32 TeamId = Structure->TeamId;
	const FKodEntityId BuilderId = Structure->BuilderId;
	if (FKodSimEntityState* Worker = States.Find(BuilderId.Value))
	{
		if (Worker->BuildSite.Value == StructureId.Value)
		{
			Worker->BuildSite = FKodEntityId();
			Worker->Order = EKodSimOrderType::None;
		}
	}
	Structures.Remove(StructureId.Value);
	if (RefundCredits > 0 || RefundLuminene > 0)
	{
		FKodResourceCost Refund;
		Refund.Jadeite = RefundCredits;
		Refund.Luminene = RefundLuminene;
		Deposit(TeamId, Refund);
	}
	UE_LOG(LogTemp, Log, TEXT("KodBuild Cancel Refund=C%d,L%d"), RefundCredits, RefundLuminene);
	return true;
}

bool UKodSimSubsystem::TryGetPrimaryTrain(FKodEntityId StructureId, FName& OutUnitId) const
{
	const FKodStructureState* Structure = Structures.Find(StructureId.Value);
	if (!Structure || !Structure->bComplete || Structure->Trains.Num() <= 0)
	{
		return false;
	}
	OutUnitId = Structure->Trains[0];
	return !OutUnitId.IsNone();
}

void UKodSimSubsystem::BindStructureActor(FKodEntityId Id, AActor* Actor)
{
	FKodStructureState* Structure = Structures.Find(Id.Value);
	if (!Actor || !Structure)
	{
		return;
	}
	Entities.Add(Id.Value, Actor);
	if (!States.Contains(Id.Value))
	{
		FKodSimEntityState State;
		State.Id = Id;
		State.DefinitionId = Structure->DefinitionId;
		State.Position = Structure->Position;
		State.YawDegrees = Actor->GetActorRotation().Yaw;
		State.MaxHealth = FMath::Max(1.f, Structure->MaxHealth);
		const float Alpha = Structure->bComplete || Structure->BuildTimeTotal <= 0
			? 1.f
			: 0.1f + 0.9f * (static_cast<float>(Structure->BuildTicksDone) / static_cast<float>(Structure->BuildTimeTotal));
		State.Health = State.MaxHealth * FMath::Clamp(Alpha, 0.1f, 1.f);
		State.bMobile = false;
		State.TeamId = Structure->TeamId;
		QuantizePose(State);
		States.Add(Id.Value, State);
		PrevPositions.Add(Id.Value, State.Position);
		PrevYaws.Add(Id.Value, State.YawDegrees);
	}
	if (FKodSimEntityState* State = States.Find(Id.Value))
	{
		State->MaxHealth = FMath::Max(1.f, Structure->MaxHealth);
		State->bMobile = false;
		State->TeamId = Structure->TeamId;
		if (!Structure->bComplete && Structure->BuildTimeTotal > 0)
		{
			const float Alpha = 0.1f + 0.9f * (static_cast<float>(Structure->BuildTicksDone) / static_cast<float>(Structure->BuildTimeTotal));
			State->Health = State->MaxHealth * FMath::Clamp(Alpha, 0.1f, 1.f);
		}
	}
}

void UKodSimSubsystem::ConsumePendingBuildingSpawns(TArray<FKodPendingBuildingSpawn>& OutSpawns)
{
	OutSpawns = MoveTemp(PendingBuildingSpawns);
	PendingBuildingSpawns.Reset();
}

void UKodSimSubsystem::ReleaseStructureRecord(FKodEntityId Id)
{
	const FKodStructureState* Structure = Structures.Find(Id.Value);
	if (!Structure)
	{
		return;
	}
	const int32 TeamId = Structure->TeamId;
	const bool bWasComplete = Structure->bComplete;
	const FKodEntityId BuilderId = Structure->BuilderId;
	if (FKodSimEntityState* Worker = States.Find(BuilderId.Value))
	{
		if (Worker->BuildSite.Value == Id.Value)
		{
			Worker->BuildSite = FKodEntityId();
		}
	}
	Structures.Remove(Id.Value);
	DropOffs.Remove(Id.Value);
	if (bWasComplete)
	{
		RecalcSupplyCap(TeamId);
	}
}

void UKodSimSubsystem::StepConstruction()
{
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->bComplete || Structure->BuildTimeTotal <= 0)
		{
			continue;
		}

		FKodSimEntityState* Builder = Structure->BuilderId.IsValid() ? States.Find(Structure->BuilderId.Value) : nullptr;
		const bool bPresent = Builder && Builder->Health > 0.f && Builder->BuildSite.Value == Structure->Id.Value;
		const float Dist = bPresent ? FVector::Dist2D(Builder->Position, Structure->StandPoint) : MAX_flt;
		const bool bAtSite = bPresent && Dist <= 180.f;
		if (!bAtSite)
		{
			Structure->bActivelyBuilding = false;
			continue;
		}

		if (!Structure->bActivelyBuilding)
		{
			Structure->bActivelyBuilding = true;
			UE_LOG(LogTemp, Log, TEXT("KodBuild Start Id=%d Type=%s"),
				Structure->Id.Value,
				*Structure->DisplayId.ToString());
		}

		if (FKodSimEntityState* Combat = States.Find(Structure->Id.Value))
		{
			const float ProgressHP = Combat->MaxHealth * (0.1f + 0.9f * (static_cast<float>(Structure->BuildTicksDone) / static_cast<float>(Structure->BuildTimeTotal)));
			if (Combat->Health + 1.f < ProgressHP)
			{
				const float Alpha = FMath::Clamp((Combat->Health / FMath::Max(1.f, Combat->MaxHealth) - 0.1f) / 0.9f, 0.f, 1.f);
				Structure->BuildTicksDone = FMath::Clamp(FMath::RoundToInt(Alpha * static_cast<float>(Structure->BuildTimeTotal)), 0, Structure->BuildTimeTotal);
			}
		}

		Structure->BuildTicksDone = FMath::Min(Structure->BuildTimeTotal, Structure->BuildTicksDone + 1);
		const int32 Bucket = (Structure->BuildTicksDone * 4) / Structure->BuildTimeTotal;
		if (Bucket != Structure->LastProgressBucket)
		{
			Structure->LastProgressBucket = Bucket;
			UE_LOG(LogTemp, Log, TEXT("KodBuild Progress Id=%d Ticks=%d/%d"),
				Structure->Id.Value,
				Structure->BuildTicksDone,
				Structure->BuildTimeTotal);
		}

		const float Alpha = 0.1f + 0.9f * (static_cast<float>(Structure->BuildTicksDone) / static_cast<float>(Structure->BuildTimeTotal));
		if (FKodSimEntityState* Combat = States.Find(Structure->Id.Value))
		{
			Combat->MaxHealth = FMath::Max(Combat->MaxHealth, Structure->MaxHealth);
			Combat->Health = FMath::Max(Combat->Health, Combat->MaxHealth * FMath::Clamp(Alpha, 0.1f, 1.f));
		}

		if (Structure->BuildTicksDone >= Structure->BuildTimeTotal)
		{
			Structure->bComplete = true;
			Structure->bActivelyBuilding = false;
			if (Builder)
			{
				Builder->BuildSite = FKodEntityId();
				Builder->Order = EKodSimOrderType::None;
			}
			Structure->BuilderId = FKodEntityId();
			if (FKodSimEntityState* Combat = States.Find(Structure->Id.Value))
			{
				Combat->Health = Combat->MaxHealth;
			}
			RecalcSupplyCap(Structure->TeamId);
			UE_LOG(LogTemp, Log, TEXT("KodBuild Complete Id=%d Type=%s"),
				Structure->Id.Value,
				*Structure->DisplayId.ToString());
		}
	}
}
