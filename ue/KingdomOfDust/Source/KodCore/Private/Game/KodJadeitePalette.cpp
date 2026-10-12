#include "Game/KodJadeitePalette.h"

#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/ConfigCacheIni.h"
#include "Sim/KodSimSubsystem.h"

namespace
{
	const TCHAR* SettingsSection = TEXT("KodJadeite");
	const TCHAR* CollectionPath = TEXT("/Game/Warden/Materials/MPC_KodJadeite.MPC_KodJadeite");

	const FName NameHue(TEXT("JadeHueShift"));
	const FName NameSaturation(TEXT("JadeSaturation"));
	const FName NameBrightness(TEXT("JadeBrightness"));
	const FName NameEmissive(TEXT("JadeEmissive"));
	const FName NameShadow(TEXT("JadeShadow"));
	const FName NameBase(TEXT("JadeBase"));
	const FName NameLit(TEXT("JadeLit"));
	const FName NameRim(TEXT("JadeRim"));
	const FName NameCore(TEXT("JadeCore"));

	float ClampHue(float Value)
	{
		return FMath::Clamp(Value, -0.5f, 0.5f);
	}

	float ClampScale(float Value)
	{
		return FMath::Clamp(Value, 0.f, 2.f);
	}

	float ClampGlow(float Value)
	{
		return FMath::Clamp(Value, 0.f, 4.f);
	}

	void AddScalar(UMaterialParameterCollection* Collection, FName Name, float DefaultValue)
	{
		FCollectionScalarParameter Parameter;
		Parameter.ParameterName = Name;
		Parameter.DefaultValue = DefaultValue;
		Parameter.Id = FGuid::NewGuid();
		Collection->ScalarParameters.Add(Parameter);
	}

	void AddVector(UMaterialParameterCollection* Collection, FName Name, const FLinearColor& DefaultValue)
	{
		FCollectionVectorParameter Parameter;
		Parameter.ParameterName = Name;
		Parameter.DefaultValue = DefaultValue;
		Parameter.Id = FGuid::NewGuid();
		Collection->VectorParameters.Add(Parameter);
	}
}

FLinearColor UKodJadeitePaletteSubsystem::TintColor(const FLinearColor& Base, float InHueShift, float InSaturation, float InBrightness)
{
	// Keep this in step with the HLSL in Tools/kod_jadeite_mpc_setup.py.
	const float R = FMath::Clamp(Base.R, 0.f, 1.f);
	const float G = FMath::Clamp(Base.G, 0.f, 1.f);
	const float B = FMath::Clamp(Base.B, 0.f, 1.f);
	const float MaxC = FMath::Max3(R, G, B);
	const float Delta = MaxC - FMath::Min3(R, G, B);
	float Hue = 0.f;
	float Sat = MaxC <= 0.f ? 0.f : Delta / MaxC;
	float Val = MaxC;
	if (Delta > 1.0e-5f)
	{
		if (MaxC == R)
		{
			Hue = (G - B) / Delta;
		}
		else if (MaxC == G)
		{
			Hue = (B - R) / Delta + 2.f;
		}
		else
		{
			Hue = (R - G) / Delta + 4.f;
		}
		Hue = FMath::Fmod(Hue / 6.f + 1.f, 1.f);
	}
	Hue = FMath::Frac(Hue + InHueShift);
	Sat = FMath::Clamp(Sat * InSaturation, 0.f, 1.f);
	Val = FMath::Clamp(Val * InBrightness, 0.f, 1.f);

	const float H6 = Hue * 6.f;
	const float Sector = FMath::FloorToFloat(H6);
	const float Frac = H6 - Sector;
	const float P = Val * (1.f - Sat);
	const float Q = Val * (1.f - Frac * Sat);
	const float T = Val * (1.f - (1.f - Frac) * Sat);
	FLinearColor Out(Val, Val, Val, 1.f);
	if (Sector < 1.f)
	{
		Out = FLinearColor(Val, T, P, 1.f);
	}
	else if (Sector < 2.f)
	{
		Out = FLinearColor(Q, Val, P, 1.f);
	}
	else if (Sector < 3.f)
	{
		Out = FLinearColor(P, Val, T, 1.f);
	}
	else if (Sector < 4.f)
	{
		Out = FLinearColor(P, Q, Val, 1.f);
	}
	else if (Sector < 5.f)
	{
		Out = FLinearColor(T, P, Val, 1.f);
	}
	else
	{
		Out = FLinearColor(Val, P, Q, 1.f);
	}
	return Out;
}

