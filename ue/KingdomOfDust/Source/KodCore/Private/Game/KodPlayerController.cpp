#include "Game/KodPlayerController.h"
#include "Game/KodRTSCameraPawn.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Sim/KodSimSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"

AKodPlayerController::AKodPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Default;
	bEnableClickEvents = true;
	PrimaryActorTick.bCanEverTick = true;
}

void AKodPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// PIE: GameAndUI + unlocked cursor so LMB/RMB reach PlayerTick (WasInputKeyJustPressed).
	// Viewport capture is CaptureDuringMouseDown (DefaultInput.ini).
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (UInputMappingContext* IMC = DefaultMappingContext.LoadSynchronous())
			{
				Subsystem->AddMappingContext(IMC, 0);
			}
		}
	}
}

void AKodPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Bind UInputAction assets (IA_Select / IA_CommandMove / IA_CommandAttack) via
	// UEnhancedInputComponent once Content/Core/Input assets exist.
	// Fallback: PlayerTick polls mouse buttons for Slice 0 greybox PIE.
}

void AKodPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Greybox mouse fallback until Enhanced Input assets exist.
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		HandleSelectStarted();
	}
	if (bSelectPressed)
	{
		float X = 0.f, Y = 0.f;
		GetMousePosition(X, Y);
		UpdateMarquee(FVector2D(X, Y));
	}
	if (WasInputKeyJustReleased(EKeys::LeftMouseButton))
	{
		HandleSelectCompleted();
	}
	if (WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		// Shift+RMB attack if hovering selectable enemy later; default move / attack smart
		AActor* HitActor = nullptr;
		FHitResult Hit;
		if (GetHitResultUnderCursor(ECC_Pawn, false, Hit) && Hit.GetActor())
		{
			HitActor = Hit.GetActor();
		}
		if (HitActor && HitActor != GetPawn())
		{
			// Prefer attack if target is a registered sim entity distinct from selection
			IssueAttackToSelection(HitActor);
		}
		else
		{
			HandleCommandMove();
		}
	}
}

void AKodPlayerController::PanCamera(FVector2D AxisValue)
{
	if (AKodRTSCameraPawn* Cam = Cast<AKodRTSCameraPawn>(GetPawn()))
	{
		Cam->Pan(AxisValue);
	}
}

void AKodPlayerController::ZoomCamera(float AxisValue)
{
	if (AKodRTSCameraPawn* Cam = Cast<AKodRTSCameraPawn>(GetPawn()))
	{
		Cam->Zoom(AxisValue);
	}
}

bool AKodPlayerController::GetGroundHitUnderCursor(FHitResult& OutHit) const
{
	return const_cast<AKodPlayerController*>(this)->GetHitResultUnderCursor(ECC_Visibility, true, OutHit);
}

TArray<AActor*> AKodPlayerController::GetLocalSelection() const
{
	TArray<AActor*> Actors;
	Actors.Reserve(LocalSelection.Num());
	for (const TWeakObjectPtr<AActor>& Ptr : LocalSelection)
	{
		if (AActor* Actor = Ptr.Get())
		{
			Actors.Add(Actor);
		}
	}
	return Actors;
}

TArray<FKodEntityId> AKodPlayerController::GetLocalSelectedEntityIds() const
{
	TArray<FKodEntityId> Ids;
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return Ids;
	}
	for (const TWeakObjectPtr<AActor>& Ptr : LocalSelection)
	{
		if (AActor* A = Ptr.Get())
		{
			FKodEntityId Id = Sim->FindIdForActor(A);
			if (Id.IsValid())
			{
				Ids.Add(Id);
			}
		}
	}
	return Ids;
}

void AKodPlayerController::HandleSelectStarted()
{
	float X = 0.f, Y = 0.f;
	GetMousePosition(X, Y);
	BeginMarquee(FVector2D(X, Y));
	bSelectPressed = true;
}

