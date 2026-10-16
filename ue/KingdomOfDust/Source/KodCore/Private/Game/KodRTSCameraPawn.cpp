#include "Game/KodRTSCameraPawn.h"

#include "Game/KodCameraSettings.h"
#include "Camera/CameraTypes.h"
#include "Math/PerspectiveMatrix.h"
#include "SceneView.h"
#include "SceneViewExtension.h"

#include "Camera/CameraComponent.h"
#include "Math/RotationMatrix.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SpringArmComponent.h"
#include "Game/KodHUD.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "Game/KodUserSettings.h"
#include "InputCoreTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Warden/KodWardenPaths.h"
#include "Sim/KodCheatRows.h"
#include "Game/KodSpectatorPawn.h"
#include "Warden/KodWardenPaths.h"
#include "UnrealClient.h"

namespace
{
	bool GKodApplyGameClip = false;

	/**
	 * Reversed-Z perspective is infinite unless a finite far was written.
	 * A finite far shorter than the converted SC2 far (150 * 200 = 30000 uu)
	 * clips the map. Rewrite only that short plane, using the engine matrix
	 * so the Z row matches this Unreal version.
	 */
	class FKodClipPlaneExtension : public FSceneViewExtensionBase
	{
	public:
		FKodClipPlaneExtension(const FAutoRegister& AutoReg)
			: FSceneViewExtensionBase(AutoReg)
		{
		}

		virtual void SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData) override
		{
			if (!GKodApplyGameClip)
			{
				return;
			}
			FMatrix& Projection = InOutProjectionData.ProjectionMatrix;
			const bool bPerspective = FMath::IsNearlyEqual(Projection.M[2][3], 1.f, 0.01f)
				&& FMath::IsNearlyZero(Projection.M[3][3], 0.01f);
			if (!bPerspective)
			{
				return;
			}
			const FKodCameraTunables Tunables = FKodCameraTunables::Read();
			float NearUU = 0.f;
			float FarUU = 0.f;
			KodCameraSettings::ResolveClipUU(Tunables.NearSc2, Tunables.FarSc2, Tunables.UnitScaleUU, NearUU, FarUU);
			if (FarUU < 1000.f)
			{
				return;
			}
			const float ZScale = Projection.M[2][2];
			// UE5 infinite reversed-Z uses a Z scale of ~0. The older form sits at ~1.
			if (FMath::Abs(ZScale) < 0.01f || ZScale <= 1.001f)
			{
				return;
			}
			const float ImpliedFar = (ZScale * NearUU) / (ZScale - 1.f);
			if (ImpliedFar >= FarUU * 0.5f)
			{
				return;
			}
			const FMatrix Finite = FReversedZPerspectiveMatrix(
				FMath::DegreesToRadians(45.f),
				FMath::DegreesToRadians(45.f),
				1.f,
				1.f,
				NearUU,
				FarUU);
			Projection.M[2][2] = Finite.M[2][2];
			Projection.M[3][2] = Finite.M[3][2];
			Projection.M[2][3] = Finite.M[2][3];
			Projection.M[3][3] = Finite.M[3][3];
			static bool bLoggedReplace = false;
			if (!bLoggedReplace)
			{
				bLoggedReplace = true;
				UE_LOG(LogTemp, Log, TEXT("KodClip FarReplaced ImpliedUU=%.0f NearUU=%.1f FarUU=%.0f"), ImpliedFar, NearUU, FarUU);
			}
		}
	};

	TSharedPtr<FKodClipPlaneExtension, ESPMode::ThreadSafe> GKodClipExtension;
}

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
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->TargetOffset = FVector::ZeroVector;
	CameraBoom->SocketOffset = FVector::ZeroVector;
	CameraBoom->SetRelativeLocation(FVector::ZeroVector);
	CameraBoom->SetRelativeRotation(FRotator(PitchDegrees, 0.f, RollDegrees));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->SetRelativeLocation(FVector::ZeroVector);
	Camera->SetRelativeRotation(FRotator::ZeroRotator);

	CurrentArmLength = DefaultArmLength;
	TargetArmLength = DefaultArmLength;
}

