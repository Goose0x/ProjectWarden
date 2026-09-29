#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "KodMinimapWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class KODUI_API UKodMinimapWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void PingWorldLocation(FVector WorldLocation);
};
