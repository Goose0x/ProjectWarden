#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodBuildTypes.h"
#include "Sim/KodProductionTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Sim/KodUnitSheet.h"
#include "Warden/KodWardenPaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodHideOxTest,
	"Kod.Unit.HideOx",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	FKodEntityId SpawnSheetUnit(UWorld* World, UKodSimSubsystem* Sim, FName TypeId, const FVector& At, int32 TeamId)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		Actor->SetActorLocation(At);
		const FKodEntityId Id = Sim->RegisterEntity(Actor);
		Sim->ConfigureEntity(Id, TypeId, 1.f, 1.f, 16.f, true, 9.f, 400.f, 1.f, TeamId, false, 34, 1);
		return Id;
	}

	FKodEntityId SpawnHall(UWorld* World, UKodSimSubsystem* Sim, FName TypeId, int32 TeamId)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		Actor->SetActorLocation(FVector(2000.f, 2000.f, 0.f));
		FKodStructureSpec Spec;
		Spec.DefinitionId = TypeId;
		Spec.TeamId = TeamId;
		Spec.SupplyProvided = 0;
		Spec.FootprintX = 5;
		Spec.FootprintY = 5;
		Spec.MaxHealth = 1500.f;
		Spec.Trains.Add(FName(KodWardenPaths::Id_Dozer));
		return Sim->RegisterStructure(Actor, Spec);
	}
}

