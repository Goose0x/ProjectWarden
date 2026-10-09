#include "Game/KodResourceReadoutWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Game/KodPlayerState.h"
#include "Sim/KodResourceTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr int32 SupplyPlaceholderCurrent = 0;
	constexpr int32 SupplyPlaceholderMax = 10;

	const FLinearColor JadeiteSwatch(0.15f, 0.82f, 0.58f, 1.f);
	const FLinearColor OilSwatch(0.28f, 0.20f, 0.10f, 1.f);
	const FLinearColor NumberColor(0.95f, 0.95f, 0.93f, 1.f);
	const FLinearColor SupplyColor(0.45f, 0.45f, 0.45f, 0.9f);

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

	UTextBlock* MakeNumber(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color, int32 Size)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size));
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
	RefreshFromSim();
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

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResourceCanvas"));
	WidgetTree->RootWidget = Canvas;

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ResourceRow"));
	Row->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Row))
	{
		CanvasSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		CanvasSlot->SetAlignment(FVector2D(1.f, 0.f));
		CanvasSlot->SetPosition(FVector2D(-20.f, 14.f));
		CanvasSlot->SetAutoSize(true);
	}

	AddChip(Row, MakeSwatch(WidgetTree, JadeiteSwatch, TEXT("JadeiteSwatch")), 6.f);
	JadeiteText = MakeNumber(WidgetTree, TEXT("JadeiteValue"), NumberColor, 16);
	JadeiteText->SetText(FText::AsNumber(0));
	AddChip(Row, JadeiteText, 22.f);

	AddChip(Row, MakeSwatch(WidgetTree, OilSwatch, TEXT("OilSwatch")), 6.f);
	OilText = MakeNumber(WidgetTree, TEXT("OilValue"), NumberColor, 16);
	OilText->SetText(FText::AsNumber(0));
	AddChip(Row, OilText, 22.f);

	UTextBlock* Supply = MakeNumber(WidgetTree, TEXT("SupplyValue"), SupplyColor, 14);
	Supply->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), SupplyPlaceholderCurrent, SupplyPlaceholderMax)));
	AddChip(Row, Supply, 0.f);
}

void UKodResourceReadoutWidget::RefreshFromSim()
{
	if (!JadeiteText || !OilText)
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
	int32 Oil = 0;
	if (const UWorld* World = GetWorld())
	{
		if (const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			FKodResourceCost Bank;
			if (Sim->TryGetBank(TeamId, Bank))
			{
				Jadeite = Bank.Jadeite;
				Oil = Bank.Oil;
			}
		}
	}

	if (Jadeite == ShownJadeite && Oil == ShownOil)
	{
		return;
	}
	ShownJadeite = Jadeite;
	ShownOil = Oil;
	JadeiteText->SetText(FText::AsNumber(Jadeite));
	OilText->SetText(FText::AsNumber(Oil));
}
