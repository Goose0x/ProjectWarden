#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Style/KodUITypes.h"
#include "Style/KodUIStyle.generated.h"

/**
 * Color tokens for Widget Blueprints until Editor materials exist.
 * Orange is a front-end commit color. The HUD never uses it, and never uses cyan glass.
 */
UCLASS()
class KODUI_API UKodUIStyleLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static EKodUIMaterialLanguage GetRequiredLanguage(EKodUIScreen Screen);

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static bool LanguageMatchesScreen(EKodUIMaterialLanguage Language, EKodUIScreen Screen);

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetIronstockMetal();

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetIronstockAmber();

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetHpGreen();

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetAlertRed();

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetCyanActive();

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetPanelBlue();

	/** Front-end primary commit only (PLAY VERSUS, PLAY RANKED). */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetOrangeCta();

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetWhiteText();

	/**
	 * Readout color for a screen. Ironstock returns amber.
	 * Cyan glass returns white text (cyan is for selection chrome, not body copy).
	 */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetReadoutColor(EKodUIMaterialLanguage Language);

	/**
	 * Commit tint. Legal on cyan-glass screens only.
	 * An Ironstock caller receives amber, and this logs an error.
	 */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetCommitColor(EKodUIMaterialLanguage Language);

	/** Retired rust swatch. Returns the held RSF proud orange, not a v3 stamp and not a warm-rust language. */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style", meta = (DeprecatedFunction, DeprecationMessage = "Use GetHudAccentColors(RSF_MetalStoneOrangeArch).Proud"))
	static FLinearColor GetIronstockRustOrange();

	/** Accent table. USA/RSF values are HOLD pending the next QA stamp, not a v3 or v4 land. CN/RU stay stamped v2. Proud, Frame, Field, and Mark stay separate. */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FKodHudAccentColors GetHudAccentColors(EKodFactionAccentTheme Theme);

	/** Icon and readout color for the language (the Proud role). */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetHudAccentColor(EKodFactionAccentTheme Theme);

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static EKodFactionAccentTheme GetActiveHudAccent();

	/** Accepts the four v2 languages. There is no cyan-glass theme. */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Style")
	static bool SetActiveHudAccent(EKodFactionAccentTheme Theme);

	/**
	 * Maps a theme name onto a v2 id.
	 * USA_Ironstock aliases USA_TanGreenGold. RSF_RustOrange aliases RSF_MetalStoneOrangeArch.
	 * CN_JadeStoneGoldRed and RU_SovietColdBlueIce resolve. Bare RU, CN, and the failed
	 * RU_RustIndustrial / CN_ImperialGreenGold sheets do not.
	 */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static bool TryResolveHudAccentId(FName AccentId, EKodFactionAccentTheme& OutTheme);

	/** True when TryResolveHudAccentId fails. Failed v1 sheet names stay rejected. */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static bool IsRejectedHudAccentId(FName AccentId);

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FKodHudAccentColors GetActiveHudAccentColors();

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static FLinearColor GetActiveHudAccentColor();

	/** Versus factions only. USA and RSF. CN and RU are not roster factions. */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Style")
	static EKodFactionAccentTheme AccentForFaction(EKodVersusFaction Faction);
};
