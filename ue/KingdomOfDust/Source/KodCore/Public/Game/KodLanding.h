#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * Where a building sits when it is placed, finishes construction, spawns, or lands.
 * Yaw is the canonical actor yaw (0, plus debug kod.BuildingYaw). The footprint is the
 * square grid cells at 0 degrees. Meshes may sit slightly inside that pad; nav uses the cells.
 */
struct KODCORE_API FKodLandingPose
{
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
	int32 CellX = 0;
	int32 CellY = 0;
	int32 FootX = 1;
	int32 FootY = 1;
};

struct KODCORE_API FKodLandingFault
{
	bool bYaw = false;
	bool bLevel = false;
	bool bGrid = false;
	bool bSocket = false;

	bool Any() const { return bYaw || bLevel || bGrid || bSocket; }
};

namespace KodLanding
{
	KODCORE_API float CanonicalYaw();
	KODCORE_API FRotator CanonicalRotation();
	KODCORE_API FVector FootprintCenter(int32 CellX, int32 CellY, int32 FootX, int32 FootY, float GroundZ);
	KODCORE_API FKodLandingPose MakePose(int32 CellX, int32 CellY, int32 FootX, int32 FootY, float GroundZ);

	/** SocketActorSpace is the offset from the actor origin with yaw 0. SocketWorld is the current world point. */
	KODCORE_API FKodLandingFault Evaluate(
		const FRotator& Rotation,
		const FVector& Location,
		const FKodLandingPose& Pose,
		const TArray<FVector>& SocketActorSpace,
		const TArray<FVector>& SocketWorld);

	KODCORE_API FRotator EaseTowardCanonical(const FRotator& Current, float DeltaSeconds);

	/**
	 * Hard-sets the actor onto the pose when any check fails, and logs KodLanding.
	 * Returns true when the actor was already on the pose.
	 */
	KODCORE_API bool ValidateLandingOrientation(AActor* Actor, const FKodLandingPose& Pose);

	KODCORE_API bool SameTransform(const FVector& LocationA, float YawA, const FVector& LocationB, float YawB);
}
