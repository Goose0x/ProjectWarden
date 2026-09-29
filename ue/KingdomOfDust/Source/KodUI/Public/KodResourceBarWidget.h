#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "KodResourceBarWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class KODUI_API UKodResourceBarWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetDustDisplay(int32 Amount);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 DisplayedDust = 0;
};
