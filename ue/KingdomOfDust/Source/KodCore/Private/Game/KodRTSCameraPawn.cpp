#include "Game/KodRTSCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpringArmComponent.h"
#include "Game/KodPlayerState.h"
#include "InputCoreTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "UnrealClient.h"

AKodRTSCameraPawn::AKodRTSCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	bFindCameraComponentWhenViewTarget = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(SceneRoot);
	CameraBoom->TargetArmLength = DefaultArmLength;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->SetRelativeRotation(FRotator(PitchDegrees, 0.f, 0.f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	CurrentArmLength = DefaultArmLength;
	TargetArmLength = DefaultArmLength;
}

void AKodRTSCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	if (!bViewCopied)
	{
		SetActorRotation(FRotator(0.f, YawDegrees, 0.f));
		CurrentArmLength = FMath::Clamp(DefaultArmLength, MinArmLength, MaxArmLength);
		TargetArmLength = CurrentArmLength;
	}
	MaintainBoom();
	ApplyVerticalFov();

	if (WantsCommandCenterFraming())
	{
		FocusCommandCenter();
	}
	CachePlayableBounds();
	UpdateTerrain(0.f, true);
	ClampToPlayable();

	if (UWorld* World = GetWorld())
	{
		if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			Sim->OnUnitHit.AddDynamic(this, &AKodRTSCameraPawn::HandleUnitHit);
			Sim->OnTrainCompleted.AddDynamic(this, &AKodRTSCameraPawn::HandleTrainCompleted);
			bBoundSimEvents = true;
		}
	}

	if (!bLoggedInit)
	{
		bLoggedInit = true;
		const float Height = CurrentArmLength * FMath::Sin(FMath::DegreesToRadians(FMath::Abs(PitchDegrees)));
		UE_LOG(LogTemp, Log, TEXT("KodCam Init Pitch=%.0f FOV=%.0f Height=%.0f"),
			PitchDegrees,
			VerticalFovDegrees,
			Height);
	}
}

void AKodRTSCameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bBoundSimEvents)
	{
		if (UWorld* World = GetWorld())
		{
			if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
			{
				Sim->OnUnitHit.RemoveDynamic(this, &AKodRTSCameraPawn::HandleUnitHit);
				Sim->OnTrainCompleted.RemoveDynamic(this, &AKodRTSCameraPawn::HandleTrainCompleted);
			}
		}
		bBoundSimEvents = false;
	}
	Super::EndPlay(EndPlayReason);
}

void AKodRTSCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Max(0.f, DeltaSeconds);

	ApplyVerticalFov();
	MaintainBoom();

	bool bManualPan = false;
	PollMoveInput(Dt, bManualPan);
	PollZoom();
	PollBookmarks();
	PollAlert();
	PollRotate();
	if (bManualPan)
	{
		bFollowUnit = false;
		FollowTarget = nullptr;
	}
	PollFollow();
	if (bFollowUnit)
	{
		UpdateFollow(Dt);
	}

	CurrentArmLength = FMath::FInterpTo(CurrentArmLength, TargetArmLength, Dt, ZoomInterpSpeed);
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = CurrentArmLength;
	}

	UpdateTerrain(Dt, false);
	ClampToPlayable();
}

void AKodRTSCameraPawn::Pan(FVector2D AxisValue)
{
	if (AxisValue.IsNearlyZero())
	{
		return;
	}
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	AddActorWorldOffset((PlanarForward() * AxisValue.Y + PlanarRight() * AxisValue.X) * ScrollSpeed * Dt);
}

void AKodRTSCameraPawn::Zoom(float AxisValue)
{
	if (FMath::IsNearlyZero(AxisValue))
	{
		return;
	}
	TargetArmLength = FMath::Clamp(TargetArmLength - AxisValue * ZoomStep, MinArmLength, MaxArmLength);
}

void AKodRTSCameraPawn::OrbitYaw(float AxisValue)
{
	if (!bAllowAltDragRotate || FMath::IsNearlyZero(AxisValue))
	{
		return;
	}
	AddActorWorldRotation(FRotator(0.f, AxisValue, 0.f));
}

