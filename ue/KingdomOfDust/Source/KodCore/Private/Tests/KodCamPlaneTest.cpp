#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Game/KodCameraSettings.h"
#include "Game/KodRTSCameraPawn.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodCamPlaneTest,
	"Kod.Cam.Plane",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	AStaticMeshActor* SpawnGround(UWorld* World, const FVector& At, const FVector& Scale)
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(At));
		if (!Actor)
		{
			return nullptr;
		}
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
		if (!Cube || !Mesh)
		{
			return nullptr;
		}
		Mesh->SetStaticMesh(Cube);
		Mesh->SetWorldScale3D(Scale);
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Actor->Tags.Add(FName(TEXT("KodGround")));
		Actor->SetActorLocation(At);
		return Actor;
	}
}

bool FKodCamPlaneTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString PackageName = FString::Printf(TEXT("/Temp/KodCamPlane_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodCamPlane")), Package, true);
	TestNotNull(TEXT("world"), World);
	if (!World)
	{
		return false;
	}
	FURL Url;
	World->InitializeActorsForPlay(Url);
	AStaticMeshActor* Flat = SpawnGround(World, FVector(0.f, 0.f, 0.f), FVector(40.f, 40.f, 0.2f));
	AStaticMeshActor* Cliff = SpawnGround(World, FVector(8000.f, 0.f, 600.f), FVector(20.f, 20.f, 6.f));
	TestNotNull(TEXT("flat ground"), Flat);
	TestNotNull(TEXT("cliff"), Cliff);
	if (!Flat || !Cliff)
	{
		World->DestroyWorld(false);
		return false;
	}

	AKodRTSCameraPawn* CameraPawn = World->SpawnActor<AKodRTSCameraPawn>(
		AKodRTSCameraPawn::StaticClass(),
		FTransform(FVector(0.f, 0.f, 2500.f)));
	TestNotNull(TEXT("camera"), CameraPawn);
	if (!CameraPawn || !CameraPawn->CameraBoom || !CameraPawn->Camera)
	{
		World->DestroyWorld(false);
		return false;
	}

	const float Dt = 1.f / 60.f;
	CameraPawn->SetActorLocation(FVector(0.f, 0.f, 2500.f));
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		CameraPawn->Tick(Dt);
	}
	const float LockedZ = CameraPawn->GetActorLocation().Z;
	const float LockedPitch = CameraPawn->CameraBoom->GetRelativeRotation().Pitch;
	const float LockedArm = CameraPawn->GetCurrentArmLength();
	const float LockedCamZ = CameraPawn->Camera->GetComponentLocation().Z;
	TestTrue(TEXT("plane locked onto the ground"), LockedZ < 1000.f);
	TestTrue(TEXT("pitch is -56"), FMath::IsNearlyEqual(LockedPitch, KodCameraSettings::PitchDegrees, 0.05f));
	TestTrue(TEXT("distance is 34"), FMath::IsNearlyEqual(LockedArm, KodCameraSettings::ToUU(KodCameraSettings::DistanceSc2), 1.f));
	TestFalse(TEXT("spring arm does not collide"), CameraPawn->CameraBoom->bDoCollisionTest);

	const FVector Stops[] = {
		FVector(0.f, 0.f, LockedZ),
		FVector(2000.f, 500.f, LockedZ),
		FVector(8000.f, 0.f, LockedZ),
		FVector(8000.f, 1500.f, LockedZ),
		FVector(-1500.f, -1500.f, LockedZ)
	};
	for (const FVector& Stop : Stops)
	{
		CameraPawn->SetActorLocation(Stop);
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			CameraPawn->Tick(Dt);
		}
		TestTrue(TEXT("pan keeps Z"), FMath::IsNearlyEqual(CameraPawn->GetActorLocation().Z, LockedZ, 1.f));
		TestTrue(TEXT("pan keeps camera Z"), FMath::IsNearlyEqual(CameraPawn->Camera->GetComponentLocation().Z, LockedCamZ, 1.f));
		TestTrue(TEXT("pan keeps pitch"), FMath::IsNearlyEqual(CameraPawn->CameraBoom->GetRelativeRotation().Pitch, LockedPitch, 0.05f));
		TestTrue(TEXT("pan keeps distance"), FMath::IsNearlyEqual(CameraPawn->GetCurrentArmLength(), LockedArm, 1.f));
		TestTrue(TEXT("pan keeps yaw"), FMath::IsNearlyEqual(CameraPawn->GetActorRotation().Yaw, KodCameraSettings::YawDegrees, 0.05f));
		TestFalse(TEXT("collision stays off"), CameraPawn->CameraBoom->bDoCollisionTest);
	}

	World->DestroyWorld(false);
	return true;
}