void AKodPlayerController::HandleSelectCompleted()
{
	bSelectPressed = false;
	const float Drag = FVector2D::Distance(MarqueeStart, MarqueeEnd);
	if (Drag >= BoxSelectDragThresholdPx)
	{
		EndMarquee(IsInputKeyDown(EKeys::LeftShift));
	}
	else
	{
		bMarqueeActive = false;
		ClickSelectAtCursor(IsInputKeyDown(EKeys::LeftShift));
	}
}

void AKodPlayerController::HandleCommandMove()
{
	FHitResult Hit;
	if (!GetGroundHitUnderCursor(Hit))
	{
		return;
	}
	IssueMoveToSelection(Hit.ImpactPoint);
}

void AKodPlayerController::HandleCommandAttack()
{
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Pawn, false, Hit) && Hit.GetActor())
	{
		IssueAttackToSelection(Hit.GetActor());
	}
}

void AKodPlayerController::IssueMoveToSelection_Implementation(FVector WorldLocation)
{
	// Default path: sim IssueMove (walk gate). Prefer UKodCommandSubsystem via Slice0 PC subclass.
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		Sim->IssueMove(Id, WorldLocation);
	}
}

void AKodPlayerController::IssueAttackToSelection_Implementation(AActor* Target)
{
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Target)
	{
		return;
	}
	const FKodEntityId TargetId = Sim->FindIdForActor(Target);
	if (!TargetId.IsValid())
	{
		// Not a sim entity — fall back to move-to location
		IssueMoveToSelection(Target->GetActorLocation());
		return;
	}
	for (const FKodEntityId& Id : GetLocalSelectedEntityIds())
	{
		if (Id != TargetId)
		{
			Sim->IssueAttack(Id, TargetId);
		}
	}
}

void AKodPlayerController::BeginMarquee(FVector2D ScreenPos)
{
	bMarqueeActive = true;
	MarqueeStart = ScreenPos;
	MarqueeEnd = ScreenPos;
}

void AKodPlayerController::UpdateMarquee(FVector2D ScreenPos)
{
	if (bMarqueeActive)
	{
		MarqueeEnd = ScreenPos;
	}
}

void AKodPlayerController::EndMarquee(bool bAddToSelection)
{
	bMarqueeActive = false;
	TArray<AActor*> Hits;
	CollectActorsInMarquee(Hits);
	if (!bAddToSelection)
	{
		LocalSelection.Reset();
	}
	for (AActor* A : Hits)
	{
		LocalSelection.AddUnique(A);
	}
}

void AKodPlayerController::ClickSelectAtCursor(bool bAddToSelection)
{
	FHitResult Hit;
	AActor* Picked = nullptr;
	if (GetHitResultUnderCursor(ECC_Pawn, false, Hit))
	{
		Picked = Hit.GetActor();
	}
	if (!Picked && GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		Picked = Hit.GetActor();
	}

	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (Picked && Sim && !Sim->FindIdForActor(Picked).IsValid())
	{
		Picked = nullptr; // only selectables registered in sim
	}

	if (!bAddToSelection)
	{
		LocalSelection.Reset();
	}
	if (Picked)
	{
		LocalSelection.AddUnique(Picked);
	}

	UE_LOG(LogTemp, Log, TEXT("ClickSelectAtCursor LocalSelection=%d"), LocalSelection.Num());
}

void AKodPlayerController::CollectActorsInMarquee(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	UWorld* World = GetWorld();
	UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!World || !Sim)
	{
		return;
	}

	const float MinX = FMath::Min(MarqueeStart.X, MarqueeEnd.X);
	const float MaxX = FMath::Max(MarqueeStart.X, MarqueeEnd.X);
	const float MinY = FMath::Min(MarqueeStart.Y, MarqueeEnd.Y);
	const float MaxY = FMath::Max(MarqueeStart.Y, MarqueeEnd.Y);

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Sim->FindIdForActor(Actor).IsValid())
		{
			continue;
		}
		FVector2D Screen;
		if (!ProjectWorldLocationToScreen(Actor->GetActorLocation(), Screen))
		{
			continue;
		}
		if (Screen.X >= MinX && Screen.X <= MaxX && Screen.Y >= MinY && Screen.Y <= MaxY)
		{
			OutActors.Add(Actor);
		}
	}
}
