#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "KodHotkeyRouter.generated.h"

class UInputAction;
class AKodPlayerController;

/**
 * Maps Enhanced Input actions to selection / command / control-group operations.
 * Owned or referenced by AKodPlayerController.
 */
UCLASS(BlueprintType)
class KODUI_API UKodHotkeyRouter : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|Input")
	void BindToController(AKodPlayerController* PC);

	UFUNCTION(BlueprintCallable, Category = "Kod|Input")
	void OnControlGroupAssign(int32 GroupIndex);

	UFUNCTION(BlueprintCallable, Category = "Kod|Input")
	void OnControlGroupRecall(int32 GroupIndex);
};