void AKodRTSCameraPawn::BeginPlay()
{
	Super::BeginPlay();
	if (const UWorld* World = GetWorld())
	{
		if (World->IsGameWorld())
		{
			GKodApplyGameClip = true;
			if (!GKodClipExtension.IsValid())
			{
				GKodClipExtension = FSceneViewExtensions::NewExtension<FKodClipPlaneExtension>();
			}
		}
	}
	ApplyTunables();

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
		SnapToPlayerStart();
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
		const FKodCameraTunables Tunables = FKodCameraTunables::Read();
		UE_LOG(LogTemp, Log, TEXT("KodCam Init Pitch=%.0f Yaw=%.0f Roll=%.0f FOV=%.0f Height=%.0f %s"),
			PitchDegrees,
			YawDegrees,
			RollDegrees,
			VerticalFovDegrees,
			Height,
			*Tunables.DescribeScale());
	}
}

void AKodRTSCameraPawn::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);
	// Near and far cvars are SC2 units. 150 uu would hide everything past 1.5 m.
	if (ActiveNearClipUU > 0.f && ActiveFarClipUU > ActiveNearClipUU)
	{
		OutResult.PerspectiveNearClipPlane = ActiveNearClipUU;
	}
}

void AKodRTSCameraPawn::ApplyTunables()
{
	const FKodCameraTunables Tunables = FKodCameraTunables::Read();
	PitchDegrees = Tunables.Pitch;
	VerticalFovDegrees = Tunables.Fov;
	RollDegrees = Tunables.Roll;
	ZoomReturnSpeed = FMath::Max(0.f, Tunables.ZoomReturnSpeed);
	ZoomStep = KodCameraSettings::ToUU(FMath::Max(0.f, Tunables.ZoomStepSc2), Tunables.UnitScaleUU);
	TargetZOffsetUU = KodCameraSettings::ToUU(Tunables.TargetZSc2, Tunables.UnitScaleUU);
	KodCameraSettings::ResolveClipUU(
		Tunables.NearSc2,
		Tunables.FarSc2,
		Tunables.UnitScaleUU,
		ActiveNearClipUU,
		ActiveFarClipUU);
	DefaultArmLength = KodCameraSettings::ToUU(Tunables.DistanceSc2, Tunables.UnitScaleUU);
	const bool bSpectator = IsA(AKodSpectatorPawn::StaticClass());
	if (!bSpectator)
	{
		YawDegrees = Tunables.Yaw;
		MinArmLength = KodCameraSettings::ToUU(Tunables.DistanceMinSc2, Tunables.UnitScaleUU);
		MaxArmLength = bShotArmHeld ? FMath::Max(DefaultArmLength, ShotArmLength) : DefaultArmLength;
		if (!bAllowAltDragRotate)
		{
			SetActorRotation(FRotator(0.f, YawDegrees, 0.f));
		}
	}
	if (!FMath::IsNearlyEqual(LoggedUnitScale, Tunables.UnitScaleUU))
	{
		LoggedUnitScale = Tunables.UnitScaleUU;
		UE_LOG(LogTemp, Log, TEXT("KodCam %s NearUU=%.1f FarUU=%.0f"),
			*Tunables.DescribeScale(),
			ActiveNearClipUU,
			ActiveFarClipUU);
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

void AKodRTSCameraPawn::AddShakeAt(const FVector& WorldPoint, float Amplitude)
{
	const UKodUserSettings* Settings = UKodUserSettings::Get();
	if (!Settings || !Settings->bCameraShake || Amplitude <= 0.f)
	{
		return;
	}
	const float Dist = FVector::Dist2D(GetActorLocation(), WorldPoint);
	if (Dist > 6000.f)
	{
		return;
	}
	const float Falloff = 1.f - FMath::Clamp(Dist / 6000.f, 0.f, 1.f);
	ShakeAmp = FMath::Max(ShakeAmp, Amplitude * FMath::Max(0.25f, Falloff));
}

void AKodRTSCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!ShakeApplied.IsNearlyZero())
	{
		AddActorWorldOffset(-ShakeApplied, false, nullptr, ETeleportType::TeleportPhysics);
		ShakeApplied = FVector::ZeroVector;
	}
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	ApplyTunables();

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
		bPendingPlayerFocus = false;
	}
	if (bPendingPlayerFocus)
	{
		SnapToPlayerStart();
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
	if (ShakeAmp > 0.5f)
	{
		const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		ShakeApplied = FVector(FMath::Sin(Time * 48.f), FMath::Cos(Time * 37.f), 0.f) * ShakeAmp;
		AddActorWorldOffset(ShakeApplied, false, nullptr, ETeleportType::TeleportPhysics);
		ShakeAmp = FMath::FInterpTo(ShakeAmp, 0.f, Dt, 6.f);
	}
	else
	{
		ShakeAmp = 0.f;
	}
}

