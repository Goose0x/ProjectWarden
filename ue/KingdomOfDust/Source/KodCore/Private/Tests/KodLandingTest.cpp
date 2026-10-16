#include "Game/KodLanding.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodBuildTypes.h"
#include "Sim/KodProductionTypes.h"
#include "Sim/KodSimSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodLandingOrientationTest,
	"Kod.Build.Landing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodLandingOrientationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FKodLandingPose Pose = KodLanding::MakePose(1, 2, 2, 2, 40.f);
	TestTrue(TEXT("canonical yaw is the debug offset"), FMath::IsNearlyEqual(Pose.Yaw, KodLanding::CanonicalYaw(), 0.001f));
	TestTrue(TEXT("center is on the footprint"), FMath::IsNearlyEqual(Pose.Location.X, 400.f, 0.1f));
	TestTrue(TEXT("center y"), FMath::IsNearlyEqual(Pose.Location.Y, 600.f, 0.1f));

	const FRotator Perturbed(12.f, Pose.Yaw + 35.f, -4.f);
	const FVector OffGrid = Pose.Location + FVector(40.f, -25.f, 15.f);
	TArray<FVector> Local;
	TArray<FVector> World;
	Local.Add(FVector(120.f, 0.f, 0.f));
	World.Add(OffGrid + FRotator(0.f, Perturbed.Yaw, 0.f).RotateVector(Local[0]));
	const FKodLandingFault Fault = KodLanding::Evaluate(Perturbed, OffGrid, Pose, Local, World);
	TestTrue(TEXT("validator catches yaw"), Fault.bYaw);
	TestTrue(TEXT("validator catches tilt"), Fault.bLevel);
	TestTrue(TEXT("validator catches grid"), Fault.bGrid);
	TestTrue(TEXT("validator catches socket side"), Fault.bSocket);

	const FString PackageName = FString::Printf(TEXT("/Temp/KodLanding_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodLanding")), Package, true);
	TestNotNull(TEXT("world"), World);
	if (!World)
	{
		return false;
	}
	FURL Url;
	World->InitializeActorsForPlay(Url);

	AActor* Building = World->SpawnActor<AActor>();
	TestNotNull(TEXT("building"), Building);
	if (!Building)
	{
		World->DestroyWorld(false);
		return false;
	}
	Building->SetActorLocation(OffGrid);
	Building->SetActorRotation(Perturbed);
	TestFalse(TEXT("perturbed landing is corrected"), KodLanding::ValidateLandingOrientation(Building, Pose));
	TestTrue(TEXT("landed yaw is canonical"), FMath::Abs(FMath::FindDeltaAngleDegrees(Building->GetActorRotation().Yaw, Pose.Yaw)) <= 0.01f);
	TestTrue(TEXT("landed pitch is flat"), FMath::Abs(Building->GetActorRotation().Pitch) <= 0.01f);
	TestTrue(TEXT("landed roll is flat"), FMath::Abs(Building->GetActorRotation().Roll) <= 0.01f);
	TestTrue(TEXT("landed on the cell"), KodLanding::SameTransform(Building->GetActorLocation(), Building->GetActorRotation().Yaw, Pose.Location, Pose.Yaw));
	TestTrue(TEXT("second check is clean"), KodLanding::ValidateLandingOrientation(Building, Pose));

	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	TestNotNull(TEXT("sim"), Sim);
	if (Sim)
	{
		FKodBuildSpec Box;
		Box.TypeId = FName(TEXT("WallBox"));
		Box.FootprintX = 2;
		Box.FootprintY = 2;
		Box.Cost.Credits = 0;
		Box.Cost.Luminene = 0;
		Sim->RegisterBuildSpec(Box);

		auto Occupy = [World, Sim](int32 CellX, int32 CellY, int32 Foot)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			Actor->SetActorLocation(FVector(KodBuildGrid::CellCenter(CellX), KodBuildGrid::CellCenter(CellY), 0.f));
			FKodStructureSpec Spec;
			Spec.DefinitionId = FName(TEXT("WallBox"));
			Spec.FootprintX = Foot;
			Spec.FootprintY = Foot;
			Spec.MaxHealth = 100.f;
			Sim->RegisterStructure(Actor, Spec);
		};
		Occupy(30, 30, 2);
		const FVector Touch = FVector(KodBuildGrid::CellCenter(32), KodBuildGrid::CellCenter(30), 0.f);
		const FKodPlacementResult Beside = Sim->QueryPlacement(0, Box.TypeId, Touch);
		TestTrue(TEXT("adjacent footprint is valid"), Beside.bValid);
		TestEqual(TEXT("adjacent origin x"), Beside.CellX, 32);
		TestEqual(TEXT("adjacent origin y"), Beside.CellY, 30);
		const FVector Overlap = FVector(KodBuildGrid::CellCenter(31), KodBuildGrid::CellCenter(30), 0.f);
		TestFalse(TEXT("overlapping footprint is blocked"), Sim->QueryPlacement(0, Box.TypeId, Overlap).bValid);

		auto Block = [Occupy](int32 CellX, int32 CellY)
		{
			Occupy(CellX, CellY, 1);
		};
		for (int32 X = -1; X <= 9; ++X)
		{
			Block(X, -1);
			Block(X, 3);
		}
		for (int32 Y = 0; Y <= 2; ++Y)
		{
			Block(-1, Y);
			Block(9, Y);
		}
		Block(4, 0);
		Block(4, 1);
		Block(4, 2);
		const FVector From(KodBuildGrid::CellCenter(1), KodBuildGrid::CellCenter(1), 0.f);
		const FVector To(KodBuildGrid::CellCenter(7), KodBuildGrid::CellCenter(1), 0.f);
		TestEqual(TEXT("three-building wall blocks the corridor"), Sim->PlanMovePath(From, To, 0, false), -1);

		FKodBuildSpec Flyer;
		Flyer.TypeId = FName(TEXT("FlyerBox"));
		Flyer.FootprintX = 2;
		Flyer.FootprintY = 2;
		Flyer.bCanLiftoff = true;
		Flyer.LiftSpeedUU = 1000000;
		Flyer.Cost.Credits = 0;
		Flyer.Cost.Luminene = 0;
		Sim->RegisterBuildSpec(Flyer);
		AActor* FlyerActor = World->SpawnActor<AActor>();
		TestNotNull(TEXT("flyer"), FlyerActor);
		if (FlyerActor)
		{
			FlyerActor->SetActorLocation(FVector(KodBuildGrid::CellCenter(4), KodBuildGrid::CellCenter(4), 25.f));
			FKodStructureSpec FlyerSpec;
			FlyerSpec.DefinitionId = Flyer.TypeId;
			FlyerSpec.FootprintX = 2;
			FlyerSpec.FootprintY = 2;
			FlyerSpec.MaxHealth = 100.f;
			const FKodEntityId FlyerId = Sim->RegisterStructure(FlyerActor, FlyerSpec);
			FlyerActor->SetActorRotation(FRotator(8.f, 40.f, -6.f));
			TestTrue(TEXT("liftoff"), Sim->IssueLiftoff(FlyerId));
			const FVector LandCursor(KodBuildGrid::CellCenter(14), KodBuildGrid::CellCenter(6), 25.f);
			const FKodPlacementResult Ghost = Sim->QueryPlacement(0, Flyer.TypeId, LandCursor);
			TestTrue(TEXT("land ghost is valid"), Ghost.bValid);
			TestEqual(TEXT("even footprint origin x"), Ghost.CellX, 14);
			TestEqual(TEXT("even footprint origin y"), Ghost.CellY, 6);
			const FKodLandingPose GhostPose = KodLanding::MakePose(Ghost.CellX, Ghost.CellY, 2, 2, 25.f);
			TestTrue(TEXT("land order"), Sim->IssueLand(FlyerId, LandCursor));
			Sim->AdvanceSimTicks(1);
			FKodStructureState Landed;
			TestTrue(TEXT("landed structure"), Sim->TryGetStructure(FlyerId, Landed));
			TestFalse(TEXT("on the ground"), Landed.bAirborne);
			TestEqual(TEXT("landed cell matches the ghost"), Landed.CellX, Ghost.CellX);
			TestEqual(TEXT("landed cell y matches the ghost"), Landed.CellY, Ghost.CellY);
			TestTrue(TEXT("landed where the ghost showed"),
				KodLanding::SameTransform(Landed.Position, KodLanding::CanonicalYaw(), GhostPose.Location, GhostPose.Yaw));
			TestTrue(TEXT("actor matches the ghost pose"),
				KodLanding::SameTransform(FlyerActor->GetActorLocation(), FlyerActor->GetActorRotation().Yaw, GhostPose.Location, GhostPose.Yaw));
			TestTrue(TEXT("landed pitch is flat"), FMath::Abs(FlyerActor->GetActorRotation().Pitch) <= 0.01f);
			TestTrue(TEXT("landed roll is flat"), FMath::Abs(FlyerActor->GetActorRotation().Roll) <= 0.01f);
		}
	}

	World->DestroyWorld(false);
	return true;
}
