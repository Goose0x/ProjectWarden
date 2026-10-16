#include "Actors/KodBuilding.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodProductionTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Warden/KodWardenPaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodBunkerTurretMeshTest,
	"Kod.Build.BunkerTurretMesh",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodBunkerTurretMeshTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString PackageName = FString::Printf(TEXT("/Temp/KodBunkerMesh_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodBunkerMesh")), Package, true);
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

	AKodBuilding* Building = World->SpawnActor<AKodBuilding>();
	TestNotNull(TEXT("building"), Building);
	if (!Building)
	{
		World->DestroyWorld(false);
		return false;
	}
	Building->SetActorLocation(FVector(2300.f, 2300.f, 0.f));
	FKodStructureSpec Row;
	Row.DefinitionId = FName(KodWardenPaths::Id_Bunker);
	Row.FootprintX = 3;
	Row.FootprintY = 3;
	Row.MaxHealth = 400.f;
	const FKodEntityId Id = Sim->RegisterStructure(Building, Row);
	Building->EntityId = Id;

	FKodUpgradeSpec Turret;
	Turret.UpgradeId = FName(KodWardenPaths::Id_BunkerTurret);
	Turret.BuildingId = FName(KodWardenPaths::Id_Bunker);
	Turret.AttackDamage = 6;
	Turret.AttackRangeUU = 600;
	Turret.BuildTicks = 1;
	Sim->RegisterUpgradeSpec(Turret);
	TestTrue(TEXT("research queued"), Sim->EnqueueResearch(Id, Turret.UpgradeId));
	Sim->AdvanceSimTicks(1);
	FKodStructureState Armed;
	TestTrue(TEXT("structure"), Sim->TryGetStructure(Id, Armed));
	TestTrue(TEXT("turret flag"), Armed.bTurret);

	Building->Tick(0.016f);
	bool bAttached = false;
	TInlineComponentArray<UStaticMeshComponent*> Meshes;
	Building->GetComponents(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh && Mesh->ComponentHasTag(FName(TEXT("KodBunkerTurret"))) && Mesh->GetAttachParent())
		{
			bAttached = true;
		}
	}
	TestTrue(TEXT("turret mesh attached"), bAttached);

	World->DestroyWorld(false);
	return true;
}
