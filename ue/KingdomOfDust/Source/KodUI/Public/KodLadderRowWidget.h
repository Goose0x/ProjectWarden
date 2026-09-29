#pragma once

#include "CommonUserWidget.h"
#include "Style/KodUITypes.h"
#include "KodLadderRowWidget.generated.h"

class UCommonTextBlock;

/**
 * One ladder line: RANK · NAME · MMR · W · L.
 * Local player row is the only orange accent in the list.
 * Parent WBP: /Game/UI/Lobby/WBP_LadderRow.
 */
UCLASS(Blueprintable)
class KODUI_API UKodLadderRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void ApplyRow(const FKodLadderRow& Row);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> RankText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> MmrText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> WinsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> LossesText;
};
