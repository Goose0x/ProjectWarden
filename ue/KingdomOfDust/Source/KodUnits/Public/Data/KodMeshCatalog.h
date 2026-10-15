#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class USkeletalMesh;

/** How a row is scaled onto the world. Unset keeps the id's historical rule. */
enum class EKodMeshFit : uint8
{
	Unset,
	/** Standing body height. Infantry and workers. */
	Height,
	/** Largest horizontal extent. Vehicles such as the AMP. */
	Length,
	/** Long side of the build grid. Buildings. */
	Footprint
};

/**
 * One [KodMeshes] row. Paths are soft object paths without a required .Asset suffix.
 * Scale multiplies the fitted size. Yaw is degrees, +X forward. Z is centimeters added to the mount.
 * Anim is an optional AnimBP class path. Empty keeps the definition clips.
 * FitCm is the target centimeters for Fit. 0 keeps the id default (242, 286, 260, or the grid).
 */
struct KODUNITS_API FKodMeshRow
{
	FName Id;
	FString Primary;
	FString Fallback;
	/** Tried after Fallback. */
	FString Fallback2;
	float Scale = 1.f;
	float Yaw = 0.f;
	float Z = 0.f;
	EKodMeshFit Fit = EKodMeshFit::Unset;
	float FitCm = 0.f;
	/** Length rows only. Scale is the larger of FitCm/length and MinHeightCm/height. 0 ignores height. */
	float MinHeightCm = 0.f;
	FString Anim;
	/** Folder of short-named sequences, for example the supply-depot raise and lower clips. */
	FString AnimFolder;
	FString RaiseAnim;
	FString LowerAnim;
	FString DeckExtendAnim;
	FString DeckRetractAnim;
};

/**
 * Optional piece hung on an owner's body. Socket None is the body origin.
 * Anim is the idle clip. Open, Close, and IdleOpen are optional and, when empty,
 * are the idle path with IdleClosed replaced.
 */
struct KODUNITS_API FKodMeshAttach
{
	FName OwnerId;
	FString Mesh;
	FName Socket;
	FString Anim;
	FString OpenAnim;
	FString CloseAnim;
	FString IdleOpenAnim;
};

/** Attach rows for one owner, in ini order. */
KODUNITS_API void KodMeshCatalogCopyAttaches(FName OwnerId, TArray<FKodMeshAttach>& OutAttaches);

/** Copy of one ini row. False when the id is not in [KodMeshes]. */
KODUNITS_API bool KodMeshCatalogFindRow(FName Id, FKodMeshRow& OutRow);

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