bool UKodJadeitePaletteSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UKodJadeitePaletteSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadSettings();
	ResolveCollection();
	PushCollection();
}

void UKodJadeitePaletteSubsystem::Deinitialize()
{
	SaveSettings(true);
	Super::Deinitialize();
}

bool UKodJadeitePaletteSubsystem::IsTuningAllowed() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	if (!Sim)
	{
		return World->IsGameWorld();
	}
	return Sim->AreCheatsAllowed();
}

void UKodJadeitePaletteSubsystem::SetHueShift(float Value)
{
	if (!IsTuningAllowed())
	{
		return;
	}
	HueShift = ClampHue(Value);
	PushCollection();
	SaveSettings(false);
	Broadcast();
}

void UKodJadeitePaletteSubsystem::SetSaturation(float Value)
{
	if (!IsTuningAllowed())
	{
		return;
	}
	Saturation = ClampScale(Value);
	PushCollection();
	SaveSettings(false);
	Broadcast();
}

void UKodJadeitePaletteSubsystem::SetBrightness(float Value)
{
	if (!IsTuningAllowed())
	{
		return;
	}
	Brightness = ClampScale(Value);
	PushCollection();
	SaveSettings(false);
	Broadcast();
}

void UKodJadeitePaletteSubsystem::SetEmissive(float Value)
{
	if (!IsTuningAllowed())
	{
		return;
	}
	Emissive = ClampGlow(Value);
	PushCollection();
	SaveSettings(false);
	Broadcast();
}

void UKodJadeitePaletteSubsystem::ResetDefaults()
{
	if (!IsTuningAllowed())
	{
		return;
	}
	HueShift = DefaultHueShift;
	Saturation = DefaultSaturation;
	Brightness = DefaultBrightness;
	Emissive = DefaultEmissive;
	PushCollection();
	SaveSettings(true);
	Broadcast();
}

FString UKodJadeitePaletteSubsystem::FormatHex() const
{
	return FString::Printf(
		TEXT("JadeShadow #%s\nJadeBase #%s\nJadeLit #%s\nJadeRim #%s\nJadeCore #%s\nEmissive #%s"),
		*HexOf(TunedShadow()),
		*HexOf(TunedBase()),
		*HexOf(TunedLit()),
		*HexOf(TunedRim()),
		*HexOf(TunedCore()),
		*HexOf(TunedEmissive()));
}

FLinearColor UKodJadeitePaletteSubsystem::TunedShadow() const
{
	return TintColor(BaseVector(NameShadow), HueShift, Saturation, Brightness);
}

FLinearColor UKodJadeitePaletteSubsystem::TunedBase() const
{
	return TintColor(BaseVector(NameBase), HueShift, Saturation, Brightness);
}

FLinearColor UKodJadeitePaletteSubsystem::TunedLit() const
{
	return TintColor(BaseVector(NameLit), HueShift, Saturation, Brightness);
}

FLinearColor UKodJadeitePaletteSubsystem::TunedRim() const
{
	return TintColor(BaseVector(NameRim), HueShift, Saturation, Brightness);
}

FLinearColor UKodJadeitePaletteSubsystem::TunedCore() const
{
	return TintColor(BaseVector(NameCore), HueShift, Saturation, Brightness);
}

FLinearColor UKodJadeitePaletteSubsystem::TunedEmissive() const
{
	return TunedCore() * Emissive;
}

void UKodJadeitePaletteSubsystem::LoadSettings()
{
	float LoadedHue = DefaultHueShift;
	float LoadedSat = DefaultSaturation;
	float LoadedBri = DefaultBrightness;
	float LoadedEmissive = DefaultEmissive;
	if (GConfig->GetFloat(SettingsSection, TEXT("HueShift"), LoadedHue, GGameUserSettingsIni))
	{
		HueShift = ClampHue(LoadedHue);
	}
	if (GConfig->GetFloat(SettingsSection, TEXT("Saturation"), LoadedSat, GGameUserSettingsIni))
	{
		Saturation = ClampScale(LoadedSat);
	}
	if (GConfig->GetFloat(SettingsSection, TEXT("Brightness"), LoadedBri, GGameUserSettingsIni))
	{
		Brightness = ClampScale(LoadedBri);
	}
	if (GConfig->GetFloat(SettingsSection, TEXT("Emissive"), LoadedEmissive, GGameUserSettingsIni))
	{
		Emissive = ClampGlow(LoadedEmissive);
	}
}

