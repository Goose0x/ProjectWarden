#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Game/KodLanding.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodBuildTypes.h"
#include "Sim/KodDepotClips.h"
#include "Sim/KodProductionTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Warden/KodWardenPaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodFootprintParityTest,
	"Kod.Build.FootprintParity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	struct FFootCase
	{
		const TCHAR* Name;
		FName Id;
		int32 Cells;
	};

	AActor* SpawnRooted(UWorld* World, const FVector& At)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		if (!Actor)
		{
			return nullptr;
		}
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(At);
		return Actor;
	}

	bool CellsMatch(const FKodPlacementResult& Placement, int32 Cells)
	{
		if (Placement.CellOk.Num() != Cells * Cells)
		{
			return false;
		}
		for (int32 Y = 0; Y < Cells; ++Y)
		{
			for (int32 X = 0; X < Cells; ++X)
			{
				const int32 CellX = Placement.CellX + X;
				const int32 CellY = Placement.CellY + Y;
				if (!KodBuildGrid::CoversCell(CellX, Placement.CellX, Cells)
					|| !KodBuildGrid::CoversCell(CellY, Placement.CellY, Cells))
				{
					return false;
				}
				const float PivotX = Placement.WorldCenter.X;
				const float Half = static_cast<float>(Cells) * static_cast<float>(KodBuildGrid::CellUU) * 0.5f;
				const float CellMin = static_cast<float>(CellX * KodBuildGrid::CellUU);
				const float CellMax = CellMin + static_cast<float>(KodBuildGrid::CellUU);
				if (CellMax <= PivotX - Half + 0.1f || CellMin >= PivotX + Half - 0.1f)
				{
					return false;
				}
			}
		}
		const int32 Outside = Placement.CellX + Cells;
		return !KodBuildGrid::CoversCell(Outside, Placement.CellX, Cells)
			&& !KodBuildGrid::CoversCell(Placement.CellX - 1, Placement.CellX, Cells);
	}
}

