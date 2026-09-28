#include "Style/KodUIStyle.h"
#include "KodUI.h"

EKodUIMaterialLanguage UKodUIStyleLibrary::GetRequiredLanguage(EKodUIScreen Screen)
{
	switch (Screen)
	{
	case EKodUIScreen::Hud:
		return EKodUIMaterialLanguage::Ironstock;
	case EKodUIScreen::MainMenu:
	case EKodUIScreen::LobbyVersus:
		return EKodUIMaterialLanguage::CyanGlass;
	default:
		return EKodUIMaterialLanguage::CyanGlass;
	}
}

bool UKodUIStyleLibrary::LanguageMatchesScreen(EKodUIMaterialLanguage Language, EKodUIScreen Screen)
{
	return Language == GetRequiredLanguage(Screen);
}

FLinearColor UKodUIStyleLibrary::GetIronstockMetal()
{
	return FLinearColor(0.07f, 0.08f, 0.06f, 1.f);
}

FLinearColor UKodUIStyleLibrary::GetIronstockAmber()
{
	return FLinearColor(0.93f, 0.58f, 0.12f, 1.f);
}

FLinearColor UKodUIStyleLibrary::GetHpGreen()
{
	return FLinearColor(0.18f, 0.72f, 0.22f, 1.f);
}

FLinearColor UKodUIStyleLibrary::GetAlertRed()
{
	return FLinearColor(0.75f, 0.16f, 0.12f, 1.f);
}

FLinearColor UKodUIStyleLibrary::GetCyanActive()
{
	return FLinearColor(FColor(0x23, 0xAD, 0xFF, 0xFF));
}

FLinearColor UKodUIStyleLibrary::GetPanelBlue()
{
	return FLinearColor(0.02f, 0.07f, 0.12f, 0.55f);
}

FLinearColor UKodUIStyleLibrary::GetOrangeCta()
{
	return FLinearColor(0.96f, 0.42f, 0.05f, 1.f);
}

FLinearColor UKodUIStyleLibrary::GetWhiteText()
{
	return FLinearColor::White;
}

FLinearColor UKodUIStyleLibrary::GetReadoutColor(EKodUIMaterialLanguage Language)
{
	if (Language == EKodUIMaterialLanguage::Ironstock)
	{
		return GetIronstockAmber();
	}
	return GetWhiteText();
}

FLinearColor UKodUIStyleLibrary::GetCommitColor(EKodUIMaterialLanguage Language)
{
	if (Language != EKodUIMaterialLanguage::CyanGlass)
	{
		UE_LOG(LogKodUI, Error, TEXT("Orange commit tint is front-end only. HUD stays Ironstock amber."));
		return GetIronstockAmber();
	}
	return GetOrangeCta();
}

FLinearColor UKodUIStyleLibrary::GetIronstockRustOrange()
{
	return GetHudAccentColors(EKodFactionAccentTheme::RSF_MetalStoneOrangeArch).Proud;
}

namespace KodUIStylePrivate
{
	EKodFactionAccentTheme& ActiveHudAccent()
	{
		static EKodFactionAccentTheme Theme = EKodFactionAccentTheme::USA_TanGreenGold;
		return Theme;
	}

	bool IsLegalHudAccent(EKodFactionAccentTheme Theme)
	{
		switch (Theme)
		{
		case EKodFactionAccentTheme::USA_TanGreenGold:
		case EKodFactionAccentTheme::RSF_MetalStoneOrangeArch:
		case EKodFactionAccentTheme::CN_JadeStoneGoldRed:
		case EKodFactionAccentTheme::RU_SovietColdBlueIce:
			return true;
		default:
			return false;
		}
	}

	FKodHudAccentColors MakeColors(const FLinearColor& Metal, const FLinearColor& Frame, const FLinearColor& Field, const FLinearColor& Proud, const FLinearColor& Mark)
	{
		FKodHudAccentColors Colors;
		Colors.Metal = Metal;
		Colors.Frame = Frame;
		Colors.Field = Field;
		Colors.Proud = Proud;
		Colors.Mark = Mark;
		return Colors;
	}

	bool EqualsId(const FString& Id, const TCHAR* Name)
	{
		return Id.Equals(Name, ESearchCase::IgnoreCase);
	}
}

