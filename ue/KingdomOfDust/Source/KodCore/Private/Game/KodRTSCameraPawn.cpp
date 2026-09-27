#include "Game/KodRTSCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SceneComponent.h"

AKodRTSCameraPawn::AKodRTSCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(SceneRoot);
	CameraBoom->TargetArmLength = 2200.f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.f;
	CameraBoom->SetRelativeRotation(FRotator(FixedPitchDegrees, 0.f, 0.f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
}

void AKodRTSCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (CameraBoom)
	{
		FRotator Rel = CameraBoom->GetRelativeRotation();
		Rel.Pitch = FixedPitchDegrees;
		if (!bAllowYawOrbit)
		{
			Rel.Yaw = 0.f;
		}
		CameraBoom->SetRelativeRotation(Rel);
	}
}

void AKodRTSCameraPawn::Pan(FVector2D AxisValue)
{
	if (AxisValue.IsNearlyZero())
	{
		return;
	}
	const FRotator YawRot(0.f, GetActorRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	AddActorWorldOffset((Forward * AxisValue.Y + Right * AxisValue.X) * PanSpeed * GetWorld()->GetDeltaSeconds());
}

void AKodRTSCameraPawn::Zoom(float AxisValue)
{
	if (!CameraBoom || FMath::IsNearlyZero(AxisValue))
	{
		return;
	}
	CameraBoom->TargetArmLength = FMath::Clamp(
		CameraBoom->TargetArmLength - AxisValue * ZoomSpeed,
		MinArmLength,
		MaxArmLength);
}

void AKodRTSCameraPawn::OrbitYaw(float AxisValue)
{
	if (!bAllowYawOrbit || FMath::IsNearlyZero(AxisValue))
	{
		return;
	}
	AddActorWorldRotation(FRotator(0.f, AxisValue * 60.f * GetWorld()->GetDeltaSeconds(), 0.f));
}