bool FKodFootprintParityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FFootCase Cases[] = {
		{TEXT("command center"), FName(KodWardenPaths::Id_CommandCenter), 5},
		{TEXT("barracks"), FName(KodWardenPaths::Id_Barracks), 3},
		{TEXT("bunker"), FName(KodWardenPaths::Id_Bunker), 3},
		{TEXT("supply depot"), FName(KodWardenPaths::Id_SupplyDepot), 2}
	};

	const FString PackageName = FString::Printf(TEXT("/Temp/KodFootParity_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodFootParity")), Package, true);
	TestNotNull(TEXT("world"), World);
	if (!World)
	{
		return false;
	}
	FURL Url;
	World->InitializeActorsForPlay(Url);
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	TestNotNull(TEXT("sim"), Sim);
	if (!Sim)
	{
		World->DestroyWorld(false);
		return false;
	}
	Sim->SetPlayableWorldBox(FBox(FVector(-8000.f, -8000.f, -500.f), FVector(40000.f, 40000.f, 2000.f)));

	int32 Slot = 0;
	for (const FFootCase& Case : Cases)
	{
		const int32 Anchor = 80 + Slot * 20;
		++Slot;
		FKodBuildSpec Spec;
		Spec.TypeId = Case.Id;
		Spec.FootprintX = Case.Cells;
		Spec.FootprintY = Case.Cells;
		Spec.MaxHealth = 400.f;
		Spec.bCanLiftoff = true;
		Spec.LiftSpeedUU = 1000000;
		if (Case.Id == FName(KodWardenPaths::Id_SupplyDepot))
		{
			Spec.bCanLower = true;
		}
		Sim->RegisterBuildSpec(Spec);

		const float Cursor = KodBuildGrid::PivotForOrigin(Anchor, Case.Cells) + 40.f;
		const FKodPlacementResult Placed = Sim->QueryPlacement(0, Case.Id, FVector(Cursor, Cursor, 12.f));
		TestTrue(*FString::Printf(TEXT("%s places"), Case.Name), Placed.bValid);
		const float Pivot = KodBuildGrid::SnapPivot(Cursor, Case.Cells);
		TestTrue(*FString::Printf(TEXT("%s pivot x"), Case.Name), FMath::IsNearlyEqual(Placed.WorldCenter.X, Pivot, 0.1f));
		TestTrue(*FString::Printf(TEXT("%s pivot y"), Case.Name), FMath::IsNearlyEqual(Placed.WorldCenter.Y, Pivot, 0.1f));
		if (KodBuildGrid::IsEvenFootprint(Case.Cells))
		{
			TestTrue(*FString::Printf(TEXT("%s pivot is a grid line"), Case.Name),
				FMath::IsNearlyEqual(FMath::Fmod(FMath::Abs(Placed.WorldCenter.X), static_cast<float>(KodBuildGrid::CellUU)), 0.f, 0.1f));
		}
		else
		{
			const float FromLine = FMath::Fmod(FMath::Abs(Placed.WorldCenter.X), static_cast<float>(KodBuildGrid::CellUU));
			TestTrue(*FString::Printf(TEXT("%s pivot is a cell centre"), Case.Name), FMath::IsNearlyEqual(FromLine, 100.f, 0.1f));
		}
		TestEqual(*FString::Printf(TEXT("%s origin"), Case.Name), Placed.CellX, KodBuildGrid::OriginCell(Pivot, Case.Cells));
		TestTrue(*FString::Printf(TEXT("%s cells under the box"), Case.Name), CellsMatch(Placed, Case.Cells));
		for (int32 Index = 0; Index < Case.Cells; ++Index)
		{
			const float Local = KodBuildGrid::CellLocalOffset(Index, Case.Cells);
			const float WorldCell = KodBuildGrid::CellCenter(Placed.CellX + Index);
			TestTrue(*FString::Printf(TEXT("%s ghost cell %d"), Case.Name, Index),
				FMath::IsNearlyEqual(Placed.WorldCenter.X + Local, WorldCell, 0.1f));
		}

		AActor* Actor = SpawnRooted(World, Placed.WorldCenter);
		FKodStructureSpec Row;
		Row.DefinitionId = Case.Id;
		Row.FootprintX = Case.Cells;
		Row.FootprintY = Case.Cells;
		Row.MaxHealth = 400.f;
		const FKodEntityId Id = Sim->RegisterStructure(Actor, Row);
		FKodStructureState State;
		TestTrue(*FString::Printf(TEXT("%s registered"), Case.Name), Sim->TryGetStructure(Id, State));
		TestEqual(*FString::Printf(TEXT("%s registered origin"), Case.Name), State.CellX, Placed.CellX);
		TestEqual(*FString::Printf(TEXT("%s registered origin y"), Case.Name), State.CellY, Placed.CellY);
		TestTrue(*FString::Printf(TEXT("%s blocks its cells"), Case.Name), Sim->IsCellBlocked(State.CellX, State.CellY, 0, true));
		TestFalse(*FString::Printf(TEXT("%s does not block the next cell"), Case.Name),
			Sim->IsCellBlocked(State.CellX + Case.Cells, State.CellY, 0, true));
	}

	FKodBuildSpec Barracks;
	Barracks.TypeId = FName(TEXT("ParityBarracks"));
	Barracks.FootprintX = 3;
	Barracks.FootprintY = 3;
	Barracks.MaxHealth = 400.f;
	Sim->RegisterBuildSpec(Barracks);
	FKodBuildSpec Depot;
	Depot.TypeId = FName(TEXT("ParityDepot"));
	Depot.FootprintX = 2;
	Depot.FootprintY = 2;
	Depot.bCanLower = true;
	Depot.MaxHealth = 400.f;
	Sim->RegisterBuildSpec(Depot);
	FKodBuildSpec Hall;
	Hall.TypeId = FName(TEXT("ParityHall"));
	Hall.FootprintX = 5;
	Hall.FootprintY = 5;
	Hall.bCanLiftoff = true;
	Hall.LiftSpeedUU = 1000000;
	Hall.MaxHealth = 400.f;
	Sim->RegisterBuildSpec(Hall);

	const FVector BarracksCursor(KodBuildGrid::PivotForOrigin(20, 3), KodBuildGrid::PivotForOrigin(20, 3), 0.f);
	const FKodPlacementResult BarracksPlace = Sim->QueryPlacement(0, Barracks.TypeId, BarracksCursor);
	TestEqual(TEXT("3x3 origin"), BarracksPlace.CellX, 20);
	AActor* BarracksActor = SpawnRooted(World, BarracksPlace.WorldCenter);
	FKodStructureSpec BarracksRow;
	BarracksRow.DefinitionId = Barracks.TypeId;
	BarracksRow.FootprintX = 3;
	BarracksRow.FootprintY = 3;
	BarracksRow.MaxHealth = 400.f;
	Sim->RegisterStructure(BarracksActor, BarracksRow);

	const FVector DepotCursor(KodBuildGrid::PivotForOrigin(23, 2), KodBuildGrid::PivotForOrigin(20, 2), 0.f);
	const FKodPlacementResult DepotBeside = Sim->QueryPlacement(0, Depot.TypeId, DepotCursor);
	TestTrue(TEXT("2x2 beside 3x3 is open"), DepotBeside.bValid);
	TestEqual(TEXT("2x2 starts on the next cell"), DepotBeside.CellX, 23);
	const FVector DepotOverlap(KodBuildGrid::PivotForOrigin(22, 2), KodBuildGrid::PivotForOrigin(20, 2), 0.f);
	TestFalse(TEXT("2x2 overlapping 3x3 is blocked"), Sim->QueryPlacement(0, Depot.TypeId, DepotOverlap).bValid);
	AActor* DepotActor = SpawnRooted(World, DepotBeside.WorldCenter);
	FKodStructureSpec DepotRow;
	DepotRow.DefinitionId = Depot.TypeId;
	DepotRow.FootprintX = 2;
	DepotRow.FootprintY = 2;
	DepotRow.MaxHealth = 400.f;
	const FKodEntityId DepotId = Sim->RegisterStructure(DepotActor, DepotRow);
	TestTrue(TEXT("depot cell blocks"), Sim->IsCellBlocked(23, 20, 0, true));
	TestTrue(TEXT("barracks cell blocks"), Sim->IsCellBlocked(22, 20, 0, true));

	const int32 OpenSteps = FMath::Max(FKodDepotClipTable::Get().WalkableTick(), 1);
	TestTrue(TEXT("lower the depot"), Sim->ToggleDepot(DepotId));
	Sim->AdvanceSimTicks(OpenSteps);
	TestFalse(TEXT("lowered depot is walkable"), Sim->IsCellBlocked(23, 20, 0, true));
	TestTrue(TEXT("adjacent barracks still walls"), Sim->IsCellBlocked(22, 20, 0, true));

	const FVector HallCursor(KodBuildGrid::PivotForOrigin(30, 5), KodBuildGrid::PivotForOrigin(40, 5), 0.f);
	const FKodPlacementResult HallPlace = Sim->QueryPlacement(0, Hall.TypeId, HallCursor);
	TestEqual(TEXT("5x5 origin"), HallPlace.CellX, 30);
	AActor* HallActor = SpawnRooted(World, HallPlace.WorldCenter);
	FKodStructureSpec HallRow;
	HallRow.DefinitionId = Hall.TypeId;
	HallRow.FootprintX = 5;
	HallRow.FootprintY = 5;
	HallRow.MaxHealth = 400.f;
	Sim->RegisterStructure(HallActor, HallRow);
	const FVector WingCursor(KodBuildGrid::PivotForOrigin(35, 3), KodBuildGrid::PivotForOrigin(40, 3), 0.f);
	const FKodPlacementResult Wing = Sim->QueryPlacement(0, Barracks.TypeId, WingCursor);
	TestTrue(TEXT("3x3 beside 5x5 is open"), Wing.bValid);
	TestEqual(TEXT("3x3 starts against the hall"), Wing.CellX, 35);
	const FVector WingOverlap(KodBuildGrid::PivotForOrigin(34, 3), KodBuildGrid::PivotForOrigin(40, 3), 0.f);
	TestFalse(TEXT("3x3 overlapping 5x5 is blocked"), Sim->QueryPlacement(0, Barracks.TypeId, WingOverlap).bValid);
	for (int32 Cell = 30; Cell < 35; ++Cell)
	{
		TestTrue(*FString::Printf(TEXT("hall cell %d"), Cell), Sim->IsCellBlocked(Cell, 40, 0, true));
	}
	TestFalse(TEXT("cell after the hall is open"), Sim->IsCellBlocked(35, 40, 0, true));

	FKodBuildSpec Pad;
	Pad.TypeId = FName(TEXT("ParityPad"));
	Pad.FootprintX = 2;
	Pad.FootprintY = 2;
	Pad.bCanLiftoff = true;
	Pad.LiftSpeedUU = 1000000;
	Pad.MaxHealth = 400.f;
	Sim->RegisterBuildSpec(Pad);
	const FVector PadHome(KodBuildGrid::PivotForOrigin(8, 2), KodBuildGrid::PivotForOrigin(8, 2), 25.f);
	AActor* PadActor = SpawnRooted(World, PadHome);
	FKodStructureSpec PadRow;
	PadRow.DefinitionId = Pad.TypeId;
	PadRow.FootprintX = 2;
	PadRow.FootprintY = 2;
	PadRow.MaxHealth = 400.f;
	const FKodEntityId PadId = Sim->RegisterStructure(PadActor, PadRow);
	PadActor->SetActorRotation(FRotator(9.f, 40.f, -3.f));
	TestTrue(TEXT("even liftoff"), Sim->IssueLiftoff(PadId));
	const FVector PadLand(KodBuildGrid::CellCenter(14), KodBuildGrid::CellCenter(6), 25.f);
	const FKodPlacementResult PadGhost = Sim->QueryPlacement(0, Pad.TypeId, PadLand);
	TestTrue(TEXT("even land ghost"), PadGhost.bValid);
	TestTrue(TEXT("even land order"), Sim->IssueLand(PadId, PadLand));
	Sim->AdvanceSimTicks(1);
	FKodStructureState PadState;
	TestTrue(TEXT("even landed"), Sim->TryGetStructure(PadId, PadState));
	TestFalse(TEXT("even on the ground"), PadState.bAirborne);
	TestEqual(TEXT("even landed cell"), PadState.CellX, PadGhost.CellX);
	TestEqual(TEXT("even landed cell y"), PadState.CellY, PadGhost.CellY);
	const FKodLandingPose PadPose = KodLanding::MakePose(PadGhost.CellX, PadGhost.CellY, 2, 2, 25.f);
	TestTrue(TEXT("even resnap matches the ghost"),
		KodLanding::SameTransform(PadState.Position, KodLanding::CanonicalYaw(), PadPose.Location, PadPose.Yaw));
	TestTrue(TEXT("even actor yaw is the authored facing"),
		FMath::Abs(FMath::FindDeltaAngleDegrees(PadActor->GetActorRotation().Yaw, PadPose.Yaw)) <= 0.01f);

	const FVector HallHome(KodBuildGrid::PivotForOrigin(50, 5), KodBuildGrid::PivotForOrigin(50, 5), 25.f);
	AActor* HallLift = SpawnRooted(World, HallHome);
	FKodStructureSpec HallLiftRow;
	HallLiftRow.DefinitionId = Hall.TypeId;
	HallLiftRow.FootprintX = 5;
	HallLiftRow.FootprintY = 5;
	HallLiftRow.MaxHealth = 400.f;
	const FKodEntityId HallId = Sim->RegisterStructure(HallLift, HallLiftRow);
	HallLift->SetActorRotation(FRotator(-4.f, 15.f, 6.f));
	TestTrue(TEXT("odd liftoff"), Sim->IssueLiftoff(HallId));
	const FVector HallLand(12050.f, 8050.f, 25.f);
	const FKodPlacementResult HallGhost = Sim->QueryPlacement(0, Hall.TypeId, HallLand);
	TestTrue(TEXT("odd land ghost"), HallGhost.bValid);
	TestTrue(TEXT("odd land order"), Sim->IssueLand(HallId, HallLand));
	Sim->AdvanceSimTicks(1);
	FKodStructureState HallState;
	TestTrue(TEXT("odd landed"), Sim->TryGetStructure(HallId, HallState));
	TestFalse(TEXT("odd on the ground"), HallState.bAirborne);
	TestEqual(TEXT("odd landed cell"), HallState.CellX, HallGhost.CellX);
	const FKodLandingPose HallPose = KodLanding::MakePose(HallGhost.CellX, HallGhost.CellY, 5, 5, 25.f);
	TestTrue(TEXT("odd resnap is a cell centre"),
		FMath::IsNearlyEqual(FMath::Fmod(HallState.Position.X, static_cast<float>(KodBuildGrid::CellUU)), 100.f, 0.1f));
	TestTrue(TEXT("odd resnap matches the ghost"),
		KodLanding::SameTransform(HallState.Position, KodLanding::CanonicalYaw(), HallPose.Location, HallPose.Yaw));
	TestTrue(TEXT("odd actor yaw is the authored facing"),
		FMath::Abs(FMath::FindDeltaAngleDegrees(HallLift->GetActorRotation().Yaw, HallPose.Yaw)) <= 0.01f);

	World->DestroyWorld(false);
	return true;
}
