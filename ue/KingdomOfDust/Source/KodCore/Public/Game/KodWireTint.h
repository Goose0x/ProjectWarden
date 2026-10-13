#pragma once

#include "CoreMinimal.h"

/**
 * HUD spec HP tokens. The wireframe keeps the B33 cut:
 * HP.Green #4CD964 at 66% and above. HP.Yellow #F2C230 from 33% up to 66%.
 * HP.Red #E5383B below 33%. World bars use UKodHudProposed (50% / 25%).
 * #3DFF6A and #FF3D3D are placement legality, not these bands.
 * Presentation only. Not sim state and not part of the idle hash.
 * The wireframe slot draws the unit icon through this tint.
 * A zone mask may split the tint. Without one, the whole icon uses overall HP.
 * Units masks are head / torso / legs / weapon. Buildings are roof / body / base.
 * ShieldRim is a blue edge glow when shield points are above zero.
 */
namespace KodWireTint
{
	constexpr int32 MaterialZoneCount = 4;

	enum class EBand : uint8
	{
		Healthy,
		Warning,
		Critical
	};

	enum class EZone : uint8
	{
		Head,
		Torso,
		Legs,
		Weapon,
		Roof,
		Body,
		Base
	};

	/** Slot size. Both sizes sample the same unit icon. */
	enum class ESlot : uint8
	{
		Info160,
		Group48
	};

	/** Blue rim. Not an HP token. */
	inline FLinearColor ShieldRimColor()
	{
		return FLinearColor(FColor(0x4D, 0xA6, 0xFF));
	}

	/** 0 hides the rim. A positive shield with no max is a full rim. */
	inline float ShieldRimStrength(int32 Shield, int32 ShieldMax)
	{
		if (Shield <= 0)
		{
			return 0.f;
		}
		if (ShieldMax <= 0)
		{
			return 1.f;
		}
		const int32 Clamped = Shield > ShieldMax ? ShieldMax : Shield;
		return static_cast<float>(Clamped) / static_cast<float>(ShieldMax);
	}