void AKodRTSCameraPawn::Pan(FVector2D AxisValue)
{
	if (AxisValue.IsNearlyZero())
	{
		return;
	}
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	AddActorWorldOffset((PlanarForward() * AxisValue.Y + PlanarRight() * AxisValue.X) * KeyboardScrollSpeed * Dt);
}

void AKodRTSCameraPawn::Zoom(float AxisValue)
{
	if (FMath::IsNearlyZero(AxisValue))
	{
		return;
	}
	const float Ceiling = ZoomCeiling();
	TargetArmLength = FMath::Clamp(TargetArmLength - AxisValue * ZoomStep, MinArmLength, Ceiling);
	NoteZoomTarget(TEXT("Wheel"));
}

void AKodRTSCameraPawn::CommitZoomSteps(float Steps)
{
	Zoom(Steps);
	CurrentArmLength = TargetArmLength;
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = CurrentArmLength;
	}
}

void AKodRTSCameraPawn::OrbitYaw(float AxisValue)
{
	if (!bAllowAltDragRotate || FMath::IsNearlyZero(AxisValue))
	{
		return;
	}
	AddActorWorldRotation(FRotator(0.f, AxisValue, 0.f));
}

void AKodRTSCameraPawn::SnapToStartView(FVector WorldLocation)
{
	const FKodCameraTunables Tunables = FKodCameraTunables::Read();
	YawDegrees = Tunables.Yaw;
	PitchDegrees = Tunables.Pitch;
	VerticalFovDegrees = Tunables.Fov;
	DefaultArmLength = KodCameraSettings::ToUU(Tunables.DistanceSc2, Tunables.UnitScaleUU);
	MinArmLength = KodCameraSettings::ToUU(Tunables.DistanceMinSc2, Tunables.UnitScaleUU);
	if (MinArmLength > DefaultArmLength)
	{
		MinArmLength = DefaultArmLength;
	}
	MaxArmLength = DefaultArmLength;
	CurrentArmLength = DefaultArmLength;
	TargetArmLength = DefaultArmLength;
	SetActorRotation(FRotator(0.f, YawDegrees, 0.f));
	MaintainBoom();
	ApplyVerticalFov();
	JumpTo(WorldLocation);
	LoggedZoomArm = TargetArmLength;
	bStartViewSnapped = true;
	StartViewFrame = GFrameCounter;
}

bool AKodRTSCameraPawn::HasStartViewSettled() const
{
	return bStartViewSnapped && GFrameCounter > StartViewFrame;
}

void AKodRTSCameraPawn::LogOpeningCamera()
{
	if (bLoggedCamStart)
	{
		return;
	}
	bLoggedCamStart = true;
	const AActor* Anchor = FindCommandCenter();
	const FVector Centre = GetActorLocation();
	const FVector CcLoc = Anchor ? Anchor->GetActorLocation() : FVector::ZeroVector;
	UE_LOG(LogTemp, Log, TEXT("KodCam Start Centre=%.0f,%.0f,%.0f CC=%s Loc=%.0f,%.0f,%.0f Arm=%.0f"),
		Centre.X,
		Centre.Y,
		Centre.Z,
		Anchor ? *Anchor->GetName() : TEXT("none"),
		CcLoc.X,
		CcLoc.Y,
		CcLoc.Z,
		CurrentArmLength);
}

