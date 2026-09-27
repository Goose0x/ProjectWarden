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
};
