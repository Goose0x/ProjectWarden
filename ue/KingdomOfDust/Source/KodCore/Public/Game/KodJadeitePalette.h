#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KodJadeitePalette.generated.h"

class UMaterialParameterCollection;

DECLARE_MULTICAST_DELEGATE(FKodJadeitePaletteChanged);

/**
 * Visual Jadeite tuner. Not sim state and not part of the idle hash.
 *
 * MPC_KodJadeite (Tools/kod_jadeite_mpc_setup.py) holds JadeHueShift,
 * JadeSaturation, JadeBrightness, JadeEmissive, and the base colours
 * JadeShadow #0B4F5C, JadeBase #11928F, JadeLit #2DBFB8, JadeRim #8EEAE4,
 * JadeCore #22CDBE. MF_KodJadeiteTint reads that collection.
 *
 * Hook the function in a material:
 *   Base     -> base colour of a Jadeite node
 *   Emissive -> emissive of a Jadeite node, or a Scinder eye / regen glow
 * M_KodJadeite already does the node hook. A Scinder material instance that
 * still has a jade emissive parameter (name contains Jade, or EyeEmissive,
 * EyeGlow, EyeColor, RegenGlow, RegenEmissive) is written from here so the
 * sliders stay live before that hook exists.
 *
 * Slider values persist in GameUserSettings.ini [KodJadeite]. F10 is the
 * pause menu, so the dev panel toggles on F9, and only in campaign or
 * offline skirmish.
 */
UCLASS()
class KODCORE_API UKodJadeitePaletteSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float DefaultHueShift = 0.f;
	static constexpr float DefaultSaturation = 1.f;
	static constexpr float DefaultBrightness = 1.f;
	static constexpr float DefaultEmissive = 1.f;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	bool IsTuningAllowed() const;

	float GetHueShift() const { return HueShift; }
	float GetSaturation() const { return Saturation; }
	float GetBrightness() const { return Brightness; }
	float GetEmissive() const { return Emissive; }

	void SetHueShift(float Value);
	void SetSaturation(float Value);
	void SetBrightness(float Value);
	void SetEmissive(float Value);
	void ResetDefaults();

	/** Tuned sRGB hex block for the five base roles plus the emissive result. */
	FString FormatHex() const;

	FLinearColor TunedShadow() const;
	FLinearColor TunedBase() const;
	FLinearColor TunedLit() const;
	FLinearColor TunedRim() const;
	FLinearColor TunedCore() const;
	/** Tuned JadeCore multiplied by the glow slider. */
	FLinearColor TunedEmissive() const;

	FKodJadeitePaletteChanged OnPaletteChanged;

	static FLinearColor TintColor(const FLinearColor& Base, float InHueShift, float InSaturation, float InBrightness);

private:
	void LoadSettings();
	void SaveSettings(bool bFlush) const;
	void PushCollection();
	void Broadcast();
	UMaterialParameterCollection* ResolveCollection();
	FLinearColor BaseVector(FName ParameterName) const;
	static FLinearColor CanonicalBase(FName ParameterName);
	static FString HexOf(const FLinearColor& Color);

	float HueShift = DefaultHueShift;
	float Saturation = DefaultSaturation;
	float Brightness = DefaultBrightness;
	float Emissive = DefaultEmissive;

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> Collection;

	bool bLoggedMissingCollection = false;
	bool bLoggedPushMiss = false;
};
