#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "KodCommandCardWidget.generated.h"

/**
 * Command card root (WBP_CommandCard). Buttons enqueue FKodCommand /
 * activate GAS powers based on current selection.
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodCommandCardWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void RefreshFromSelection();

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void OnCommandButtonClicked(FName CommandId);
};
