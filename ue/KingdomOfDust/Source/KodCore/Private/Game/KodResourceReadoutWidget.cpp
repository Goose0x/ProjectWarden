#include "Game/KodResourceReadoutWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Fonts/SlateFontInfo.h"
#include "Game/KodPlayerState.h"
#include "Sim/KodResourceTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr int32 SupplyPlaceholderCurrent = 0;
	constexpr int32 SupplyPlaceholderMax = 10;

	const FLinearColor SupplyColor(0.72f, 0.72f, 0.72f, 1.f);

	UImage* MakeSwatch(UWidgetTree* Tree, const FLinearColor& Tint, const FName& Name)
	{
		UImage* Swatch = Tree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		FSlateBrush Brush;
		if (const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		{
			Brush = *White;
			Brush.DrawAs = ESlateBrushDrawType::Image;
		}
		else
		{
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.TintColor = FSlateColor(FLinearColor::White);
		}
		Brush.ImageSize = FVector2D(14.f, 14.f);
		Swatch->SetBrush(Brush);
		Swatch->SetColorAndOpacity(Tint);
		Swatch->SetDesiredSizeOverride(FVector2D(14.f, 14.f));
		Swatch->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Swatch;
	}

	FSlateFontInfo MakeEngineFont(int32 Size)
	{
		if (GEngine)
		{
			if (UFont* EngineFont = GEngine->GetMediumFont())
			{
				return FSlateFontInfo(EngineFont, Size);
			}
		}
		return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
	}

	UTextBlock* MakeNumber(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color, int32 Size)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetFont(MakeEngineFont(Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Text;
	}

	void AddChip(UHorizontalBox* Row, UWidget* Widget, float RightPadding)
	{
		if (UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Widget))
		{
			Slot->SetPadding(FMargin(0.f, 0.f, RightPadding, 0.f));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
	}
}

void UKodResourceReadoutWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	BuildTree();
}

void UKodResourceReadoutWidget::SetDisplayed(int32 Jadeite, int32 Luminene)
{
	if (!IsReadoutBuilt())
	{
		BuildTree();
	}
	if (!JadeiteText || !LumineneText)
	{
		return;
	}
	if (Jadeite == ShownJadeite && Luminene == ShownLuminene)
	{
		return;
	}
	ShownJadeite = Jadeite;
	ShownLuminene = Luminene;
	JadeiteText->SetText(FText::FromString(FString::Printf(TEXT("Credits %d"), Jadeite)));
	LumineneText->SetText(FText::FromString(FString::Printf(TEXT("Luminene %d"), Luminene)));
}

void UKodResourceReadoutWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFromSim();
}

void UKodResourceReadoutWidget::PullFromSim()
{
	RefreshFromSim();
}

void UKodResourceReadoutWidget::BuildTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	// Root is a border, not a canvas. A canvas desired-size is 0, so AddToViewport
	// used to place an empty widget and the Director saw no tracker.
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ReadoutFrame"));
	Frame->SetPadding(FMargin(12.f, 8.f));
	Frame->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.88f));
	Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Frame;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ResourceRow"));
	Row->SetVisibility(ESlateVisibility::HitTestInvisible);
	Frame->SetContent(Row);

	AddChip(Row, MakeSwatch(WidgetTree, KodResourceColors::Jadeite(), TEXT("CreditsSwatch")), 6.f);
	JadeiteText = MakeNumber(WidgetTree, TEXT("CreditsValue"), KodResourceColors::Jadeite(), 16);
	JadeiteText->SetText(FText::FromString(TEXT("Credits 0")));
	AddChip(Row, JadeiteText, 22.f);

	AddChip(Row, MakeSwatch(WidgetTree, KodResourceColors::LumineneCore(), TEXT("LumineneSwatch")), 6.f);
	LumineneText = MakeNumber(WidgetTree, TEXT("LumineneValue"), KodResourceColors::LumineneHighlight(), 16);
	LumineneText->SetText(FText::FromString(TEXT("Luminene 0")));
	AddChip(Row, LumineneText, 22.f);

	UTextBlock* Supply = MakeNumber(WidgetTree, TEXT("SupplyValue"), SupplyColor, 14);
	Supply->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), SupplyPlaceholderCurrent, SupplyPlaceholderMax)));
	AddChip(Row, Supply, 0.f);
}

void UKodResourceReadoutWidget::RefreshFromSim()
{
	if (!JadeiteText || !LumineneText)
	{
		return;
	}

	int32 TeamId = 0;
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const AKodPlayerState* KodPS = PC->GetPlayerState<AKodPlayerState>())
		{
			TeamId = KodPS->TeamId;
		}
	}

	int32 Jadeite = 0;
	int32 Luminene = 0;
	if (const UWorld* World = GetWorld())
	{
		if (const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			FKodResourceCost Bank;
			if (Sim->TryGetBank(TeamId, Bank))
			{
				Jadeite = Bank.Jadeite;
				Luminene = Bank.Luminene;
			}
		}
	}

	if (Jadeite == ShownJadeite && Luminene == ShownLuminene)
	{
		return;
	}
	SetDisplayed(Jadeite, Luminene);
}
