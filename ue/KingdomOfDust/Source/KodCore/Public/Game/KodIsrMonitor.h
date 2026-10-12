#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KodIsrMonitor.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/** Downward picture-in-picture camera for an ISR satellite. No mesh, so the main view does not show it. */
UCLASS()
class KODCORE_API AKodIsrMonitor : public AActor
{
	GENERATED_BODY()

public:
	AKodIsrMonitor();

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }
	void SetMonitorVisible(bool bVisible);
	void AimAt(const FVector& Center, int32 SightUU);
	void CaptureView();

private:
	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	bool bMonitorVisible = false;
	float LastCaptureSeconds = -1.f;
};