void AKodRTSCameraPawn::JumpTo(FVector WorldLocation)
{
	FVector Loc = GetActorLocation();
	Loc.X = WorldLocation.X;
	Loc.Y = WorldLocation.Y;
	SetActorLocation(Loc);
	PanVelocity = FVector::ZeroVector;
	bGrabbing = false;
	bFollowUnit = false;
	FollowTarget = nullptr;
	UpdateTerrain(0.f, true);
	ClampToPlayable();
}

void AKodRTSCameraPawn::NoteAlert(FVector WorldLocation)
{
	AlertLocation = WorldLocation;
	bHasAlert = true;
}

void AKodRTSCameraPawn::CopyViewFrom(const AKodRTSCameraPawn* Other)
{
	if (!Other)
	{
		return;
	}
	SetActorLocation(Other->GetActorLocation());
	SetActorRotation(FRotator(0.f, Other->GetActorRotation().Yaw, 0.f));
	TargetArmLength = FMath::Clamp(Other->GetCurrentArmLength(), MinArmLength, MaxArmLength);
	CurrentArmLength = TargetArmLength;
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = CurrentArmLength;
	}
	PanVelocity = FVector::ZeroVector;
	bViewCopied = true;
}

bool AKodRTSCameraPawn::ConsumesAltDrag() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !bAllowAltDragRotate)
	{
		return false;
	}
	return PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt);
}

void AKodRTSCameraPawn::HandleUnitHit(AActor* Attacker, AActor* Target, float Health, int32 TargetId)
{
	(void)Attacker;
	(void)Health;
	(void)TargetId;
	if (Target)
	{
		NoteAlert(Target->GetActorLocation());
	}
}

void AKodRTSCameraPawn::HandleTrainCompleted(FVector Location, int32 TeamId)
{
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (const AKodPlayerState* PS = PC->GetPlayerState<AKodPlayerState>())
		{
			if (PS->TeamId != TeamId)
			{
				return;
			}
		}
	}
	NoteAlert(Location);
}

void AKodRTSCameraPawn::ApplyVerticalFov()
{
	if (!Camera)
	{
		return;
	}
	float Aspect = 16.f / 9.f;
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		int32 SizeX = 0;
		int32 SizeY = 0;
		PC->GetViewportSize(SizeX, SizeY);
		if (SizeX > 0 && SizeY > 0)
		{
			Aspect = static_cast<float>(SizeX) / static_cast<float>(SizeY);
		}
	}
	const float HalfVertical = FMath::DegreesToRadians(FMath::Clamp(VerticalFovDegrees, 10.f, 80.f) * 0.5f);
	const float HalfHorizontal = FMath::Atan(FMath::Tan(HalfVertical) * Aspect);
	Camera->SetFieldOfView(FMath::RadiansToDegrees(HalfHorizontal * 2.f));
}

void AKodRTSCameraPawn::MaintainBoom()
{
	if (!CameraBoom)
	{
		return;
	}
	CameraBoom->SetRelativeRotation(FRotator(PitchDegrees, 0.f, 0.f));
	CameraBoom->bEnableCameraLag = false;
}

void AKodRTSCameraPawn::FocusCommandCenter()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AActor* Best = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == this)
		{
			continue;
		}
		const FString Label = Actor->GetActorNameOrLabel();
		const bool bCc = Label.Contains(TEXT("PROXY_CC"))
			|| Actor->ActorHasTag(FName(TEXT("KodBuildingCommandCenter")));
		if (!bCc)
		{
			continue;
		}
		if (!Best || Actor->GetName() < Best->GetName())
		{
			Best = Actor;
		}
	}
	if (!Best)
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Best = *It;
			break;
		}
	}
	if (!Best)
	{
		return;
	}
	FVector Loc = GetActorLocation();
	Loc.X = Best->GetActorLocation().X;
	Loc.Y = Best->GetActorLocation().Y;
	SetActorLocation(Loc);
	SetActorRotation(FRotator(0.f, YawDegrees, 0.f));
}

