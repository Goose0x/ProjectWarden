#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodSimSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodFogChaseTest,
	"Kod.Sim.FogChase",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodFogChaseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString PackageName = FString::Printf(TEXT("/Temp/KodFogChase_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodFogChase")), Package, true);
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

	AActor* AttackerActor = World->SpawnActor<AActor>();
	AActor* TargetActor = World->SpawnActor<AActor>();
	TestNotNull(TEXT("attacker actor"), AttackerActor);
	TestNotNull(TEXT("target actor"), TargetActor);
	if (!AttackerActor || !TargetActor)
	{
		World->DestroyWorld(false);
		return false;
	}
	auto PlaceActor = [](AActor* Actor, const FVector& At)
	{
		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();
		Actor->SetActorLocation(At);
	};
	PlaceActor(AttackerActor, FVector(2000.f, 0.f, 0.f));
	PlaceActor(TargetActor, FVector(6000.f, 0.f, 0.f));
	const FKodEntityId Attacker = Sim->RegisterEntity(AttackerActor);
	const FKodEntityId Victim = Sim->RegisterEntity(TargetActor);
	Sim->ConfigureEntity(Attacker, FName(TEXT("Marine")), 120.f, 400.f, 40.f, true, 6.f, 100.f, 0.5f, 1, false, 16, 100);
	Sim->ConfigureEntity(Victim, FName(TEXT("CommandCenter")), 1500.f, 0.f, 40.f, false, 0.f, 0.f, 0.f, 0, false, 64, 0);

	FKodSimEntityState Placed;
	TestTrue(TEXT("attacker placed off the origin"), Sim->TryGetState(Attacker, Placed) && Placed.Position.X > 1500.f);
	Sim->IssueAttack(Attacker, Victim);
	const float StartX = Placed.Position.X;
	Sim->AdvanceSimTicks(20);
	FKodSimEntityState After;
	TestTrue(TEXT("attacker still registered"), Sim->TryGetState(Attacker, After));
	const float MovedTowardTarget = After.Position.X - StartX;
	const float MovedTowardOrigin = StartX - After.Position.X;
	TestTrue(TEXT("moves toward the unseen target"), MovedTowardTarget > 50.f);
	TestTrue(TEXT("does not chase the origin"), MovedTowardTarget > MovedTowardOrigin);
	TestTrue(TEXT("keeps the attack on the known target"), After.Order == EKodSimOrderType::Attack && After.AttackTarget == Victim);

	World->DestroyWorld(false);
	return true;
}