void AKodRTSCameraPawn::NoteZoomTarget(const TCHAR* Reason)
{
	if (FMath::IsNearlyEqual(LoggedZoomArm, TargetArmLength, 0.5f))
	{
		return;
	}
	UE_LOG(LogTemp, Verbose, TEXT("KodCam Zoom %s Arm=%.0f"), Reason ? Reason : TEXT("Change"), TargetArmLength);
	LoggedZoomArm = TargetArmLength;
}

AActor* AKodRTSCameraPawn::FindCommandCenter() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
	{
		TArray<FKodStructureState> Structures;
		Sim->CopyStructures(Structures);
		for (const FKodStructureState& Structure : Structures)
		{
			if (Structure.TeamId != 0 || Structure.DefinitionId != FName(KodWardenPaths::Id_CommandCenter))
			{
				continue;
			}
			if (AActor* Actor = Sim->ResolveEntity(Structure.Id))
			{
				return Actor;
			}
		}
	}
	AActor* Best = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == this)
		{
			continue;
		}
		const bool bCc = Actor->ActorHasTag(FName(TEXT("KodBuildingCommandCenter")))
			|| Actor->ActorHasTag(FName(TEXT("P1_SPAWN")))
			|| Actor->GetActorNameOrLabel().Contains(TEXT("PROXY_CC"));
		if (!bCc)
		{
			continue;
		}
		if (!Best || Actor->GetName() < Best->GetName())
		{
			Best = Actor;
		}
	}
	return Best;
}

void AKodRTSCameraPawn::SnapToPlayerStart()
{
	if (!WantsCommandCenterFraming())
	{
		return;
	}
	AActor* Anchor = FindCommandCenter();
	if (!Anchor)
	{
		return;
	}
	const FVector At = Anchor->GetActorLocation();
	if (FMath::Abs(At.X) < 50.f && FMath::Abs(At.Y) < 50.f)
	{
		return;
	}
	SnapToStartView(At);
	bPendingPlayerFocus = false;
	UE_LOG(LogTemp, Log, TEXT("KodCam Focus CC Loc=%.0f,%.0f,%.0f"), At.X, At.Y, GetActorLocation().Z);
}

void AKodRTSCameraPawn::JumpTo(FVector WorldLocation)
{
	bPendingPlayerFocus = false;
	FVector Loc = GetActorLocation();
	Loc.X = WorldLocation.X;
	Loc.Y = WorldLocation.Y;
	if (bHaveGround)
	{
		Loc.Z = TargetGroundZ;
	}
	SetActorLocation(Loc);
	PanVelocity = FVector::ZeroVector;
	bGrabbing = false;
	bFollowUnit = false;
	FollowTarget = nullptr;
	UpdateTerrain(0.f, true);
	ClampToPlayable();
	MaintainBoom();
	UE_LOG(LogTemp, Log, TEXT("KodCam Focus X=%.0f Y=%.0f Z=%.0f"), Loc.X, Loc.Y, GetActorLocation().Z);
}

