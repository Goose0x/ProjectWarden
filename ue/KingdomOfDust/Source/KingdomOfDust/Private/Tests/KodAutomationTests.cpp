#if WITH_DEV_AUTOMATION_TESTS

#include "Actors/KodBuilding.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Sim/KodResourceTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Slice0/KodFaultline.h"
#include "Slice0/KodWorldSettings.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	UWorld* MakeKodTestWorld()
	{
		const FString PackageName = FString::Printf(TEXT("/Temp/KodGate0_%s"), *FGuid::NewGuid().ToString());
		UPackage* Package = CreatePackage(*PackageName);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodGate0")), Package, true);
		if (!World)
		{
			return nullptr;
		}
		FURL Url;
		World->InitializeActorsForPlay(Url);
		if (ULevel* Level = World->PersistentLevel)
		{
			// 5.8 SetWorldSettings looks the actor up in Level->Actors, so spawn it instead of NewObject.
			FActorSpawnParameters SpawnParams;
			SpawnParams.OverrideLevel = Level;
			AKodWorldSettings* Settings = World->SpawnActor<AKodWorldSettings>(SpawnParams);
			if (!Settings)
			{
				return World;
			}
			Settings->bFaultlineTraining = true;
			Level->SetWorldSettings(Settings);
		}
		return World;
	}

	void DestroyKodTestWorld(UWorld* World)
	{
		if (World)
		{
			World->DestroyWorld(false);
		}
	}

	int32 CountRegisteredOutposts(UWorld* World, UKodSimSubsystem* Sim)
	{
		int32 Count = 0;
		if (!World || !Sim)
		{
			return 0;
		}
		const FName Tag(TEXT("KodEnemyOutpost"));
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->ActorHasTag(Tag))
			{
				continue;
			}
			if (Sim->IsStructure(Sim->FindIdForActor(*It)))
			{
				++Count;
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKodFaultlineOutpostsTest, "Kod.Faultline.Outposts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKodFaultlineOutpostsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = MakeKodTestWorld();
	TestNotNull(TEXT("Faultline test world"), World);
	if (!World)
	{
		return false;
	}
	KodFaultline::Begin(World);
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	TestNotNull(TEXT("sim"), Sim);
	const int32 Outposts = CountRegisteredOutposts(World, Sim);
	TestTrue(TEXT("Faultline registers more than zero outposts"), Outposts > 0);
	const bool bMapPackage = FPackageName::DoesPackageExist(TEXT("/Game/Maps/Faultline/L_FaultlineTraining"))
		|| FPackageName::DoesPackageExist(TEXT("/Game/Maps/L_FaultlineTraining"));
	if (!bMapPackage)
	{
		AddInfo(TEXT("L_FaultlineTraining package is not in this workspace; registration was checked from layout v2"));
	}
	DestroyKodTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKodJadeiteCollectionTest, "Kod.Jadeite.Collection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKodJadeiteCollectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TCHAR* PackagePath = TEXT("/Game/Warden/Materials/MPC_KodJadeite");
	const bool bPackage = FPackageName::DoesPackageExist(PackagePath);
	UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(
		nullptr,
		TEXT("/Game/Warden/Materials/MPC_KodJadeite.MPC_KodJadeite"),
		nullptr,
		LOAD_NoWarn | LOAD_Quiet);
	TestTrue(TEXT("MPC_KodJadeite exists (run Tools/kod_jadeite_mpc_setup.py)"), bPackage || Collection != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKodTrainDozerTest, "Kod.Train.Dozer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FKodTrainDozerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = MakeKodTestWorld();
	TestNotNull(TEXT("train test world"), World);
	if (!World)
	{
		return false;
	}
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	TestNotNull(TEXT("sim"), Sim);
	if (!Sim)
	{
		DestroyKodTestWorld(World);
		return false;
	}
	Sim->SetAiEnabled(false);
	FKodTrainSpec Spec;
	Spec.UnitId = FName(KodWardenPaths::Id_Dozer);
	Spec.DisplayId = Spec.UnitId;
	Spec.Cost.Credits = KodEconomyDefaults::AmpCredits;
	Spec.Cost.Luminene = KodEconomyDefaults::AmpLuminene;
	Spec.Cost.Supply = KodEconomyDefaults::AmpSupply;
	Spec.Cost.BuildTime = KodEconomyDefaults::AmpBuildTicks;
	Sim->RegisterTrainSpec(Spec);
	Sim->SetTeamBank(0, KodEconomyDefaults::AmpCredits, 0);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Center = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(0.f, 0.f, 0.f)), Params);
	TestNotNull(TEXT("command center actor"), Center);
	if (!Center)
	{
		DestroyKodTestWorld(World);
		return false;
	}
	FKodStructureSpec Structure;
	Structure.DefinitionId = FName(KodWardenPaths::Id_CommandCenter);
	Structure.TeamId = 0;
	Structure.SupplyProvided = KodEconomyDefaults::SupplyPerHq;
	Structure.bIsDropOff = true;
	Structure.MaxHealth = 2500.f;
	Structure.FootprintX = 5;
	Structure.FootprintY = 5;
	Structure.Trains.Add(Spec.UnitId);
	const FKodEntityId Built = Sim->RegisterStructure(Center, Structure);
	TestTrue(TEXT("command center registered"), Built.IsValid());
	TestTrue(TEXT("enqueue Dozer"), Sim->EnqueueTrain(Built, Spec.UnitId));
	Sim->AdvanceSimTicks(KodEconomyDefaults::AmpBuildTicks + 2);
	int32 Collected = 0;
	int32 Trained = 0;
	int32 Killed = 0;
	int32 BuiltCount = 0;
	int32 Razed = 0;
	Sim->GetTeamTotals(0, Collected, Trained, Killed, BuiltCount, Razed);
	TestTrue(TEXT("Dozer finishes inside its build ticks"), Trained > 0);
	DestroyKodTestWorld(World);
	return true;
}

#endif
