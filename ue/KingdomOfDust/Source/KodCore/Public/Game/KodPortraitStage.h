#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KodPortraitStage.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class USkeletalMesh;
class UStaticMesh;

/** Off-screen capture of the selected unit. Created at runtime. Not a sim entity. */
UCLASS()
class KODCORE_API AKodPortraitStage : public AActor
{
	GENERATED_BODY()

public:
	AKodPortraitStage();

	virtual void Tick(float DeltaSeconds) override;

	/** Frames the head and upper torso. PortraitMesh wins over the body mesh. */
	void ShowSubject(AActor* Subject, int32 TeamId);

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

private:
	void EnsureCapture();

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> SkelMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> StaticMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Backdrop;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> RimLight;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BackdropMid;

	float Sway = 0.f;
	bool bSway = false;
	int32 ShownTeam = 0;
};
