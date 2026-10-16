#include "Components/SceneComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "Game/KodRTSCameraPawn.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Slice0/KodSlice0GameMode.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodPlayStartTest,
	"Kod.Play.StartOnBase",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodPlayStartTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString PackageName = FString::Printf(TEXT("/Temp/KodPlayStart_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodPlayStart")), Package, true);
	TestNotNull(TEXT("world"), World);
	if (!World)
	{
		return false;
	}
	FURL Url;
	World->InitializeActorsForPlay(Url);

	World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(FRotator(-20.f, 10.f, 0.f)));
	World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(FRotator(-40.f, 80.f, 0.f)));
	const int32 Enabled = AKodSlice0GameMode::KeepSingleDirectionalSun(World, false);
	TestEqual(TEXT("one directional light"), Enabled, 1);

	AActor* Base = World->SpawnActor<AActor>();
	TestNotNull(TEXT("base"), Base);
	if (Base)
	{
		USceneComponent* Root = NewObject<USceneComponent>(Base, TEXT("Root"));
		Base->SetRootComponent(Root);
		Root->RegisterComponent();
		Base->SetActorLocation(FVector(4200.f, -3600.f, 80.f));
		Base->Tags.Add(FName(TEXT("KodBuildingCommandCenter")));
	}
	AKodRTSCameraPawn* CameraPawn = World->SpawnActor<AKodRTSCameraPawn>(AKodRTSCameraPawn::StaticClass(), FTransform(FVector(0.f, 0.f, 0.f)));
	TestNotNull(TEXT("camera"), CameraPawn);
	if (CameraPawn)
	{
		CameraPawn->SetActorLocation(FVector(0.f, 0.f, 0.f));
		CameraPawn->SnapToPlayerStart();
		const float Distance = Base ? FVector::Dist2D(CameraPawn->GetActorLocation(), Base->GetActorLocation()) : 99999.f;
		TestTrue(TEXT("camera on the command center"), Distance < 500.f);
		TestTrue(TEXT("command center is not at the origin"), Base && FVector::Dist2D(Base->GetActorLocation(), FVector::ZeroVector) > 500.f);
	}

	World->DestroyWorld(false);
	return true;
}
