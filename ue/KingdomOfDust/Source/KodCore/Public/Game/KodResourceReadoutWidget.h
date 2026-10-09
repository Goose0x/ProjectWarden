#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KodResourceReadoutWidget.generated.h"

class UTextBlock;

/**
 * Top-right Jadeite / Oil readout. Built in C++ (no Widget Blueprint).
 * Reads the sim bank for the local team. Does not write sim state or the idle hash.
 * Supply is a grey placeholder (0/10) until population exists.
 */
UCLASS()
class KODCORE_API UKodResourceReadoutWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** HUD calls this every frame. C++ NativeTick is not scheduled unless a Blueprint implements Tick. */
	void PullFromSim();

private:
	void BuildTree();
	void RefreshFromSim();

	UPROPERTY()
	TObjectPtr<UTextBlock> JadeiteText;

	UPROPERTY()
	TObjectPtr<UTextBlock> OilText;

	int32 ShownJadeite = MIN_int32;
	int32 ShownOil = MIN_int32;
};
