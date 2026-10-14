#include "Actors/KodUnit.h"
#include "Engine/World.h"
#include "Game/KodPlayerController.h"
#include "Game/KodRTSCameraPawn.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodSimSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodSelectOwnWorkerTest,
	"Kod.Play.SelectOwnWorker",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodSelectOwnWorkerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString PackageName = FString::Printf(TEXT("/Temp/KodSelectOwn_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodSelectOwn")), Package, true);
	TestNotNull(TEXT("world"), World);
	if (!World)
	{
		return false;
	}
	FURL Url;
	World->InitializeActorsForPlay(Url);

	AKodPlayerController* PC = World->SpawnActor<AKodPlayerController>();
	AKodRTSCameraPawn* CameraPawn = World->SpawnActor<AKodRTSCameraPawn>();
	TestNotNull(TEXT("controller"), PC);
	TestNotNull(TEXT("camera"), CameraPawn);
	if (!PC || !CameraPawn)
	{
		World->DestroyWorld(false);
		return false;
	}
	PC->Possess(CameraPawn);
	if (!CameraPawn->HasActorBegunPlay())
	{
		CameraPawn->DispatchBeginPlay();
	}
	CameraPawn->SetActorLocation(FVector::ZeroVector);
	CameraPawn->SnapToStartView(FVector::ZeroVector);

	const FVector WorkerAt(0.f, 0.f, 88.f);
	AKodUnit* Worker = World->SpawnActor<AKodUnit>(AKodUnit::StaticClass(), FTransform(WorkerAt));
	TestNotNull(TEXT("worker"), Worker);
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	TestNotNull(TEXT("sim"), Sim);
	if (!Worker || !Sim)
	{
		World->DestroyWorld(false);
		return false;
	}
	if (!Worker->HasActorBegunPlay())
	{
		Worker->DispatchBeginPlay();
	}
	Worker->TeamId = 0;
	Worker->SetActorLocation(WorkerAt);
	if (!Worker->EntityId.IsValid())
	{
		Worker->EntityId = Sim->RegisterEntity(Worker);
	}
	Sim->ConfigureEntity(
		Worker->EntityId,
		FName(TEXT("Dozer")),
		40.f,
		280.f,
		16.f,
		true,
		0.f,
		0.f,
		0.f,
		0,
		true,
		32,
		400);
	Sim->SetEntityGroundPosition(Worker->EntityId, WorkerAt);

	FVector2D Screen = FVector2D::ZeroVector;
	TestTrue(TEXT("worker projects to the screen"), PC->ProjectActorToMarqueeSpace(Worker, Screen));
	AActor* Picked = PC->SelectAtScreen(Screen, false);
	TestTrue(TEXT("click selects the worker"), Picked == Worker);
	TestTrue(TEXT("selection contains the worker"), PC->GetLocalSelection().Contains(Worker));

	const FVector Dest(600.f, 40.f, 88.f);
	PC->IssueMoveToSelection(Dest);
	FKodSimEntityState State;
	TestTrue(TEXT("worker state"), Sim->TryGetState(Worker->EntityId, State));
	TestEqual(TEXT("move order"), static_cast<uint8>(State.Order), static_cast<uint8>(EKodSimOrderType::Move));
	TestTrue(TEXT("move reaches the worker"), FVector::Dist2D(State.MoveTarget, Dest) < 50.f);

	World->DestroyWorld(false);
	return true;
}