void AKodRTSCameraPawn::FrameGroundSpan(FVector WorldLocation, float SpanXcm, float SpanYcm)
{
	JumpTo(WorldLocation);
	const float VFov = FMath::DegreesToRadians(FMath::Max(1.f, VerticalFovDegrees));
	float Aspect = 16.f / 9.f;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D ViewSize = FVector2D::ZeroVector;
		GEngine->GameViewport->GetViewportSize(ViewSize);
		if (ViewSize.Y > 1.f)
		{
			Aspect = ViewSize.X / ViewSize.Y;
		}
	}
	const float HFov = 2.f * FMath::Atan(FMath::Tan(VFov * 0.5f) * Aspect);
	const float Pitch = FMath::DegreesToRadians(FMath::Abs(PitchDegrees));
	const float Yaw = FMath::DegreesToRadians(YawDegrees);
	const FVector Forward(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f);
	const FVector Right(-FMath::Sin(Yaw), FMath::Cos(Yaw), 0.f);
	const float Across = FMath::Abs(Right.X) * SpanXcm + FMath::Abs(Right.Y) * SpanYcm;
	const float Along = FMath::Abs(Forward.X) * SpanXcm + FMath::Abs(Forward.Y) * SpanYcm;
	constexpr float Margin = 1.15f;
	const float ArmAcross = (FMath::Max(100.f, Across) * Margin) / FMath::Max(0.05f, 2.f * FMath::Tan(HFov * 0.5f));
	const float ArmAlong = (FMath::Max(100.f, Along) * Margin * FMath::Max(0.2f, FMath::Sin(Pitch)))
		/ FMath::Max(0.05f, 2.f * FMath::Tan(VFov * 0.5f));
	const float Arm = FMath::Max(ArmAcross, ArmAlong);
	bShotArmHeld = true;
	ShotArmLength = Arm;
	MaxArmLength = FMath::Max(MaxArmLength, Arm);
	TargetArmLength = Arm;
	CurrentArmLength = Arm;
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = Arm;
	}
	UE_LOG(LogTemp, Log, TEXT("KodCam Frame Span=%.0f,%.0f Arm=%.0f"), SpanXcm, SpanYcm, Arm);
}

void AKodRTSCameraPawn::NoteAlert(FVector WorldLocation, float VisibleSeconds)
{
	AlertLocation = WorldLocation;
	bHasAlert = true;
	AlertVisibleSeconds = FMath::Max(0.5f, VisibleSeconds);
	AlertNotedSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

bool AKodRTSCameraPawn::GetActiveAlert(FVector& OutLocation) const
{
	const UWorld* World = GetWorld();
	if (!bHasAlert || !World)
	{
		return false;
	}
	if (World->GetTimeSeconds() - AlertNotedSeconds > AlertVisibleSeconds)
	{
		return false;
	}
	OutLocation = AlertLocation;
	return true;
}

bool AKodRTSCameraPawn::GetGroundViewCorners(FVector& OutNearLeft, FVector& OutNearRight, FVector& OutFarRight, FVector& OutFarLeft) const
{
	if (!Camera)
	{
		return false;
	}
	const float GroundZ = GetActorLocation().Z;
	const FVector CamLoc = Camera->GetComponentLocation();
	const FRotator CamRot = Camera->GetComponentRotation();
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
	const float HalfHorizontal = FMath::DegreesToRadians(FMath::Max(1.f, Camera->FieldOfView) * 0.5f);
	const float HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / FMath::Max(0.1f, Aspect));
	const FVector Forward = CamRot.Vector();
	const FVector Right = FRotationMatrix(CamRot).GetScaledAxis(EAxis::Y);
	const FVector Up = FRotationMatrix(CamRot).GetScaledAxis(EAxis::Z);
	const float Corners[4][2] = {
		{-1.f, -1.f},
		{1.f, -1.f},
		{1.f, 1.f},
		{-1.f, 1.f}
	};
	FVector Hits[4];
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FVector Dir = Forward
			+ Right * (Corners[Index][0] * FMath::Tan(HalfHorizontal))
			+ Up * (Corners[Index][1] * FMath::Tan(HalfVertical));
		if (!Dir.Normalize() || FMath::IsNearlyZero(Dir.Z))
		{
			return false;
		}
		const float T = (GroundZ - CamLoc.Z) / Dir.Z;
		if (T <= 0.f)
		{
			return false;
		}
		Hits[Index] = CamLoc + Dir * T;
	}
	OutNearLeft = Hits[0];
	OutNearRight = Hits[1];
	OutFarRight = Hits[2];
	OutFarLeft = Hits[3];
	return true;
}

