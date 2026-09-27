#pragma once

#include "CommonUserWidget.h"
#include "KodIdentityStripWidget.generated.h"

class UCommonTextBlock;
class UImage;

/**
 * Avatar · commander name · clan tag.
 * Clan tags are forced into [TAG] form. Main Menu omits LeagueLine; Lobby Versus shows it.
 * Parent WBP: /Game/UI/CommonUI/WBP_IdentityStrip.
 */
UCLASS(Blueprintable)
class KODUI_API UKodIdentityStripWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UKodIdentityStripWidget();

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetIdentity(const FText& InCommanderName, const FText& InClanTag);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetLeagueLine(const FText& InLeagueLine);

	UFUNCTION(BlueprintPure, Category = "Kod|UI")
	static FText FormatClanTag(const FText& Tag);

protected:
	virtual void NativePreConstruct() override;
	void ApplyIdentity();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI")
	FText CommanderName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI")
	FText ClanTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI")
	FText LeagueLine;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI")
	TObjectPtr<UImage> Avatar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI")
	TObjectPtr<UCommonTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI")
	TObjectPtr<UCommonTextBlock> ClanTagText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI")
	TObjectPtr<UCommonTextBlock> LeagueLineText;
};
