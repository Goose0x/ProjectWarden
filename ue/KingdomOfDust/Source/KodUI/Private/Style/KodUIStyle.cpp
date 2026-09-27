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