	constexpr EBand BandFor(int32 Health, int32 MaxHealth)
	{
		if (MaxHealth <= 0)
		{
			return EBand::Critical;
		}
		const int32 Hp = Health < 0 ? 0 : (Health > MaxHealth ? MaxHealth : Health);
		const int64 Scaled = static_cast<int64>(Hp) * 100;
		if (Scaled >= static_cast<int64>(MaxHealth) * 66)
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
	static_assert(BandFor(66, 100) == EBand::Healthy, "66% is green");
	static_assert(BandFor(65, 100) == EBand::Warning, "below 66% is yellow");
	static_assert(BandFor(50, 100) == EBand::Warning, "mid HP is yellow");
	static_assert(BandFor(33, 100) == EBand::Warning, "33% is yellow");
	static_assert(BandFor(32, 100) == EBand::Critical, "below 33% is red");
	static_assert(BandFor(0, 100) == EBand::Critical, "empty HP is red");
	static_assert(BandFor(0, 0) == EBand::Critical, "missing max HP is red");
	static_assert(BandFor(-4, 100) == EBand::Critical, "negative HP clamps to red");
	static_assert(BandFor(200, 100) == EBand::Healthy, "HP above max clamps to green");
	static_assert(BandFor(660, 1000) == EBand::Healthy, "remain 0.66 is green");
	static_assert(BandFor(659, 1000) == EBand::Warning, "remain below 0.66 is yellow");
	static_assert(BandFor(330, 1000) == EBand::Warning, "remain 0.33 is yellow");
	static_assert(BandFor(329, 1000) == EBand::Critical, "remain below 0.33 is red");

	/** HP.Green #4CD964 */
	inline FLinearColor Healthy()
	{
		return FLinearColor(FColor(0x4C, 0xD9, 0x64));
	}

	/** HP.Yellow #F2C230 */
	inline FLinearColor Warning()
	{
		return FLinearColor(FColor(0xF2, 0xC2, 0x30));
	}

	/** HP.Red #E5383B */
	inline FLinearColor Critical()
	{
		return FLinearColor(FColor(0xE5, 0x38, 0x3B));
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

	/**
	 * Data-driven cut. Permille is thousandths of max HP.
	 * Wireframe passes 660/330 (B33). World bars pass 500/250 (WORLD_UI).
	 */
	/**
	 * bAboveHealthy: world bars are green only above the healthy cut (50% is yellow).
	 * Wireframe passes false so 66% stays green.
	 */
	inline EBand BandForPermille(int32 Health, int32 MaxHealth, int32 HealthyPermille, int32 WarningPermille, bool bAboveHealthy)
	{
		if (MaxHealth <= 0)
		{
			return EBand::Critical;
		}
		const int32 Hp = Health < 0 ? 0 : (Health > MaxHealth ? MaxHealth : Health);
		const int32 HealthyCut = HealthyPermille < 1 ? 1 : (HealthyPermille > 1000 ? 1000 : HealthyPermille);
		const int32 WarningCut = WarningPermille < 0 ? 0 : (WarningPermille > HealthyCut ? HealthyCut : WarningPermille);
		const int64 Scaled = static_cast<int64>(Hp) * 1000;
		const int64 HealthyLine = static_cast<int64>(MaxHealth) * static_cast<int64>(HealthyCut);
		const bool bHealthy = bAboveHealthy ? (Scaled > HealthyLine) : (Scaled >= HealthyLine);
		if (bHealthy)
		{
			return EBand::Healthy;
		}
		if (Scaled >= static_cast<int64>(MaxHealth) * static_cast<int64>(WarningCut))
		{
			return EBand::Warning;
		}
		return EBand::Critical;
	}

	inline FLinearColor FromHitPoints(int32 Health, int32 MaxHealth)
	{
		return ColorFor(BandFor(Health, MaxHealth));
	}

	inline FLinearColor FromHitPointsPermille(int32 Health, int32 MaxHealth, int32 HealthyPermille, int32 WarningPermille, bool bAboveHealthy)
	{
		return ColorFor(BandForPermille(Health, MaxHealth, HealthyPermille, WarningPermille, bAboveHealthy));
	}

	/** Same bands as the health bar: >= 66% green, >= 33% yellow, otherwise red. */
	inline FLinearColor FromRemain(float Remain)
	{
		const float Clamped = Remain < 0.f ? 0.f : (Remain > 1.f ? 1.f : Remain);
		if (Clamped >= 0.66f)
		{
			return Healthy();
		}
		if (Clamped >= 0.33f)
		{
			return Warning();
		}
		return Critical();
	}

	inline FLinearColor FromRemainPermille(float Remain, int32 HealthyPermille, int32 WarningPermille, bool bAboveHealthy)
	{
		const float Clamped = Remain < 0.f ? 0.f : (Remain > 1.f ? 1.f : Remain);
		const int32 Hp = FMath::RoundToInt(Clamped * 1000.f);
		return ColorFor(BandForPermille(Hp, 1000, HealthyPermille, WarningPermille, bAboveHealthy));
	}

	/** One slice of the silhouette, in 0..1 slot space. Remain is that zone's HP fraction. */
	struct FZone
	{
		EZone Id = EZone::Torso;
		float U0 = 0.f;
		float V0 = 0.f;
		float U1 = 1.f;
		float V1 = 1.f;
		float Remain = 1.f;
	};

	inline void BuildZones(bool bStructure, TArray<FZone>& Out)
	{
		Out.Reset();
		auto Add = [&Out](EZone Id, float U0, float V0, float U1, float V1)
		{
			FZone Zone;
			Zone.Id = Id;
			Zone.U0 = U0;
			Zone.V0 = V0;
			Zone.U1 = U1;
			Zone.V1 = V1;
			Out.Add(Zone);
		};
		if (bStructure)
		{
			Add(EZone::Roof, 0.10f, 0.06f, 0.90f, 0.34f);
			Add(EZone::Body, 0.18f, 0.34f, 0.82f, 0.74f);
			Add(EZone::Base, 0.08f, 0.74f, 0.92f, 0.96f);
			return;
		}
		Add(EZone::Head, 0.34f, 0.04f, 0.66f, 0.26f);
		Add(EZone::Torso, 0.28f, 0.26f, 0.66f, 0.58f);
		Add(EZone::Legs, 0.30f, 0.58f, 0.70f, 0.96f);
		Add(EZone::Weapon, 0.66f, 0.32f, 0.96f, 0.52f);
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
