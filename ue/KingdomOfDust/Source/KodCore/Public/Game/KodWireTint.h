#pragma once

#include "CoreMinimal.h"

/**
 * HP bands for wireframe zones.
 * Green when HP is above 66%. Yellow from 33% through 66%. Red below 33%.
 * Presentation only. Not sim state and not part of the idle hash.
 * All zones use overall HP until a caller fills per-zone Remain values.
 */
namespace KodWireTint
{
	constexpr int32 MaterialZoneCount = 6;

	enum class EBand : uint8
	{
		Healthy,
		Warning,
		Critical
	};

	constexpr EBand BandFor(int32 Health, int32 MaxHealth)
	{
		if (MaxHealth <= 0)
		{
			return EBand::Critical;
		}
		const int32 Hp = Health < 0 ? 0 : (Health > MaxHealth ? MaxHealth : Health);
		const int64 Scaled = static_cast<int64>(Hp) * 100;
		if (Scaled > static_cast<int64>(MaxHealth) * 66)
		{
			return EBand::Healthy;
		}
		if (Scaled >= static_cast<int64>(MaxHealth) * 33)
		{
			return EBand::Warning;
		}
		return EBand::Critical;
	}

	static_assert(BandFor(100, 100) == EBand::Healthy, "full HP is green");
	static_assert(BandFor(67, 100) == EBand::Healthy, "above 66% is green");
	static_assert(BandFor(66, 100) == EBand::Warning, "66% is yellow");
	static_assert(BandFor(50, 100) == EBand::Warning, "mid HP is yellow");
	static_assert(BandFor(33, 100) == EBand::Warning, "33% is yellow");
	static_assert(BandFor(32, 100) == EBand::Critical, "below 33% is red");
	static_assert(BandFor(0, 100) == EBand::Critical, "empty HP is red");
	static_assert(BandFor(0, 0) == EBand::Critical, "missing max HP is red");
	static_assert(BandFor(-4, 100) == EBand::Critical, "negative HP clamps to red");
	static_assert(BandFor(200, 100) == EBand::Healthy, "HP above max clamps to green");
	static_assert(BandFor(661, 1000) == EBand::Healthy, "remain above 0.66 is green");
	static_assert(BandFor(660, 1000) == EBand::Warning, "remain 0.66 is yellow");
	static_assert(BandFor(330, 1000) == EBand::Warning, "remain 0.33 is yellow");
	static_assert(BandFor(329, 1000) == EBand::Critical, "remain below 0.33 is red");

	inline FLinearColor Healthy()
	{
		return FLinearColor(FColor(0x3D, 0xFF, 0x6A));
	}

	inline FLinearColor Warning()
	{
		return FLinearColor(FColor(0xFF, 0xD2, 0x3D));
	}

	inline FLinearColor Critical()
	{
		return FLinearColor(FColor(0xFF, 0x3D, 0x3D));
	}

	inline FLinearColor ColorFor(EBand Band)
	{
		if (Band == EBand::Healthy)
		{
			return Healthy();
		}
		if (Band == EBand::Warning)
		{
			return Warning();
		}
		return Critical();
	}

	inline FLinearColor FromHitPoints(int32 Health, int32 MaxHealth)
	{
		return ColorFor(BandFor(Health, MaxHealth));
	}

	/** Remain is 0..1. Thousandths use the same bands as FromHitPoints. */
	inline FLinearColor FromRemain(float Remain)
	{
		const float Clamped = Remain < 0.f ? 0.f : (Remain > 1.f ? 1.f : Remain);
		const int32 Milli = FMath::RoundToInt(Clamped * 1000.f);
		return FromHitPoints(Milli, 1000);
	}

	/** One slice of the silhouette, in 0..1 slot space. Remain is that zone's HP fraction. */
	struct FZone
	{
		float U0 = 0.f;
		float V0 = 0.f;
		float U1 = 1.f;
		float V1 = 1.f;
		float Remain = 1.f;
	};

	inline void BuildZones(bool bStructure, TArray<FZone>& Out)
	{
		Out.Reset();
		auto Add = [&Out](float U0, float V0, float U1, float V1)
		{
			FZone Zone;
			Zone.U0 = U0;
			Zone.V0 = V0;
			Zone.U1 = U1;
			Zone.V1 = V1;
			Out.Add(Zone);
		};
		if (bStructure)
		{
			Add(0.08f, 0.08f, 0.48f, 0.48f);
			Add(0.52f, 0.08f, 0.92f, 0.48f);
			Add(0.08f, 0.52f, 0.48f, 0.92f);
			Add(0.52f, 0.52f, 0.92f, 0.92f);
			return;
		}
		Add(0.36f, 0.06f, 0.64f, 0.26f);
		Add(0.30f, 0.28f, 0.70f, 0.56f);
		Add(0.08f, 0.30f, 0.28f, 0.50f);
		Add(0.72f, 0.30f, 0.92f, 0.50f);
		Add(0.32f, 0.58f, 0.46f, 0.94f);
		Add(0.54f, 0.58f, 0.68f, 0.94f);
	}

	inline void ApplyOverallHp(TArray<FZone>& Zones, int32 Health, int32 MaxHealth)
	{
		const float Remain = MaxHealth <= 0
			? 0.f
			: static_cast<float>(Health < 0 ? 0 : (Health > MaxHealth ? MaxHealth : Health)) / static_cast<float>(MaxHealth);
		for (FZone& Zone : Zones)
		{
			Zone.Remain = Remain;
		}
	}

	/** Later per-zone HP. Returns false and leaves Remain unchanged when the counts differ. */
	inline bool ApplyPerZone(TArray<FZone>& Zones, const TArray<float>& Remains)
	{
		if (Remains.Num() != Zones.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Zones.Num(); ++Index)
		{
			const float Remain = Remains[Index];
			Zones[Index].Remain = Remain < 0.f ? 0.f : (Remain > 1.f ? 1.f : Remain);
		}
		return true;
	}
}
