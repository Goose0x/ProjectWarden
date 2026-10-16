#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodBuildTypes.h"
#include "Sim/KodProductionTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Warden/KodWardenPaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodBunkerTest,
	"Kod.Build.Bunker",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
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

	FKodEntityId SpawnUnit(
		UWorld* World,
		UKodSimSubsystem* Sim,
		FName TypeId,
		const FVector& At,
		int32 TeamId,
		float Health,
		float Damage,
		float Range)
	{
		AActor* Actor = SpawnRooted(World, At);
		const FKodEntityId Id = Sim->RegisterEntity(Actor);
		Sim->ConfigureEntity(Id, TypeId, Health, 400.f, 16.f, true, Damage, Range, 1.f, TeamId, false, 34, 2000);
		return Id;
	}
}

bool FKodBunkerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString PackageName = FString::Printf(TEXT("/Temp/KodBunker_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodBunker")), Package, true);
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

	FKodTrainSpec MarineSpec;
	MarineSpec.UnitId = FName(KodWardenPaths::Id_Marine);
	MarineSpec.bInfantry = true;
	MarineSpec.bBiological = true;
	MarineSpec.CargoSize = 1;
	Sim->RegisterTrainSpec(MarineSpec);

	FKodBuildSpec Barracks;
	Barracks.TypeId = FName(KodWardenPaths::Id_Barracks);
	Barracks.FootprintX = 3;
	Barracks.FootprintY = 3;
	Sim->RegisterBuildSpec(Barracks);

	FKodBuildSpec BunkerSpec;
	BunkerSpec.TypeId = FName(KodWardenPaths::Id_Bunker);
	BunkerSpec.FootprintX = 3;
	BunkerSpec.FootprintY = 3;
	BunkerSpec.CargoCap = 4;
	BunkerSpec.MaxHealth = 400.f;
	BunkerSpec.Armor = 1;
	BunkerSpec.bArmored = true;
	BunkerSpec.bStructure = true;
	BunkerSpec.Cost.Credits = 100;
	BunkerSpec.Cost.BuildTime = 29 * 16;
	BunkerSpec.RequiresId = FName(KodWardenPaths::Id_Barracks);
	Sim->RegisterBuildSpec(BunkerSpec);

	Sim->SetPlayableWorldBox(FBox(FVector(-4000.f, -4000.f, -200.f), FVector(20000.f, 20000.f, 2000.f)));
	const FVector BlockedCursor(KodBuildGrid::CellCenter(40), KodBuildGrid::CellCenter(40), 0.f);
	const FKodPlacementResult NeedsBarracks = Sim->QueryPlacement(0, BunkerSpec.TypeId, BlockedCursor);
	TestFalse(TEXT("bunker needs a barracks"), NeedsBarracks.bValid);
	TestTrue(TEXT("require reason"), NeedsBarracks.Reason == FName(TEXT("Requires")));

	AActor* BarracksActor = SpawnRooted(World, FVector(2300.f, 400.f, 0.f));
	FKodStructureSpec BarracksRow;
	BarracksRow.DefinitionId = Barracks.TypeId;
	BarracksRow.FootprintX = 3;
	BarracksRow.FootprintY = 3;
	BarracksRow.MaxHealth = 1500.f;
	Sim->RegisterStructure(BarracksActor, BarracksRow);
	const FKodPlacementResult Open = Sim->QueryPlacement(0, BunkerSpec.TypeId, BlockedCursor);
	TestTrue(TEXT("bunker places after a barracks"), Open.bValid);

	const FVector BunkerAt(2300.f, 2300.f, 0.f);
	AActor* BunkerActor = SpawnRooted(World, BunkerAt);
	FKodStructureSpec BunkerRow;
	BunkerRow.DefinitionId = BunkerSpec.TypeId;
	BunkerRow.FootprintX = 3;
	BunkerRow.FootprintY = 3;
	BunkerRow.MaxHealth = 400.f;
	const FKodEntityId BunkerId = Sim->RegisterStructure(BunkerActor, BunkerRow);
	FKodStructureState Bunker;
	TestTrue(TEXT("bunker registered"), Sim->TryGetStructure(BunkerId, Bunker));
	TestEqual(TEXT("cargo cap"), Bunker.CargoCap, 4);
	FKodSimEntityState BunkerCombat;
	TestTrue(TEXT("bunker combat"), Sim->TryGetState(BunkerId, BunkerCombat));
	TestEqual(TEXT("armour 1"), BunkerCombat.ArmorPoints, 1);

	const FKodEntityId Marine = SpawnUnit(World, Sim, MarineSpec.UnitId, BunkerAt, 0, 120.f, 15.f, 200.f);
	TestTrue(TEXT("load"), Sim->LoadUnitIntoBunker(Marine, BunkerId));
	FKodSimEntityState Loaded;
	TestTrue(TEXT("marine state"), Sim->TryGetState(Marine, Loaded));
	TestEqual(TEXT("marine is inside"), Loaded.InsideBunker.Value, BunkerId.Value);
	TestTrue(TEXT("unload all"), Sim->UnloadAll(BunkerId));
	FKodSimEntityState Unloaded;
	TestTrue(TEXT("unloaded state"), Sim->TryGetState(Marine, Unloaded));
	TestFalse(TEXT("marine left the bunker"), Unloaded.InsideBunker.IsValid());
	Sim->TryGetStructure(BunkerId, Bunker);
	TestEqual(TEXT("cargo empty"), Bunker.Cargo.Num(), 0);
	TestTrue(TEXT("load again"), Sim->LoadUnitIntoBunker(Marine, BunkerId));

	const FKodEntityId Enemy = SpawnUnit(World, Sim, FName(TEXT("Target")), BunkerAt + FVector(400.f, 0.f, 0.f), 1, 100.f, 0.f, 200.f);
	Sim->AdvanceSimTicks(3);
	FKodSimEntityState Shot;
	TestTrue(TEXT("enemy still there"), Sim->TryGetState(Enemy, Shot));
	TestTrue(TEXT("marine fired from inside with the range bonus"), Shot.Health < 100.f);

	Sim->ConfigureEntity(Enemy, FName(TEXT("Target")), 100.f, 400.f, 16.f, true, 5000.f, 2000.f, 1.f, 1, false, 34, 2000);
	Sim->IssueAttack(Enemy, BunkerId);
	Sim->AdvanceSimTicks(4);
	TestFalse(TEXT("bunker is destroyed"), Sim->IsStructure(BunkerId));
	FKodSimEntityState Ejected;
	TestTrue(TEXT("marine survived the eject"), Sim->TryGetState(Marine, Ejected));
	TestTrue(TEXT("marine is alive"), Ejected.Health > 0.f);
	TestFalse(TEXT("death ejected the marine"), Ejected.InsideBunker.IsValid());

	const FVector TurretAt(8300.f, 2300.f, 0.f);
	AActor* TurretBunkerActor = SpawnRooted(World, TurretAt);
	const FKodEntityId TurretBunker = Sim->RegisterStructure(TurretBunkerActor, BunkerRow);
	FKodUpgradeSpec Turret;
	Turret.UpgradeId = FName(KodWardenPaths::Id_BunkerTurret);
	Turret.BuildingId = FName(KodWardenPaths::Id_Bunker);
	Turret.Credits = 50;
	Turret.Luminene = 25;
	Turret.AttackDamage = 12;
	Turret.AttackRangeUU = 1200;
	Turret.AttacksAir = 0;
	Turret.CooldownTicks = 14;
	Turret.BonusVsLight = 6;
	Turret.BonusHealth = 100;
	Turret.BuildTicks = 1;
	Sim->RegisterUpgradeSpec(Turret);
	FKodResourceCost TurretPay;
	TurretPay.Jadeite = 50;
	TurretPay.Luminene = 25;
	Sim->Deposit(0, TurretPay);
	FKodTrainSpec LightSpec;
	LightSpec.UnitId = FName(TEXT("LightTarget"));
	LightSpec.bLight = true;
	Sim->RegisterTrainSpec(LightSpec);
	TestTrue(TEXT("turret research"), Sim->EnqueueResearch(TurretBunker, Turret.UpgradeId));
	const FKodEntityId TurretTarget = SpawnUnit(World, Sim, LightSpec.UnitId, TurretAt + FVector(400.f, 0.f, 0.f), 1, 80.f, 0.f, 200.f);
	Sim->AdvanceSimTicks(3);
	FKodStructureState Armed;
	TestTrue(TEXT("turret bunker"), Sim->TryGetStructure(TurretBunker, Armed));
	TestTrue(TEXT("turret installed"), Armed.bTurret);
	TestEqual(TEXT("turret damage"), Armed.AttackDamage, 12);
	TestEqual(TEXT("turret adds 100 HP"), FMath::RoundToInt(Armed.MaxHealth), 500);
	TestEqual(TEXT("empty cargo"), Armed.Cargo.Num(), 0);
	FKodSimEntityState TurretHull;
	TestTrue(TEXT("turret hull"), Sim->TryGetState(TurretBunker, TurretHull));
	TestEqual(TEXT("current HP rose with the bonus"), FMath::RoundToInt(TurretHull.Health), 500);
	FKodSimEntityState TurretShot;
	TestTrue(TEXT("light target"), Sim->TryGetState(TurretTarget, TurretShot));
	TestEqual(TEXT("empty bunker turret hit Light for 18"), FMath::RoundToInt(TurretShot.Health), 62);

	const FVector YardAt(12300.f, 2300.f, 0.f);
	AActor* YardActor = SpawnRooted(World, YardAt);
	const FKodEntityId YardId = Sim->RegisterStructure(YardActor, BunkerRow);
	const FKodEntityId Siege = SpawnUnit(World, Sim, FName(TEXT("Siege")), YardAt + FVector(100.f, 0.f, 0.f), 1, 50.f, 101.f, 500.f);
	Sim->IssueAttack(Siege, YardId);
	Sim->AdvanceSimTicks(3);
	Sim->IssueStop(Siege);
	FKodSimEntityState YardHurt;
	TestTrue(TEXT("yard damaged"), Sim->TryGetState(YardId, YardHurt));
	TestEqual(TEXT("armour shaved one point"), FMath::RoundToInt(YardHurt.Health), 300);
	FKodResourceCost Grant;
	Grant.Jadeite = 10;
	Sim->Deposit(0, Grant);
	const FKodEntityId Amp = SpawnUnit(World, Sim, FName(KodWardenPaths::Id_Dozer), YardAt, 0, 60.f, 0.f, 0.f);
	Sim->ConfigureEntity(Amp, FName(KodWardenPaths::Id_Dozer), 60.f, 400.f, 16.f, true, 0.f, 0.f, 1.f, 0, true, 34, 1200);
	TestTrue(TEXT("AMP repairs"), Sim->IssueRepair(Amp, YardId));
	Sim->AdvanceSimTicks(8);
	FKodSimEntityState YardFixed;
	TestTrue(TEXT("yard after repair"), Sim->TryGetState(YardId, YardFixed));
	TestTrue(TEXT("repair restored HP"), YardFixed.Health > 300.f);
	FKodResourceCost Bank;
	TestTrue(TEXT("bank"), Sim->TryGetBank(0, Bank));
	TestTrue(TEXT("repair spent credits"), Bank.Jadeite < 10);
	const int32 AfterRepair = Bank.Jadeite;
	TestTrue(TEXT("salvage started"), Sim->IssueSalvage(YardId));
	Sim->AdvanceSimTicks(79);
	TestTrue(TEXT("salvage still running at 5s minus one tick"), Sim->IsStructure(YardId));
	Sim->AdvanceSimTicks(1);
	TestFalse(TEXT("salvage removed the bunker"), Sim->IsStructure(YardId));
	TestTrue(TEXT("bank after refund"), Sim->TryGetBank(0, Bank));
	TestEqual(TEXT("salvage refunded 75%"), Bank.Jadeite, AfterRepair + 75);

	World->DestroyWorld(false);
	return true;
}