void AKodRTSCameraPawn::PollMoveInput(float DeltaSeconds, bool& bOutManualPan)
{
	bOutManualPan = false;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		PanVelocity = FMath::VInterpTo(PanVelocity, FVector::ZeroVector, DeltaSeconds, PanAccelSpeed);
		if (!PanVelocity.IsNearlyZero())
		{
			AddActorWorldOffset(FVector(PanVelocity.X, PanVelocity.Y, 0.f) * DeltaSeconds);
		}
		return;
	}

	const bool bAlt = PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt);
	const bool bRotateDrag = bAllowAltDragRotate && bAlt
		&& (PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::MiddleMouseButton));

	if (!bRotateDrag && PC->WasInputKeyJustPressed(EKeys::MiddleMouseButton))
	{
		FVector Hit;
		if (GroundUnderCursor(Hit))
		{
			bGrabbing = true;
			GrabWorld = Hit;
		}
	}
	if (PC->WasInputKeyJustReleased(EKeys::MiddleMouseButton) || bRotateDrag)
	{
		bGrabbing = false;
	}
	if (bGrabbing)
	{
		FVector CursorGround;
		if (CursorOnPlane(GrabWorld.Z, CursorGround))
		{
			AddActorWorldOffset(FVector(GrabWorld.X - CursorGround.X, GrabWorld.Y - CursorGround.Y, 0.f));
		}
		PanVelocity = FVector::ZeroVector;
		bOutManualPan = true;
		return;
	}

	FVector2D Input = FVector2D::ZeroVector;
	if (PC->IsInputKeyDown(EKeys::Up))
	{
		Input.Y += 1.f;
	}
	if (PC->IsInputKeyDown(EKeys::Down))
	{
		Input.Y -= 1.f;
	}
	if (PC->IsInputKeyDown(EKeys::Right))
	{
		Input.X += 1.f;
	}
	if (PC->IsInputKeyDown(EKeys::Left))
	{
		Input.X -= 1.f;
	}

	if (IsViewportFocused())
	{
		float MouseX = 0.f;
		float MouseY = 0.f;
		int32 SizeX = 0;
		int32 SizeY = 0;
		PC->GetViewportSize(SizeX, SizeY);
		if (SizeX > 0 && SizeY > 0 && PC->GetMousePosition(MouseX, MouseY))
		{
			const float Margin = FMath::Max(0.f, EdgeScrollMarginPx);
			if (MouseX <= Margin)
			{
				Input.X -= 1.f;
			}
			else if (MouseX >= static_cast<float>(SizeX - 1) - Margin)
			{
				Input.X += 1.f;
			}
			if (MouseY <= Margin)
			{
				Input.Y += 1.f;
			}
			else if (MouseY >= static_cast<float>(SizeY - 1) - Margin)
			{
				Input.Y -= 1.f;
			}
		}
	}

	if (Input.SizeSquared() > 1.f)
	{
		Input.Normalize();
	}
	if (!Input.IsNearlyZero())
	{
		bOutManualPan = true;
	}
	ApplyPan(DeltaSeconds, Input);
}

void AKodRTSCameraPawn::ApplyPan(float DeltaSeconds, const FVector2D& PlanarInput)
{
	const FVector Desired = (PlanarRight() * PlanarInput.X + PlanarForward() * PlanarInput.Y) * ScrollSpeed;
	PanVelocity = FMath::VInterpTo(PanVelocity, Desired, DeltaSeconds, PanAccelSpeed);
	if (!PanVelocity.IsNearlyZero())
	{
		AddActorWorldOffset(FVector(PanVelocity.X, PanVelocity.Y, 0.f) * DeltaSeconds);
	}
}

void AKodRTSCameraPawn::PollZoom()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	float Wheel = PC->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
	if (FMath::IsNearlyZero(Wheel))
	{
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
		{
			Wheel = 1.f;
		}
		else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
		{
			Wheel = -1.f;
		}
	}
	if (!FMath::IsNearlyZero(Wheel))
	{
		TargetArmLength = FMath::Clamp(TargetArmLength - Wheel * ZoomStep, MinArmLength, MaxArmLength);
	}
}

