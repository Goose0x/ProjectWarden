#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class USkeletalMesh;

/**
 * One [KodMeshes] row. Paths are soft object paths without a required .Asset suffix.
 * Scale multiplies the fit-to-footprint scale on buildings and the body scale on units.
 * Yaw is degrees, +X forward. Z is centimeters added to the mount.
 * Anim is an optional AnimBP class path. Empty keeps the definition clips.
 */
struct KODUNITS_API FKodMeshRow
{
	FName Id;
	FString Primary;
	FString Fallback;
	float Scale = 1.f;
	float Yaw = 0.f;
	float Z = 0.f;
	FString Anim;
};

/** Result of resolving one id. Path is the primary or the fallback that was chosen. */
struct KODUNITS_API FKodMeshChoice
{
	FName Id;
	bool bHasRow = false;
	FString Path;
	bool bLoaded = false;
	bool bFallback = false;
	bool bSkeletal = false;
	/** Horizontal span in centimeters after scale. 0 when nothing loaded. */
	float BoundsCm = 0.f;
	float Scale = 1.f;
	float Yaw = 0.f;
	float Z = 0.f;
	FString Anim;
	UStaticMesh* StaticMesh = nullptr;
	USkeletalMesh* SkeletalMesh = nullptr;
};

/** Mesh paths live in DefaultGame.ini [KodMeshes], not in the Make* bootstrap. */
class KODUNITS_API FKodMeshCatalog
{
public:
	static const TArray<FKodMeshRow>& Rows();
	static FKodMeshChoice Resolve(FName Id);
	static float FootprintCm(FName Id);
	static bool SpanWithin(float BoundsCm, float ExpectedCm, float Fraction = 0.15f);
	/** Raw mesh span times the scale that fits Expected, then times the row scale. */
	static float FittedSpanCm(float RawSpanCm, float RowScale, float FootprintCm);
};