bool FKodHideOxTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FKodUnitCsvRow Row;
	const FName HideId(KodWardenPaths::Id_ScnHideOx);
	TestTrue(TEXT("units.csv has SCN_HIDEOX"), FKodUnitSheet::Find(HideId, Row));
	TestEqual(TEXT("sheet credits"), Row.Credits, 100);
	TestEqual(TEXT("sheet luminene"), Row.Luminene, 0);
	TestEqual(TEXT("sheet supply cost"), Row.SupplyCost, 0);
	TestEqual(TEXT("sheet supply provided"), Row.SupplyProvided, 8);
	TestEqual(TEXT("sheet build seconds"), Row.BuildSeconds, 18.f);
	TestEqual(TEXT("sheet hp"), Row.Hp, 200.f);
	TestEqual(TEXT("sheet armor"), Row.Armor, 1);
	TestEqual(TEXT("sheet damage"), Row.Damage, 0.f);
	TestEqual(TEXT("sheet speed cells"), Row.MoveSpeedCells, 1.4f);
	TestEqual(TEXT("sheet sight cells"), Row.SightCells, 9.f);
	TestEqual(TEXT("trained at SCI_HQ"), Row.ProducedAt, FString(TEXT("SCI_HQ")));
	TestTrue(TEXT("placeholder"), Row.bPlaceholder);
	TestTrue(TEXT("armored tag"), Row.HasTag(TEXT("Armored")));
	TestTrue(TEXT("biological tag"), Row.HasTag(TEXT("Biological")));
	TestTrue(TEXT("soft collision tag"), Row.HasTag(TEXT("SoftCollision")));
	TestTrue(TEXT("ground tag"), Row.HasTag(TEXT("Ground")));
	TestFalse(TEXT("not air"), Row.HasTag(TEXT("Air")));

	const FKodTrainSpec SheetSpec = Row.ToTrainSpec();
	TestEqual(TEXT("producer is Scinder HQ"), SheetSpec.BuildingId, FName(KodWardenPaths::Id_ScinderHQ));
	TestTrue(TEXT("sheet spec"), SheetSpec.bFromSheet);
	TestTrue(TEXT("armored"), SheetSpec.bArmored);
	TestTrue(TEXT("biological"), SheetSpec.bBiological);
	TestTrue(TEXT("soft"), SheetSpec.bSoftCollision);
	TestFalse(TEXT("not infantry"), SheetSpec.bInfantry);
	TestFalse(TEXT("not a flyer"), SheetSpec.bFlyer);
	TestEqual(TEXT("train supply cost"), SheetSpec.Cost.Supply, Row.SupplyCost);
	TestEqual(TEXT("train supply provided"), SheetSpec.SupplyProvided, Row.SupplyProvided);
	TestEqual(TEXT("train ticks from the sheet"), SheetSpec.Cost.BuildTime, Row.BuildTicks());

	const FString PackageName = FString::Printf(TEXT("/Temp/KodHideOx_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodHideOx")), Package, true);
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

	Sim->RegisterTrainSpec(SheetSpec);
	int32 Used = 0;
	int32 Cap = 0;
	Sim->TryGetSupply(0, Used, Cap);
	TestEqual(TEXT("cap starts empty"), Cap, 0);

	const FKodEntityId First = SpawnSheetUnit(World, Sim, HideId, FVector(400.f, 400.f, 0.f), 0);
	FKodSimEntityState Alive;
	TestTrue(TEXT("hide-ox state"), Sim->TryGetState(First, Alive));
	TestEqual(TEXT("hp from the sheet"), Alive.MaxHealth, Row.Hp);
	TestEqual(TEXT("health from the sheet"), Alive.Health, Row.Hp);
	TestEqual(TEXT("speed from the sheet"), Alive.MoveSpeed, Row.MoveSpeedUU());
	TestEqual(TEXT("sight from the sheet"), Alive.SightRadiusUU, Row.SightUU());
	TestEqual(TEXT("armor from the sheet"), Alive.ArmorPoints, Row.Armor);
	TestEqual(TEXT("no attack"), Alive.WeaponDamage, 0.f);
	Sim->TryGetSupply(0, Used, Cap);
	TestEqual(TEXT("living hide-ox raises the cap"), Cap, Row.SupplyProvided);

	const FKodEntityId Second = SpawnSheetUnit(World, Sim, HideId, FVector(800.f, 400.f, 0.f), 0);
	Sim->TryGetSupply(0, Used, Cap);
	TestEqual(TEXT("two hide-ox stack supply"), Cap, Row.SupplyProvided * 2);
	Sim->UnregisterEntity(Second);
	Sim->TryGetSupply(0, Used, Cap);
	TestEqual(TEXT("death removes one"), Cap, Row.SupplyProvided);
	Sim->UnregisterEntity(First);
	Sim->TryGetSupply(0, Used, Cap);
	TestEqual(TEXT("death removes the last"), Cap, 0);

	FKodTrainSpec Pile;
	Pile.UnitId = FName(TEXT("SupplyPile"));
	Pile.SupplyProvided = KodEconomyDefaults::SupplyHardCap + Row.SupplyProvided;
	Sim->RegisterTrainSpec(Pile);
	const FKodEntityId PileId = SpawnSheetUnit(World, Sim, Pile.UnitId, FVector(1200.f, 400.f, 0.f), 0);
	Sim->TryGetSupply(0, Used, Cap);
	TestEqual(TEXT("supply clamps at 200"), Cap, KodEconomyDefaults::SupplyHardCap);
	Sim->UnregisterEntity(PileId);
	Sim->TryGetSupply(0, Used, Cap);
	TestEqual(TEXT("despawn clears the pile"), Cap, 0);

	FKodBuildSpec Pad;
	Pad.TypeId = FName(TEXT("HideOxPad"));
	Pad.FootprintX = 1;
	Pad.FootprintY = 1;
	Sim->RegisterBuildSpec(Pad);

	const FVector SoftAt(KodBuildGrid::CellCenter(4), KodBuildGrid::CellCenter(4), 0.f);
	SpawnSheetUnit(World, Sim, HideId, SoftAt, 1);
	const FKodPlacementResult SoftPlace = Sim->QueryPlacement(0, Pad.TypeId, SoftAt);
	TestTrue(TEXT("soft hide-ox does not wall"), SoftPlace.bValid);

	FKodTrainSpec Wall;
	Wall.UnitId = FName(TEXT("HideOxWall"));
	Sim->RegisterTrainSpec(Wall);
	const FVector HardAt(KodBuildGrid::CellCenter(8), KodBuildGrid::CellCenter(8), 0.f);
	SpawnSheetUnit(World, Sim, Wall.UnitId, HardAt, 1);
	const FKodPlacementResult HardPlace = Sim->QueryPlacement(0, Pad.TypeId, HardAt);
	TestFalse(TEXT("a hard enemy still walls"), HardPlace.bValid);

	FKodStructureState Hall;
	TestTrue(TEXT("vanguard hall"), Sim->TryGetStructure(SpawnHall(World, Sim, FName(KodWardenPaths::Id_CommandCenter), 0), Hall));
	TestFalse(TEXT("vanguard hall does not train hide-ox"), Hall.Trains.Contains(HideId));
	TestTrue(TEXT("scinder team hall"), Sim->TryGetStructure(SpawnHall(World, Sim, FName(KodWardenPaths::Id_CommandCenter), 1), Hall));
	TestTrue(TEXT("scinder command center trains hide-ox"), Hall.Trains.Contains(HideId));
	TestTrue(TEXT("scinder hq"), Sim->TryGetStructure(SpawnHall(World, Sim, FName(KodWardenPaths::Id_ScinderHQ), 0), Hall));
	TestTrue(TEXT("scinder hq trains hide-ox"), Hall.Trains.Contains(HideId));

	Sim->SetPlayerFaction(1);
	TestTrue(TEXT("scinder player hall"), Sim->TryGetStructure(SpawnHall(World, Sim, FName(KodWardenPaths::Id_CommandCenter), 0), Hall));
	TestTrue(TEXT("scinder player trains hide-ox at the command center"), Hall.Trains.Contains(HideId));

	World->DestroyWorld(false);
	return true;
}