void AKodRTSCameraPawn::CopyViewFrom(const AKodRTSCameraPawn* Other)
{
	if (!Other)
	{
		return;
	}
	SetActorLocation(Other->GetActorLocation());
	SetActorRotation(FRotator(0.f, Other->GetActorRotation().Yaw, 0.f));
	TargetArmLength = FMath::Clamp(Other->GetCurrentArmLength(), MinArmLength, ZoomCeiling());
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
	(void)Health;
	(void)TargetId;
	if (Target && Attacker != Target)
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
	if (!bAllowAltDragRotate)
	{
		SetActorRotation(FRotator(0.f, YawDegrees, 0.f));
	}
	CameraBoom->TargetOffset = FVector::ZeroVector;
	CameraBoom->SocketOffset = FVector::ZeroVector;
	CameraBoom->SetRelativeLocation(FVector::ZeroVector);
	CameraBoom->SetRelativeRotation(FRotator(PitchDegrees, 0.f, RollDegrees));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = false;
	CameraBoom->bEnableCameraRotationLag = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritRoll = false;
	if (Camera)
	{
		Camera->SetRelativeLocation(FVector::ZeroVector);
		Camera->SetRelativeRotation(FRotator::ZeroRotator);
	}
	CameraBoom->UpdateComponentToWorld();
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

bool AKodRTSCameraPawn::IsChatFocused() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const AKodHUD* HUD = PC ? Cast<AKodHUD>(PC->GetHUD()) : nullptr;
	return HUD && HUD->IsChatOpen();
}

bool AKodRTSCameraPawn::HasSelectedStructure() const
{
	const AKodPlayerController* PC = Cast<AKodPlayerController>(GetController());
	const UKodSimSubsystem* Sim = GetWorld() ? GetWorld()->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!PC || !Sim)
	{
		return false;
	}
	for (AActor* Actor : PC->GetLocalSelection())
	{
		if (Actor && Sim->IsStructure(Sim->FindIdForActor(Actor)))
		{
			return true;
		}
	}
	return false;
}

void AKodRTSCameraPawn::PollMoveInput(float DeltaSeconds, bool& bOutManualPan)
{
	bOutManualPan = false;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || IsChatFocused())
	{
		PanVelocity = FVector::ZeroVector;
		return;
	}

	const bool bAlt = PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt);
	const bool bRotateDrag = bAllowAltDragRotate && bAlt
		&& (PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::MiddleMouseButton));

	if (!bRotateDrag && PC->WasInputKeyJustPressed(EKeys::MiddleMouseButton))
	{
		bGrabbing = true;
		bGrabMoved = false;
		bGrabHasWorld = false;
		float PressX = 0.f;
		float PressY = 0.f;
		PC->GetMousePosition(PressX, PressY);
		GrabMouseX = PressX;
		GrabMouseY = PressY;
		FVector Hit;
		if (GroundUnderCursor(Hit))
		{
			bGrabHasWorld = true;
			GrabWorld = Hit;
		}
	}
	if (PC->WasInputKeyJustReleased(EKeys::MiddleMouseButton))
	{
		float ReleaseX = 0.f;
		float ReleaseY = 0.f;
		PC->GetMousePosition(ReleaseX, ReleaseY);
		const float Drag = FVector2D(ReleaseX - GrabMouseX, ReleaseY - GrabMouseY).Size();
		if (bGrabbing && !bGrabMoved && Drag < 6.f && !bRotateDrag)
		{
			ResetZoomAndPitch();
		}
		bGrabbing = false;
	}
	else if (bRotateDrag)
	{
		bGrabbing = false;
	}
	if (bGrabbing)
	{
		float MoveX = 0.f;
		float MoveY = 0.f;
		if (PC->GetMousePosition(MoveX, MoveY) && FVector2D(MoveX - GrabMouseX, MoveY - GrabMouseY).Size() >= 6.f)
		{
			bGrabMoved = true;
		}
		if (bGrabHasWorld)
		{
			FVector CursorGround;
			if (CursorOnPlane(GrabWorld.Z, CursorGround))
			{
				const float Sens = FMath::Max(0.05f, MouseSensitivity);
				AddActorWorldOffset(FVector(GrabWorld.X - CursorGround.X, GrabWorld.Y - CursorGround.Y, 0.f) * Sens);
			}
		}
		PanVelocity = FVector::ZeroVector;
		bOutManualPan = true;
		return;
	}

	FVector2D KeysInput = FVector2D::ZeroVector;
	FVector2D EdgeInput = FVector2D::ZeroVector;
	if (PC->IsInputKeyDown(EKeys::Up))
	{
		KeysInput.Y += 1.f;
	}
	if (PC->IsInputKeyDown(EKeys::Down))
	{
		KeysInput.Y -= 1.f;
	}
	if (PC->IsInputKeyDown(EKeys::Right))
	{
		KeysInput.X += 1.f;
	}
	if (PC->IsInputKeyDown(EKeys::Left))
	{
		KeysInput.X -= 1.f;
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
				EdgeInput.X -= 1.f;
			}
			else if (MouseX >= static_cast<float>(SizeX - 1) - Margin)
			{
				EdgeInput.X += 1.f;
			}
			if (MouseY <= Margin)
			{
				EdgeInput.Y += 1.f;
			}
			else if (MouseY >= static_cast<float>(SizeY - 1) - Margin)
			{
				EdgeInput.Y -= 1.f;
			}
		}
	}

	if (KeysInput.SizeSquared() > 1.f)
	{
		KeysInput.Normalize();
	}
	if (EdgeInput.SizeSquared() > 1.f)
	{
		EdgeInput.Normalize();
	}
	const bool bEdge = !EdgeInput.IsNearlyZero();
	const FVector2D Input = bEdge ? EdgeInput * EdgeScrollSpeed : KeysInput * KeyboardScrollSpeed;
	if (!Input.IsNearlyZero())
	{
		bOutManualPan = true;
	}
	ApplyPan(DeltaSeconds, Input, bEdge);
}

