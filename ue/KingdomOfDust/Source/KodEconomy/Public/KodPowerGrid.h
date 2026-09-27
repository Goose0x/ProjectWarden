#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "KodPowerGrid.generated.h"

/** Per-team power provided vs consumed. */
UCLASS(BlueprintType)
class KODECONOMY_API UKodPowerGrid : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Power")
	float PowerProvided = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Power")
	float PowerConsumed = 0.f;

	UFUNCTION(BlueprintCallable, Category = "Kod|Power")
	bool HasSufficientPower() const { return PowerProvided >= PowerConsumed; }

	UFUNCTION(BlueprintCallable, Category = "Kod|Power")
	float GetPowerDelta() const { return PowerProvided - PowerConsumed; }

	UFUNCTION(BlueprintCallable, Category = "Kod|Power")
	void Recalculate(float Provided, float Consumed);
};
