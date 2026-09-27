#pragma once

#include "CommonActivatableWidget.h"
#include "Style/KodUITypes.h"
#include "KodUIScreenRoot.generated.h"

/**
 * Activatable root for one stamped screen.
 * Material language is fixed in the subclass constructor. Widget Blueprints cannot retarget it.
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodUIScreenRoot : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Kod|UI")
	EKodUIMaterialLanguage GetMaterialLanguage() const { return MaterialLanguage; }

	UFUNCTION(BlueprintPure, Category = "Kod|UI")
	EKodUIScreen GetScreen() const { return Screen; }

protected:
	virtual void NativePreConstruct() override;

	EKodUIMaterialLanguage MaterialLanguage = EKodUIMaterialLanguage::Ironstock;
	EKodUIScreen Screen = EKodUIScreen::Hud;
};
