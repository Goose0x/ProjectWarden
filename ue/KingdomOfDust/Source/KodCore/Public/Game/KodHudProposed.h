#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KodHudProposed.generated.h"

/**
 * Tunables the painted HUD addendum and WORLD_UI_SPEC mark PROPOSED, plus the
 * two HP band pairs. A content asset at /Game/Warden/UI/DA_KodHudProposed
 * overrides these defaults. Missing asset uses this class's constructor values.
 * Presentation only. Not sim state.
 *
 * Wireframe bands stay on the B33 cut (66% / 33%) unless this asset changes them.
 * World bars use the WORLD_UI cut (50% / 25%).
 */
UCLASS(BlueprintType)
class KODCORE_API UKodHudProposed : public UDataAsset
{
	GENERATED_BODY()

public:
	UKodHudProposed();

	/** HUD wireframe. 660 = 66% of max HP and above is green. */
	UPROPERTY(EditAnywhere, Category = "HP", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 WireHealthyPermille = 660;

	/** HUD wireframe. 330 = 33% of max HP and above is yellow. Below is red. */
	UPROPERTY(EditAnywhere, Category = "HP", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 WireWarningPermille = 330;

	/** World HP bars. 500 = above 50% is green. */
	UPROPERTY(EditAnywhere, Category = "HP", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 WorldHealthyPermille = 500;

	/** World HP bars. 250 = 25% and above is yellow. Below is red. */
	UPROPERTY(EditAnywhere, Category = "HP", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 WorldWarningPermille = 250;

	/** PROPOSED. Shield segments sit this many design pixels above the HP bar. */
	UPROPERTY(EditAnywhere, Category = "Shield")
	float ShieldGapPx = 2.f;

	UPROPERTY(EditAnywhere, Category = "Shield")
	FLinearColor ShieldColor;

	/** PROPOSED. Incomplete buildings draw at this opacity until the material exposes Opacity. */
	UPROPERTY(EditAnywhere, Category = "Construction", meta = (ClampMin = "0.05", ClampMax = "1"))
	float IncompleteOpacity = 0.55f;

	UPROPERTY(EditAnywhere, Category = "Construction")
	FLinearColor ConstructionColor;

	UPROPERTY(EditAnywhere, Category = "Construction")
	FLinearColor ConstructionLabelColor;

	/** PROPOSED. Vanguard supply lattice, shown only while placing a building that needs it. */
	UPROPERTY(EditAnywhere, Category = "Ground")
	float SupplyLatticeRadiusUU = 600.f;

	UPROPERTY(EditAnywhere, Category = "Ground")
	FLinearColor SupplyLatticeColor;

	UPROPERTY(EditAnywhere, Category = "Ground")
	FLinearColor SupplyRingColor;

	/** PROPOSED. Scinder toxic blotch colour. Drawn under the same proposed gate. */
	UPROPERTY(EditAnywhere, Category = "Ground")
	FLinearColor ToxicBlotchColor;

	/** PROPOSED. Left error stack replaces the centre flash for placement and resource errors. */
	UPROPERTY(EditAnywhere, Category = "Error")
	float ErrorX = 16.f;

	UPROPERTY(EditAnywhere, Category = "Error")
	float ErrorY = 620.f;

	UPROPERTY(EditAnywhere, Category = "Error")
	float ErrorW = 560.f;

	UPROPERTY(EditAnywhere, Category = "Error")
	float ErrorH = 84.f;

	UPROPERTY(EditAnywhere, Category = "Error")
	float ErrorHoldSeconds = 2.f;

	UPROPERTY(EditAnywhere, Category = "Error")
	float ErrorFadeSeconds = 1.f;

	UPROPERTY(EditAnywhere, Category = "Error")
	int32 ErrorMaxRows = 3;

	UPROPERTY(EditAnywhere, Category = "Error")
	float ErrorRowPx = 28.f;

	/** Alpha of the row above the newest, then the oldest, while the newest is fully opaque. */
	UPROPERTY(EditAnywhere, Category = "Error")
	float ErrorOlderAlpha = 0.70f;

	UPROPERTY(EditAnywhere, Category = "Error")
	float ErrorOldestAlpha = 0.43f;

	/** Design Y above which the alert feed is clipped while the stack is on screen. */
	UPROPERTY(EditAnywhere, Category = "Error")
	float AlertClipY = 612.f;

	static const TCHAR* AssetPath;
};
