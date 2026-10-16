#pragma once

#include "CoreMinimal.h"

/**
 * Construction view derived from FKodStructureState::BuildProgress.
 * The sim owns the 0-1 value. This only picks the mesh slot and the reveal.
 *
 * Master building material (masked, not translucent, so Nanite can keep it):
 *   Custom Primitive Data 0  BuildReveal, 0-1 inside the current stage only
 *   Custom Primitive Data 1-3  faction RGB for the emissive edge
 *   World-position Z height mask. The emissive band is 20-40 uu tall.
 *   No per-actor material instance. The component writes the primitive data.
 *
 * Dynasty ghost material is a separate unlit additive-translucent asset
 * (fresnel rim, scanline pan, opacity about 0.25) on the ghost's LOD1.
 */
struct KODCORE_API FKodBuildStageView
{
	/** 0, 1, 2 while building. 3 is the finished mesh. */
	int32 StageIndex = 3;
	/** 0-1 remapped inside the current third. 1 on the finished mesh. */
	float Reveal = 1.f;
	bool bFinal = true;
};

namespace KodBuildStage
{
	inline constexpr int32 RevealDataIndex = 0;
	inline constexpr int32 FactionDataIndex = 1;

	inline FKodBuildStageView FromProgress(float Progress)
	{
		FKodBuildStageView View;
		const float Clamped = FMath::Clamp(Progress, 0.f, 1.f);
		if (Clamped >= 1.f)
		{
			return View;
		}
		View.bFinal = false;
		const float Scaled = Clamped * 3.f;
		int32 Stage = FMath::FloorToInt(Scaled);
		float Reveal = Scaled - static_cast<float>(Stage);
		if (Reveal >= 0.999f)
		{
			Reveal = 0.f;
			++Stage;
		}
		View.StageIndex = FMath::Clamp(Stage, 0, 2);
		View.Reveal = FMath::Clamp(Reveal, 0.f, 1.f);
		return View;
	}

	/** Destructible and Geometry Collection actors keep the death path. */
	inline bool IsExemptClassName(const FString& ClassName)
	{
		return ClassName.Contains(TEXT("GeometryCollection")) || ClassName.Contains(TEXT("Destructible"));
	}

	inline FLinearColor FactionColor(int32 TeamId)
	{
		if (TeamId == 0)
		{
			return FLinearColor(FColor(0xD9, 0xB8, 0x78));
		}
		return FLinearColor(FColor(0xC4, 0x5A, 0x3A));
	}
}