void AKodRTSCameraPawn::ApplyPan(float DeltaSeconds, const FVector2D& UnitsPerSecond, bool bInstant)
{
	const FVector Desired = PlanarRight() * UnitsPerSecond.X + PlanarForward() * UnitsPerSecond.Y;
	const bool bStop = Desired.IsNearlyZero();
	const float Accel = FMath::Max(1.f, PanAccelSpeed) * FMath::Max(Desired.Size(), 1.f);
	PanVelocity = KodCameraSettings::IntegratePanVelocity(PanVelocity, Desired, DeltaSeconds, Accel, bInstant || bStop);
	if (!PanVelocity.IsNearlyZero())
	{
		AddActorWorldOffset(FVector(PanVelocity.X, PanVelocity.Y, 0.f) * DeltaSeconds);
	}
}

float AKodRTSCameraPawn::ZoomCeiling() const
{
	if (bShotArmHeld)
	{
		return FMath::Max(MaxArmLength, ShotArmLength);
	}
	if (IsA(AKodSpectatorPawn::StaticClass()))
	{
		return MaxArmLength;
	}
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	const EKodMatchMode Mode = Sim ? Sim->GetMatchMode() : EKodMatchMode::OfflineSkirmish;
	if (Mode == EKodMatchMode::Replay || Mode == EKodMatchMode::Custom)
	{
		return MaxArmLength;
	}
	return FMath::Min(MaxArmLength, DefaultArmLength);
}

void AKodRTSCameraPawn::ResetZoomAndPitch()
{
	TargetArmLength = FMath::Clamp(DefaultArmLength, MinArmLength, ZoomCeiling());
	CurrentArmLength = TargetArmLength;
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = CurrentArmLength;
		CameraBoom->SetRelativeRotation(FRotator(PitchDegrees, 0.f, RollDegrees));
	}
	UE_LOG(LogTemp, Log, TEXT("KodCam Reset Arm=%.0f Pitch=%.0f"), CurrentArmLength, PitchDegrees);
}

bool AKodRTSCameraPawn::HasLiftableSelection() const
{
	const AKodPlayerController* PC = Cast<AKodPlayerController>(GetController());
	const UKodSimSubsystem* Sim = GetWorld() ? GetWorld()->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!PC || !Sim)
	{
		return false;
	}
	for (AActor* Actor : PC->GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		FKodStructureState Structure;
		if (!Sim->TryGetStructure(Sim->FindIdForActor(Actor), Structure) || !Structure.bComplete)
		{
			continue;
		}
		if (Structure.DefinitionId == FName(KodWardenPaths::Id_CommandCenter)
			|| Structure.DefinitionId == FName(KodWardenPaths::Id_Barracks)
			|| Structure.DefinitionId == FName(KodWardenPaths::Id_EngineeringBay))
		{
			return true;
		}
	}
	return false;
}