void AKodRTSCameraPawn::PollBookmarks()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	const FKey Keys[4] = { EKeys::F5, EKeys::F6, EKeys::F7, EKeys::F8 };
	const bool bCtrl = PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		if (!PC->WasInputKeyJustPressed(Keys[Index]))
		{
			continue;
		}
		const int32 SlotNumber = 5 + Index;
		if (bCtrl)
		{
			SaveBookmark(SlotNumber);
		}
		else
		{
			RecallBookmark(SlotNumber);
		}
	}
}

void AKodRTSCameraPawn::SaveBookmark(int32 SlotNumber)
{
	const int32 Index = SlotNumber - 5;
	if (Index < 0 || Index >= 4)
	{
		return;
	}
	FKodCameraBookmark& Slot = Bookmarks[Index];
	Slot.bSet = true;
	Slot.Location = GetActorLocation();
	Slot.Yaw = GetActorRotation().Yaw;
	Slot.ArmLength = CurrentArmLength;
	UE_LOG(LogTemp, Log, TEXT("KodCam Save Slot=%d"), SlotNumber);
}

void AKodRTSCameraPawn::RecallBookmark(int32 SlotNumber)
{
	const int32 Index = SlotNumber - 5;
	if (Index < 0 || Index >= 4 || !Bookmarks[Index].bSet)
	{
		return;
	}
	const FKodCameraBookmark& Slot = Bookmarks[Index];
	SetActorLocation(Slot.Location);
	SetActorRotation(FRotator(0.f, Slot.Yaw, 0.f));
	TargetArmLength = FMath::Clamp(Slot.ArmLength, MinArmLength, MaxArmLength);
	CurrentArmLength = TargetArmLength;
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = CurrentArmLength;
	}
	PanVelocity = FVector::ZeroVector;
	bGrabbing = false;
	bFollowUnit = false;
	FollowTarget = nullptr;
	ClampToPlayable();
	UE_LOG(LogTemp, Log, TEXT("KodCam Recall Slot=%d"), SlotNumber);
}

void AKodRTSCameraPawn::PollAlert()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !bHasAlert)
	{
		return;
	}
	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		JumpTo(AlertLocation);
	}
}

void AKodRTSCameraPawn::PollRotate()
{
	if (!bAllowAltDragRotate)
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	const bool bAlt = PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt);
	const bool bDrag = PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::MiddleMouseButton);
	if (!bAlt || !bDrag)
	{
		return;
	}
	float DeltaX = 0.f;
	float DeltaY = 0.f;
	PC->GetInputMouseDelta(DeltaX, DeltaY);
	if (!FMath::IsNearlyZero(DeltaX))
	{
		AddActorWorldRotation(FRotator(0.f, DeltaX * RotateDegreesPerPixel, 0.f));
	}
}

void AKodRTSCameraPawn::PollFollow()
{
	if (!SupportsUnitFollow())
	{
		return;
	}
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->WasInputKeyJustPressed(EKeys::F))
	{
		return;
	}
	if (bFollowUnit)
	{
		bFollowUnit = false;
		FollowTarget = nullptr;
		return;
	}
	if (AActor* Target = FindFollowTarget())
	{
		bFollowUnit = true;
		FollowTarget = Target;
	}
}

void AKodRTSCameraPawn::UpdateFollow(float DeltaSeconds)
{
	AActor* Target = FollowTarget.Get();
	if (!Target)
	{
		bFollowUnit = false;
		return;
	}
	const FVector Desired = Target->GetActorLocation();
	const FVector Current = GetActorLocation();
	const FVector Next = FMath::VInterpTo(Current, FVector(Desired.X, Desired.Y, Current.Z), DeltaSeconds, FollowInterpSpeed);
	SetActorLocation(FVector(Next.X, Next.Y, Current.Z));
}