void UKodJadeitePaletteSubsystem::SaveSettings(bool bFlush) const
{
	GConfig->SetFloat(SettingsSection, TEXT("HueShift"), HueShift, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("Saturation"), Saturation, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("Brightness"), Brightness, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("Emissive"), Emissive, GGameUserSettingsIni);
	if (bFlush)
	{
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void UKodJadeitePaletteSubsystem::Broadcast()
{
	OnPaletteChanged.Broadcast();
}

FLinearColor UKodJadeitePaletteSubsystem::CanonicalBase(FName ParameterName)
{
	if (ParameterName == NameShadow)
	{
		return FLinearColor(FColor(0x0B, 0x4F, 0x5C));
	}
	if (ParameterName == NameBase)
	{
		return FLinearColor(FColor(0x11, 0x92, 0x8F));
	}
	if (ParameterName == NameLit)
	{
		return FLinearColor(FColor(0x2D, 0xBF, 0xB8));
	}
	if (ParameterName == NameRim)
	{
		return FLinearColor(FColor(0x8E, 0xEA, 0xE4));
	}
	return FLinearColor(FColor(0x22, 0xCD, 0xBE));
}

FLinearColor UKodJadeitePaletteSubsystem::BaseVector(FName ParameterName) const
{
	if (Collection)
	{
		for (const FCollectionVectorParameter& Parameter : Collection->VectorParameters)
		{
			if (Parameter.ParameterName == ParameterName)
			{
				return Parameter.DefaultValue;
			}
		}
	}
	return CanonicalBase(ParameterName);
}

FString UKodJadeitePaletteSubsystem::HexOf(const FLinearColor& Color)
{
	const FColor Srgb = Color.ToFColor(true);
	return FString::Printf(TEXT("%02X%02X%02X"), Srgb.R, Srgb.G, Srgb.B);
}

UMaterialParameterCollection* UKodJadeitePaletteSubsystem::ResolveCollection()
{
	if (Collection)
	{
		return Collection;
	}
	Collection = LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath, nullptr, LOAD_NoWarn);
	if (Collection)
	{
		return Collection;
	}
	if (!bLoggedMissingCollection)
	{
		bLoggedMissingCollection = true;
		UE_LOG(LogTemp, Log, TEXT("KodJadeite MPC missing. Run Tools/kod_jadeite_mpc_setup.py so materials can read MPC_KodJadeite. Using a transient collection until then."));
	}
	UMaterialParameterCollection* Transient = NewObject<UMaterialParameterCollection>(GetTransientPackage(), NAME_None, RF_Transient);
	AddScalar(Transient, NameHue, DefaultHueShift);
	AddScalar(Transient, NameSaturation, DefaultSaturation);
	AddScalar(Transient, NameBrightness, DefaultBrightness);
	AddScalar(Transient, NameEmissive, DefaultEmissive);
	AddVector(Transient, NameShadow, CanonicalBase(NameShadow));
	AddVector(Transient, NameBase, CanonicalBase(NameBase));
	AddVector(Transient, NameLit, CanonicalBase(NameLit));
	AddVector(Transient, NameRim, CanonicalBase(NameRim));
	AddVector(Transient, NameCore, CanonicalBase(NameCore));
	Transient->StateId = FGuid::NewGuid();
	Transient->PostLoad();
	Collection = Transient;
	return Collection;
}

void UKodJadeitePaletteSubsystem::PushCollection()
{
	UWorld* World = GetWorld();
	UMaterialParameterCollection* Resolved = ResolveCollection();
	if (!World || !Resolved)
	{
		return;
	}
	UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(Resolved);
	if (!Instance)
	{
		return;
	}
	const bool bHue = Instance->SetScalarParameterValue(NameHue, HueShift);
	const bool bSat = Instance->SetScalarParameterValue(NameSaturation, Saturation);
	const bool bBri = Instance->SetScalarParameterValue(NameBrightness, Brightness);
	const bool bGlow = Instance->SetScalarParameterValue(NameEmissive, Emissive);
	if (!(bHue && bSat && bBri && bGlow) && !bLoggedPushMiss)
	{
		bLoggedPushMiss = true;
		UE_LOG(LogTemp, Warning, TEXT("KodJadeite collection is missing a tuner scalar"));
	}
}