void AKodRTSCameraPawn::PollZoom()
{
	if (IsChatFocused())
	{
		return;
	}
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
	const float Ceiling = ZoomCeiling();
	if (!FMath::IsNearlyZero(Wheel))
	{
		TargetArmLength = FMath::Clamp(TargetArmLength - Wheel * ZoomStep, MinArmLength, Ceiling);
		NoteZoomTarget(TEXT("Wheel"));
	}
	if (PC->WasInputKeyJustPressed(EKeys::End))
	{
		const bool bAtClose = FMath::Abs(TargetArmLength - MinArmLength) <= 1.f;
		const float Next = bAtClose ? DefaultArmLength : MinArmLength;
		TargetArmLength = FMath::Clamp(Next, MinArmLength, Ceiling);
		NoteZoomTarget(TEXT("End"));
		UE_LOG(LogTemp, Log, TEXT("KodCam Zoom End Arm=%.0f"), TargetArmLength);
	}
	if (Ceiling > DefaultArmLength + 1.f && PC->WasInputKeyJustPressed(EKeys::Z) && !HasLiftableSelection())
	{
		TargetArmLength = FMath::Clamp(TargetArmLength + ZoomStep, MinArmLength, Ceiling);
		NoteZoomTarget(TEXT("Key"));
	}
}

void AKodRTSCameraPawn::PollBookmarks()
{
	if (IsChatFocused())
	{
		return;
	}
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
	TargetArmLength = FMath::Clamp(Slot.ArmLength, MinArmLength, ZoomCeiling());
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
	if (IsChatFocused())
	{
		return;
	}
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
	if (IsChatFocused() || !bAllowAltDragRotate)
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
	if (!SupportsUnitFollow() || IsChatFocused() || HasSelectedStructure())
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

namespace
{
	bool IsMapGround(const AActor* Actor)
	{
		if (!Actor)
		{
			return false;
		}
		if (Actor->ActorHasTag(FName(TEXT("KodGround"))))
		{
			return true;
		}
		const FString ClassName = Actor->GetClass() ? Actor->GetClass()->GetName() : FString();
		if (ClassName.Contains(TEXT("Landscape")))
		{
			return true;
		}
		const FString Label = Actor->GetActorNameOrLabel();
		return Label.Contains(TEXT("PROXY_GROUND")) || Label.Contains(TEXT("Landscape"));
	}
}

void AKodRTSCameraPawn::UpdateTerrain(float DeltaSeconds, bool bInstant)
{
	(void)DeltaSeconds;
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
	TArray<FHitResult> Hits;
	float SampledZ = TargetGroundZ;
	bool bSampled = false;
	if (World->LineTraceMultiByChannel(Hits, Start, End, ECC_WorldStatic, Params))
	{
		for (const FHitResult& Hit : Hits)
		{
			if (!IsMapGround(Hit.GetActor()))
			{
				continue;
			}
			SampledZ = Hit.ImpactPoint.Z + TargetZOffsetUU;
			bSampled = true;
			break;
		}
	}
	// The first ground hit locks the play plane. Later samples, including cliffs, do not move it.
	if (bSampled && !bHaveGround)
	{
		TargetGroundZ = SampledZ;
		bHaveGround = true;
	}
	else if (bSampled)
	{
		TargetGroundZ = KodCameraSettings::HoldPlaneZ(TargetGroundZ, SampledZ, KodCameraSettings::PlaneHoldUU);
	}
	(void)bInstant;
	if (!bHaveGround)
	{
		return;
	}
	if (CameraBoom)
	{
		CameraBoom->bDoCollisionTest = false;
	}
	FVector Moved = Loc;
	Moved.Z = TargetGroundZ;
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