FKodHudAccentColors UKodUIStyleLibrary::GetHudAccentColors(EKodFactionAccentTheme Theme)
{
	using namespace KodUIStylePrivate;
	if (!IsLegalHudAccent(Theme))
	{
		UE_LOG(LogKodUI, Error, TEXT("HUD accent must be a legal theme id. Cyan glass is not a HUD theme."));
		Theme = EKodFactionAccentTheme::USA_TanGreenGold;
	}

	switch (Theme)
	{
	case EKodFactionAccentTheme::RSF_MetalStoneOrangeArch:
		// v3 PASS. Sandstone plate, adobe mortar, accent orange.
		// Not gothic black iron. Cause-safe: no faith chrome, no arch widget.
		return MakeColors(
			FLinearColor(0.38f, 0.28f, 0.18f, 1.f),
			FLinearColor(0.66f, 0.50f, 0.34f, 1.f),
			FLinearColor(0.74f, 0.58f, 0.40f, 1.f),
			FLinearColor(0.93f, 0.48f, 0.14f, 1.f),
			FLinearColor(0.93f, 0.48f, 0.14f, 1.f));
	case EKodFactionAccentTheme::CN_JadeStoneGoldRed:
		// HUD preview. Jade, stone, gold, and a mandate red. Not a Versus tile.
		return MakeColors(
			FLinearColor(0.08f, 0.12f, 0.09f, 1.f),
			FLinearColor(0.16f, 0.32f, 0.22f, 1.f),
			FLinearColor(0.24f, 0.48f, 0.32f, 1.f),
			FLinearColor(0.82f, 0.64f, 0.24f, 1.f),
			FLinearColor(0.62f, 0.14f, 0.12f, 1.f));
	case EKodFactionAccentTheme::RU_SovietColdBlueIce:
		// HUD preview. Cold steel and ice. Not warm rust, and not lobby cyan.
		return MakeColors(
			FLinearColor(0.10f, 0.13f, 0.16f, 1.f),
			FLinearColor(0.28f, 0.36f, 0.42f, 1.f),
			FLinearColor(0.18f, 0.28f, 0.36f, 1.f),
			FLinearColor(0.62f, 0.76f, 0.84f, 1.f),
			FLinearColor(0.62f, 0.76f, 0.84f, 1.f));
	case EKodFactionAccentTheme::USA_TanGreenGold:
	default:
		// v3 PASS. Desert sand plate, muted field green, patriotic gold.
		// Not olive and not dark burnt Ironstock. Star/stripe rhythm is material, not a widget.
		return MakeColors(
			FLinearColor(0.52f, 0.42f, 0.28f, 1.f),
			FLinearColor(0.80f, 0.68f, 0.46f, 1.f),
			FLinearColor(0.42f, 0.48f, 0.28f, 1.f),
			FLinearColor(0.95f, 0.78f, 0.30f, 1.f),
			FLinearColor(0.95f, 0.78f, 0.30f, 1.f));
	}
}

FLinearColor UKodUIStyleLibrary::GetHudAccentColor(EKodFactionAccentTheme Theme)
{
	return GetHudAccentColors(Theme).Proud;
}

EKodFactionAccentTheme UKodUIStyleLibrary::GetActiveHudAccent()
{
	return KodUIStylePrivate::ActiveHudAccent();
}

bool UKodUIStyleLibrary::SetActiveHudAccent(EKodFactionAccentTheme Theme)
{
	if (!KodUIStylePrivate::IsLegalHudAccent(Theme))
	{
		UE_LOG(LogKodUI, Error, TEXT("Rejected HUD accent. Legal ids are USA_TanGreenGold, RSF_MetalStoneOrangeArch, CN_JadeStoneGoldRed, and RU_SovietColdBlueIce."));
		return false;
	}
	KodUIStylePrivate::ActiveHudAccent() = Theme;
	return true;
}

bool UKodUIStyleLibrary::TryResolveHudAccentId(FName AccentId, EKodFactionAccentTheme& OutTheme)
{
	const FString Id = AccentId.ToString();
	using namespace KodUIStylePrivate;
	if (EqualsId(Id, TEXT("USA_TanGreenGold")) || EqualsId(Id, TEXT("USA_Ironstock")))
	{
		OutTheme = EKodFactionAccentTheme::USA_TanGreenGold;
		return true;
	}
	if (EqualsId(Id, TEXT("RSF_MetalStoneOrangeArch")) || EqualsId(Id, TEXT("RSF_RustOrange")))
	{
		OutTheme = EKodFactionAccentTheme::RSF_MetalStoneOrangeArch;
		return true;
	}
	if (EqualsId(Id, TEXT("CN_JadeStoneGoldRed")))
	{
		OutTheme = EKodFactionAccentTheme::CN_JadeStoneGoldRed;
		return true;
	}
	if (EqualsId(Id, TEXT("RU_SovietColdBlueIce")))
	{
		OutTheme = EKodFactionAccentTheme::RU_SovietColdBlueIce;
		return true;
	}
	return false;
}

bool UKodUIStyleLibrary::IsRejectedHudAccentId(FName AccentId)
{
	EKodFactionAccentTheme Resolved = EKodFactionAccentTheme::USA_TanGreenGold;
	if (TryResolveHudAccentId(AccentId, Resolved))
	{
		return false;
	}
	const FString Id = AccentId.ToString();
	if (KodUIStylePrivate::EqualsId(Id, TEXT("RU_RustIndustrial")) || KodUIStylePrivate::EqualsId(Id, TEXT("CN_ImperialGreenGold")))
	{
		UE_LOG(LogKodUI, Error, TEXT("HUD accent '%s' is a failed sheet. It is not a v2 theme id."), *Id);
	}
	return true;
}

FKodHudAccentColors UKodUIStyleLibrary::GetActiveHudAccentColors()
{
	return GetHudAccentColors(GetActiveHudAccent());
}

FLinearColor UKodUIStyleLibrary::GetActiveHudAccentColor()
{
	return GetHudAccentColor(GetActiveHudAccent());
}

EKodFactionAccentTheme UKodUIStyleLibrary::AccentForFaction(EKodVersusFaction Faction)
{
	if (Faction == EKodVersusFaction::RSF)
	{
		return EKodFactionAccentTheme::RSF_MetalStoneOrangeArch;
	}
	return EKodFactionAccentTheme::USA_TanGreenGold;
}