void AKodRTSCameraPawn::UpdateTerrain(float DeltaSeconds, bool bInstant)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Loc = GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KodCamGround), false);
	Params.AddIgnoredActor(this);
	const FVector Start(Loc.X, Loc.Y, Loc.Z + 8000.f);
	const FVector End(Loc.X, Loc.Y, Loc.Z - 16000.f);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		TargetGroundZ = Hit.ImpactPoint.Z;
		bHaveGround = true;
	}
	if (!bHaveGround)
	{
		return;
	}
	FVector Moved = Loc;
	Moved.Z = bInstant ? TargetGroundZ : FMath::FInterpTo(Loc.Z, TargetGroundZ, DeltaSeconds, TerrainInterpSpeed);
	SetActorLocation(Moved);
}

void AKodRTSCameraPawn::CachePlayableBounds()
{
	bPlayableCached = false;
	CachedPlayable = FBox(ForceInit);
	if (bUsePlayableBox && PlayableBox.IsValid)
	{
		CachedPlayable = PlayableBox;
		bPlayableCached = true;
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FBox Combined(ForceInit);
	bool bAny = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		const FString Label = Actor->GetActorNameOrLabel();
		const bool bGround = Actor->ActorHasTag(FName(TEXT("KodGround")))
			|| Label.Contains(TEXT("PROXY_GROUND"));
		if (!bGround)
		{
			continue;
		}
		const FBox Bounds = Actor->GetComponentsBoundingBox(true);
		if (!Bounds.IsValid)
		{
			continue;
		}
		Combined += Bounds;
		bAny = true;
	}
	if (bAny)
	{
		CachedPlayable = Combined;
		bPlayableCached = true;
	}
}

void AKodRTSCameraPawn::ClampToPlayable()
{
	if (!bPlayableCached || !CachedPlayable.IsValid)
	{
		CachePlayableBounds();
	}
	if (!bPlayableCached || !CachedPlayable.IsValid)
	{
		return;
	}
	const float Inset = FMath::Max(0.f, BoundsInsetUU);
	const float MinX = CachedPlayable.Min.X + Inset;
	const float MaxX = CachedPlayable.Max.X - Inset;
	const float MinY = CachedPlayable.Min.Y + Inset;
	const float MaxY = CachedPlayable.Max.Y - Inset;
	if (MinX > MaxX || MinY > MaxY)
	{
		return;
	}
	FVector Loc = GetActorLocation();
	Loc.X = FMath::Clamp(Loc.X, MinX, MaxX);
	Loc.Y = FMath::Clamp(Loc.Y, MinY, MaxY);
	SetActorLocation(Loc);
}

bool AKodRTSCameraPawn::IsViewportFocused() const
{
	const UWorld* World = GetWorld();
	UGameViewportClient* ViewportClient = World ? World->GetGameViewport() : nullptr;
	FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
	if (!Viewport)
	{
		return false;
	}
	if (Viewport->HasFocus() || Viewport->HasMouseCapture())
	{
		return true;
	}
	return !GIsEditor && Viewport->IsForegroundWindow();
}

bool AKodRTSCameraPawn::GroundUnderCursor(FVector& OutWorld) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return false;
	}
	FHitResult Hit;
	if (!const_cast<APlayerController*>(PC)->GetHitResultUnderCursor(ECC_Visibility, true, Hit))
	{
		return CursorOnPlane(GetActorLocation().Z, OutWorld);
	}
	OutWorld = Hit.ImpactPoint;
	return true;
}

bool AKodRTSCameraPawn::CursorOnPlane(float PlaneZ, FVector& OutWorld) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return false;
	}
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	if (!PC->DeprojectMousePositionToWorld(Origin, Direction) || FMath::IsNearlyZero(Direction.Z))
	{
		return false;
	}
	const float Distance = (PlaneZ - Origin.Z) / Direction.Z;
	if (Distance < 0.f)
	{
		return false;
	}
	OutWorld = Origin + Direction * Distance;
	return true;
}

FVector AKodRTSCameraPawn::PlanarForward() const
{
	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}
	return Forward;
}

FVector AKodRTSCameraPawn::PlanarRight() const
{
	FVector Right = GetActorRightVector();
	Right.Z = 0.f;
	if (!Right.Normalize())
	{
		Right = FVector::RightVector;
	}
	return Right;
}
