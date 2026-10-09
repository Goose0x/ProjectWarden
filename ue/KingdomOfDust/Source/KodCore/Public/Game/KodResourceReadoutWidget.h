#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KodResourceReadoutWidget.generated.h"

class UTextBlock;

/**
 * Top-right Credits / Luminene readout. Built in C++ (no Widget Blueprint).
 * Credits is the player-facing label for the Jadeite bank. The sim field stays Jadeite.
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
	TObjectPtr<UTextBlock> LumineneText;

	int32 ShownJadeite = MIN_int32;
	int32 ShownLuminene = MIN_int32;
};
