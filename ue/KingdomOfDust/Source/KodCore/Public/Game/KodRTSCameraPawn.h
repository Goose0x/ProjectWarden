#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "KodRTSCameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USceneComponent;

/**
 * Isometric / RTS boom camera for L_Slice0.
 * Fixed pitch, optional yaw orbit, zoom via arm length. No possession of units.
 */
UCLASS(Blueprintable)
class KODCORE_API AKodRTSCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AKodRTSCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Kod|Camera")
	void Pan(FVector2D AxisValue);

	UFUNCTION(BlueprintCallable, Category = "Kod|Camera")
	void Zoom(float AxisValue);

	UFUNCTION(BlueprintCallable, Category = "Kod|Camera")
	void OrbitYaw(float AxisValue);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	float PanSpeed = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	float ZoomSpeed = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	float MinArmLength = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	float MaxArmLength = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	float FixedPitchDegrees = -55.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	bool bAllowYawOrbit = true;
};
