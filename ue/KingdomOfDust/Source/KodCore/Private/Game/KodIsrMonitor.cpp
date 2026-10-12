#include "Game/KodIsrMonitor.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

AKodIsrMonitor::AKodIsrMonitor()
{
	PrimaryActorTick.bCanEverTick = false;
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	SetRootComponent(Capture);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->FOVAngle = 35.f;
}

void AKodIsrMonitor::SetMonitorVisible(bool bVisible)
{
	bMonitorVisible = bVisible;
}

void AKodIsrMonitor::AimAt(const FVector& Center, int32 SightUU)
{
	if (!RenderTarget)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		RenderTarget->ClearColor = FLinearColor(0.02f, 0.03f, 0.02f, 1.f);
		RenderTarget->InitCustomFormat(256, 192, PF_B8G8R8A8, false);
		RenderTarget->UpdateResourceImmediate(true);
		if (Capture)
		{
			Capture->TextureTarget = RenderTarget;
		}
	}
	const float Height = FMath::Clamp(static_cast<float>(FMath::Max(400, SightUU)) * 1.4f, 1200.f, 3200.f);
	SetActorLocation(FVector(Center.X, Center.Y, Height));
	SetActorRotation(FRotator(-90.f, 0.f, 0.f));
}

void AKodIsrMonitor::CaptureView()
{
	if (!bMonitorVisible || !Capture || !RenderTarget)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (LastCaptureSeconds >= 0.f && Now - LastCaptureSeconds < (1.f / 12.f))
	{
		return;
	}
	LastCaptureSeconds = Now;
	Capture->CaptureScene();
}
