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
	// Color-table HOLD. Ids stay valid. Do not return a faction palette.
	return GetIronstockAmber();
}

namespace KodUIStylePrivate
{
	EKodFactionAccentTheme& ActiveHudAccent()
	{
		static EKodFactionAccentTheme Theme = EKodFactionAccentTheme::USA_Ironstock;
		return Theme;
	}

	bool IsLegalHudAccent(EKodFactionAccentTheme Theme)
	{
		return Theme == EKodFactionAccentTheme::USA_Ironstock || Theme == EKodFactionAccentTheme::RSF_RustOrange;
	}
}

FLinearColor UKodUIStyleLibrary::GetHudAccentColor(EKodFactionAccentTheme Theme)
{
	if (!KodUIStylePrivate::IsLegalHudAccent(Theme))
	{
		UE_LOG(LogKodUI, Error, TEXT("HUD accent must be USA_Ironstock or RSF_RustOrange. Cyan glass is not a HUD theme."));
		return GetIronstockAmber();
	}
	// HOLD: no faction palette. USA_Ironstock and RSF_RustOrange both stay paint v3 amber.
	return GetIronstockAmber();
}

EKodFactionAccentTheme UKodUIStyleLibrary::GetActiveHudAccent()
{
	return KodUIStylePrivate::ActiveHudAccent();
}

bool UKodUIStyleLibrary::SetActiveHudAccent(EKodFactionAccentTheme Theme)
{
	if (!KodUIStylePrivate::IsLegalHudAccent(Theme))
	{
		UE_LOG(LogKodUI, Error, TEXT("Rejected HUD accent. Legal theme ids are USA_Ironstock and RSF_RustOrange. RU_RustIndustrial and CN_ImperialGreenGold are not ids."));
		return false;
	}
	KodUIStylePrivate::ActiveHudAccent() = Theme;
	return true;
}

bool UKodUIStyleLibrary::IsRejectedHudAccentId(FName AccentId)
{
	const FString Id = AccentId.ToString();
	// CONDITIONAL/FAIL. Not hidden theme ids, and not materials to author.
	const TCHAR* FailedSheets[] = {
		TEXT("RU_RustIndustrial"),
		TEXT("CN_ImperialGreenGold"),
		TEXT("RU"),
		TEXT("CN"),
	};
	for (const TCHAR* Name : FailedSheets)
	{
		if (Id.Equals(Name, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return !Id.Equals(TEXT("USA_Ironstock"), ESearchCase::IgnoreCase)
		&& !Id.Equals(TEXT("RSF_RustOrange"), ESearchCase::IgnoreCase);
}

FLinearColor UKodUIStyleLibrary::GetActiveHudAccentColor()
{
	return GetHudAccentColor(GetActiveHudAccent());
}

EKodFactionAccentTheme UKodUIStyleLibrary::AccentForFaction(EKodVersusFaction Faction)
{
	if (Faction == EKodVersusFaction::RSF)
	{
		return EKodFactionAccentTheme::RSF_RustOrange;
	}
	return EKodFactionAccentTheme::USA_Ironstock;
}
