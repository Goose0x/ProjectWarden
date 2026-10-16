#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Math/Box.h"
#include "KodRTSCameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USceneComponent;
class AActor;

/**
 * StarCraft 2 style player camera. Fixed pitch, yaw north, no WASD.
 * The pawn sits on the ground look-at point. The spring arm holds the camera up and back.
 * Numbers are config so DefaultGame.ini can retune them without a code change.
 */
UCLASS(Blueprintable, config = Game)
class KODCORE_API AKodRTSCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AKodRTSCameraPawn();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;

	/** One-shot world pan. X is right, Y is forward, in camera yaw. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Camera")
	void Pan(FVector2D AxisValue);

	/** Change the zoom target. Positive moves closer. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Camera")
	void Zoom(float AxisValue);

	/** Zoom and apply it on this frame. Shot hooks use this after the start camera is set. */
	void CommitZoomSteps(float Steps);

	/** Yaw the view. Ignored unless alt-drag rotate is enabled. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Camera")
	void OrbitYaw(float AxisValue);

	/** Instant move of the look-at point. Keeps zoom and yaw. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Camera")
	void JumpTo(FVector WorldLocation);

	/** Look at a point and fit a ground rectangle. Shot mode keeps this arm past the normal zoom clamp. */
	void FrameGroundSpan(FVector WorldLocation, float SpanXcm, float SpanYcm);

	/** Locked SC2 start: pitch -56, yaw 90, FOV 35, arm 6800, centred on the point. */
	void SnapToStartView(FVector WorldLocation);
	/** True on a frame after SnapToStartView, so a shot zoom is not undone by that snap. */
	bool HasStartViewSettled() const;
	void LogOpeningCamera();
	void NoteZoomTarget(const TCHAR* Reason);

	/** Player command center, or the P1 start tag. */
	AActor* FindCommandCenter() const;

	/** Snap the look-at point onto the player command center. */
	void SnapToPlayerStart();

	/** Last alert (unit hit or train complete). Space jumps here. */
	void NoteAlert(FVector WorldLocation, float VisibleSeconds = 4.f);

	/** Recent alert for the minimap ping. Space still uses the last alert after the ping fades. */
	bool GetActiveAlert(FVector& OutLocation) const;

	/** Light world shake. Ignored when the camera-shake setting is off. Presentation only. */
	void AddShakeAt(const FVector& WorldPoint, float Amplitude);

	/** Four ground corners of the view, near-left, near-right, far-right, far-left. */
	bool GetGroundViewCorners(FVector& OutNearLeft, FVector& OutNearRight, FVector& OutFarRight, FVector& OutFarLeft) const;

	/** Match location, yaw, and zoom. Used when entering spectator. */
	void CopyViewFrom(const AKodRTSCameraPawn* Other);

	float GetCurrentArmLength() const { return CurrentArmLength; }
	float GetActiveNearClipUU() const { return ActiveNearClipUU; }
	float GetActiveFarClipUU() const { return ActiveFarClipUU; }

	/** True while alt is held and alt-drag rotate is enabled. Player select should yield. */
	bool ConsumesAltDrag() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Degrees below the horizon. Negative looks down. kod.Cam.Pitch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float PitchDegrees = -56.f;

	/** World yaw. 90 puts map north at the top of the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float YawDegrees = 90.f;

	/** Camera roll. The Director default is 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float RollDegrees = 0.f;

	/**
	 * Vertical field of view. Applied as the matching horizontal FOV for the current viewport.
	 * kod.Cam.Fov retunes it live.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float VerticalFovDegrees = 35.f;

	/** World units per second at full scroll. SC2 "Scroll speed". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float ScrollSpeed = 4800.f;

	/** Viewport-edge pan, world units per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	float EdgeScrollSpeed = 4800.f;

	/** Arrow-key pan, world units per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	float KeyboardScrollSpeed = 4800.f;

	/** Scales middle-mouse grab. 1 matches the ground under the cursor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	float MouseSensitivity = 1.f;

	/** How fast pan speed catches the target. High is snappy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float PanAccelSpeed = 24.f;

	/** Mouse must be this close to the viewport edge to edge-scroll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float EdgeScrollMarginPx = 2.f;

	/** Arm-length change per mouse-wheel notch, in uu. kod.Cam.ZoomStep is in SC2 units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float ZoomStep = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float ZoomInterpSpeed = 12.f;

	/** Kept for kod.Cam.ZoomReturn. Wheel zoom holds; it does not ease back to the default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float ZoomReturnSpeed = 1.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float MinArmLength = 3200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float MaxArmLength = 6800.f;

	/** Resting distance. Gameplay is locked here. 34 SC2 units at 200 uu each. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float DefaultArmLength = 6800.f;

	/** How fast the look-at point settles onto the ground under it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float TerrainInterpSpeed = 8.f;

	/** Shrink the playable box inward before clamping the look-at point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float BoundsInsetUU = 0.f;

	/** SC2 default is off. Spectator turns it on. Alt+drag yaws. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	bool bAllowAltDragRotate = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float RotateDegreesPerPixel = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category = "Kod|Camera")
	float FollowInterpSpeed = 5.f;

	/** When set, this box wins over the KodGround / PROXY_GROUND bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	bool bUsePlayableBox = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Camera")
	FBox PlayableBox = FBox(ForceInit);

protected:
	bool bShotArmHeld = false;
	float ShotArmLength = 0.f;

	virtual bool WantsCommandCenterFraming() const { return bFrameOnCommandCenter; }
	virtual bool SupportsUnitFollow() const { return bSupportsUnitFollow; }
	virtual AActor* FindFollowTarget() const { return nullptr; }

	bool bFrameOnCommandCenter = true;
	bool bSupportsUnitFollow = false;
	/** BeginPlay can run before the command center exists. Tick retries once. */
	bool bPendingPlayerFocus = true;

	UFUNCTION()
	void HandleUnitHit(AActor* Attacker, AActor* Target, float Health, int32 TargetId);

	UFUNCTION()
	void HandleTrainCompleted(FVector Location, int32 TeamId);

	void ApplyVerticalFov();
	void ApplyTunables();
	void MaintainBoom();
	void FocusCommandCenter();
	void PollMoveInput(float DeltaSeconds, bool& bOutManualPan);
	void PollZoom();
	float ZoomCeiling() const;
	void ResetZoomAndPitch();
	bool HasLiftableSelection() const;
	void PollBookmarks();
	void PollAlert();
	void PollRotate();
	void PollFollow();
	bool IsChatFocused() const;
	bool HasSelectedStructure() const;
	void ApplyPan(float DeltaSeconds, const FVector2D& UnitsPerSecond, bool bInstant);
	void UpdateTerrain(float DeltaSeconds, bool bInstant);
	void ClampToPlayable();
	void UpdateFollow(float DeltaSeconds);
	void CachePlayableBounds();
	bool IsViewportFocused() const;
	bool GroundUnderCursor(FVector& OutWorld) const;
	bool CursorOnPlane(float PlaneZ, FVector& OutWorld) const;
	FVector PlanarForward() const;
	FVector PlanarRight() const;
	void SaveBookmark(int32 SlotNumber);
	void RecallBookmark(int32 SlotNumber);

	float CurrentArmLength = 6800.f;
	float TargetArmLength = 6800.f;
	float TargetZOffsetUU = 0.f;
	float ActiveNearClipUU = 20.f;
	float ActiveFarClipUU = 30000.f;
	float LoggedUnitScale = -1.f;
	float LoggedZoomArm = -1.f;
	bool bLoggedCamStart = false;
	bool bStartViewSnapped = false;
	uint64 StartViewFrame = 0;
	FVector PanVelocity = FVector::ZeroVector;
	bool bGrabbing = false;
	bool bGrabMoved = false;
	bool bGrabHasWorld = false;
	float GrabMouseX = 0.f;
	float GrabMouseY = 0.f;
	FVector GrabWorld = FVector::ZeroVector;
	bool bHaveGround = false;
	float TargetGroundZ = 0.f;
	bool bHasAlert = false;
	FVector AlertLocation = FVector::ZeroVector;
	float AlertVisibleSeconds = 4.f;
	float AlertNotedSeconds = -1.f;
	bool bFollowUnit = false;
	TWeakObjectPtr<AActor> FollowTarget;
	bool bLoggedInit = false;
	bool bViewCopied = false;
	bool bBoundSimEvents = false;
	bool bPlayableCached = false;
	FBox CachedPlayable = FBox(ForceInit);

	struct FKodCameraBookmark
	{
		bool bSet = false;
		FVector Location = FVector::ZeroVector;
		float Yaw = 0.f;
		float ArmLength = 0.f;
	};
	FKodCameraBookmark Bookmarks[4];

	float ShakeAmp = 0.f;
	FVector ShakeApplied = FVector::ZeroVector;
};
