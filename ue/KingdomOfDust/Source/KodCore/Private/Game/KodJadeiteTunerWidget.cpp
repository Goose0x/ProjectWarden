#include "Game/KodJadeiteTunerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/KodJadeitePalette.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor PanelInk(0.04f, 0.07f, 0.08f, 0.94f);
	const FLinearColor TitleInk(0.56f, 0.92f, 0.89f, 1.f);
	const FLinearColor LabelInk(0.90f, 0.93f, 0.92f, 1.f);

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

	UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, const FString& Value, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetFont(MakeEngineFont(Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetText(FText::FromString(Value));
		Text->SetAutoWrapText(true);
		return Text;
	}

	UImage* MakeSwatch(UWidgetTree* Tree, const FName& Name)
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
		}
		Brush.ImageSize = FVector2D(28.f, 18.f);
		Swatch->SetBrush(Brush);
		Swatch->SetDesiredSizeOverride(FVector2D(28.f, 18.f));
		return Swatch;
	}

	void AddV(UVerticalBox* Column, UWidget* Widget, float Bottom)
	{
		if (UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Widget))
		{
			Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UKodJadeiteTunerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildTree();
	PullFromPalette();
	SetAnchorsInViewport(FAnchors(0.f, 0.f, 0.f, 0.f));
	SetAlignmentInViewport(FVector2D(0.f, 0.f));
	SetPositionInViewport(FVector2D(16.f, 88.f), false);
}

bool UKodJadeiteTunerWidget::IsCursorOverPanel() const
{
	if (!IsInViewport() || GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
	{
		return false;
	}
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}
	return GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

UKodJadeitePaletteSubsystem* UKodJadeiteTunerWidget::Palette() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UKodJadeitePaletteSubsystem>() : nullptr;
}

void UKodJadeiteTunerWidget::BuildTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("JadeiteFrame"));
	Frame->SetPadding(FMargin(12.f));
	Frame->SetBrushColor(PanelInk);
	Frame->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = Frame;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("JadeiteColumn"));
	Frame->SetContent(Column);

	AddV(Column, MakeText(WidgetTree, TEXT("JadeiteTitle"), TEXT("Jadeite   F9"), 16, TitleInk), 4.f);
	AddV(Column, MakeText(WidgetTree, TEXT("JadeiteHint"), TEXT("Campaign and offline only. Visual. Not saved in the match."), 10, LabelInk), 8.f);

	UHorizontalBox* Swatches = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("JadeiteSwatches"));
	ShadowSwatch = MakeSwatch(WidgetTree, TEXT("SwatchShadow"));
	BaseSwatch = MakeSwatch(WidgetTree, TEXT("SwatchBase"));
	LitSwatch = MakeSwatch(WidgetTree, TEXT("SwatchLit"));
	RimSwatch = MakeSwatch(WidgetTree, TEXT("SwatchRim"));
	CoreSwatch = MakeSwatch(WidgetTree, TEXT("SwatchCore"));
	UImage* SwatchList[] = { ShadowSwatch, BaseSwatch, LitSwatch, RimSwatch, CoreSwatch };
	for (UImage* Swatch : SwatchList)
	{
		if (UHorizontalBoxSlot* SwatchSlot = Swatches->AddChildToHorizontalBox(Swatch))
		{
			SwatchSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
	}
	AddV(Column, Swatches, 8.f);

	auto AddSlider = [this, Column](const FName& SliderName, const FName& LabelName, TObjectPtr<USlider>& OutSlider, TObjectPtr<UTextBlock>& OutLabel, float Min, float Max)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *(SliderName.ToString() + TEXT("Row")));
		OutLabel = MakeText(WidgetTree, LabelName, TEXT(""), 11, LabelInk);
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *(LabelName.ToString() + TEXT("Box")));
		LabelBox->SetWidthOverride(148.f);
		LabelBox->SetContent(OutLabel);
		if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelBox))
		{
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
		USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), SliderName);
		Slider->SetMinValue(Min);
		Slider->SetMaxValue(Max);
		Slider->SetStepSize(0.01f);
		OutSlider = Slider;
		if (UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider))
		{
			SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			SliderSlot->SetVerticalAlignment(VAlign_Center);
		}
		AddV(Column, Row, 6.f);
	};

	AddSlider(TEXT("HueSlider"), TEXT("HueLabel"), HueSlider, HueLabel, -0.5f, 0.5f);
	AddSlider(TEXT("SatSlider"), TEXT("SatLabel"), SaturationSlider, SaturationLabel, 0.f, 2.f);
	AddSlider(TEXT("BriSlider"), TEXT("BriLabel"), BrightnessSlider, BrightnessLabel, 0.f, 2.f);
	AddSlider(TEXT("GlowSlider"), TEXT("GlowLabel"), EmissiveSlider, EmissiveLabel, 0.f, 4.f);

	if (HueSlider)
	{
		HueSlider->OnValueChanged.AddDynamic(this, &UKodJadeiteTunerWidget::HandleHueChanged);
		HueSlider->OnMouseCaptureEnd.AddDynamic(this, &UKodJadeiteTunerWidget::HandleSliderCaptureEnd);
	}
	if (SaturationSlider)
	{
		SaturationSlider->OnValueChanged.AddDynamic(this, &UKodJadeiteTunerWidget::HandleSaturationChanged);
		SaturationSlider->OnMouseCaptureEnd.AddDynamic(this, &UKodJadeiteTunerWidget::HandleSliderCaptureEnd);
	}
	if (BrightnessSlider)
	{
		BrightnessSlider->OnValueChanged.AddDynamic(this, &UKodJadeiteTunerWidget::HandleBrightnessChanged);
		BrightnessSlider->OnMouseCaptureEnd.AddDynamic(this, &UKodJadeiteTunerWidget::HandleSliderCaptureEnd);
	}
	if (EmissiveSlider)
	{
		EmissiveSlider->OnValueChanged.AddDynamic(this, &UKodJadeiteTunerWidget::HandleEmissiveChanged);
		EmissiveSlider->OnMouseCaptureEnd.AddDynamic(this, &UKodJadeiteTunerWidget::HandleSliderCaptureEnd);
	}

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("JadeiteButtons"));
	UButton* ResetButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ResetButton"));
	ResetButton->AddChild(MakeText(WidgetTree, TEXT("ResetCaption"), TEXT("Reset"), 12, LabelInk));
	ResetButton->OnClicked.AddDynamic(this, &UKodJadeiteTunerWidget::HandleReset);
	UButton* CopyButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CopyButton"));
	CopyButton->AddChild(MakeText(WidgetTree, TEXT("CopyCaption"), TEXT("Copy hex"), 12, LabelInk));
	CopyButton->OnClicked.AddDynamic(this, &UKodJadeiteTunerWidget::HandleCopyHex);
	if (UHorizontalBoxSlot* ResetSlot = Buttons->AddChildToHorizontalBox(ResetButton))
	{
		ResetSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	Buttons->AddChildToHorizontalBox(CopyButton);
	AddV(Column, Buttons, 8.f);

	HexText = MakeText(WidgetTree, TEXT("HexReadout"), TEXT(""), 11, TitleInk);
	AddV(Column, HexText, 0.f);
}

void UKodJadeiteTunerWidget::PullFromPalette()
{
	const UKodJadeitePaletteSubsystem* Source = Palette();
	if (!Source)
	{
		return;
	}
	bMuteSlider = true;
	if (HueSlider)
	{
		HueSlider->SetValue(Source->GetHueShift());
	}
	if (SaturationSlider)
	{
		SaturationSlider->SetValue(Source->GetSaturation());
	}
	if (BrightnessSlider)
	{
		BrightnessSlider->SetValue(Source->GetBrightness());
	}
	if (EmissiveSlider)
	{
		EmissiveSlider->SetValue(Source->GetEmissive());
	}
	bMuteSlider = false;
	RefreshReadout();
}

void UKodJadeiteTunerWidget::RefreshReadout()
{
	const UKodJadeitePaletteSubsystem* Source = Palette();
	if (!Source)
	{
		return;
	}
	if (HueLabel)
	{
		HueLabel->SetText(FText::FromString(FString::Printf(TEXT("Hue  %+.2f"), Source->GetHueShift())));
	}
	if (SaturationLabel)
	{
		SaturationLabel->SetText(FText::FromString(FString::Printf(TEXT("Saturation  %.2f"), Source->GetSaturation())));
	}
	if (BrightnessLabel)
	{
		BrightnessLabel->SetText(FText::FromString(FString::Printf(TEXT("Brightness  %.2f"), Source->GetBrightness())));
	}
	if (EmissiveLabel)
	{
		EmissiveLabel->SetText(FText::FromString(FString::Printf(TEXT("Glow  %.2f"), Source->GetEmissive())));
	}
	if (ShadowSwatch)
	{
		ShadowSwatch->SetColorAndOpacity(Source->TunedShadow());
	}
	if (BaseSwatch)
	{
		BaseSwatch->SetColorAndOpacity(Source->TunedBase());
	}
	if (LitSwatch)
	{
		LitSwatch->SetColorAndOpacity(Source->TunedLit());
	}
	if (RimSwatch)
	{
		RimSwatch->SetColorAndOpacity(Source->TunedRim());
	}
	if (CoreSwatch)
	{
		CoreSwatch->SetColorAndOpacity(Source->TunedCore());
	}
	if (HexText)
	{
		HexText->SetText(FText::FromString(Source->FormatHex()));
	}
}

void UKodJadeiteTunerWidget::HandleHueChanged(float Value)
{
	if (bMuteSlider)
	{
		return;
	}
	if (UKodJadeitePaletteSubsystem* Source = Palette())
	{
		Source->SetHueShift(Value);
	}
	RefreshReadout();
}

void UKodJadeiteTunerWidget::HandleSaturationChanged(float Value)
{
	if (bMuteSlider)
	{
		return;
	}
	if (UKodJadeitePaletteSubsystem* Source = Palette())
	{
		Source->SetSaturation(Value);
	}
	RefreshReadout();
}

void UKodJadeiteTunerWidget::HandleBrightnessChanged(float Value)
{
	if (bMuteSlider)
	{
		return;
	}
	if (UKodJadeitePaletteSubsystem* Source = Palette())
	{
		Source->SetBrightness(Value);
	}
	RefreshReadout();
}

void UKodJadeiteTunerWidget::HandleEmissiveChanged(float Value)
{
	if (bMuteSlider)
	{
		return;
	}
	if (UKodJadeitePaletteSubsystem* Source = Palette())
	{
		Source->SetEmissive(Value);
	}
	RefreshReadout();
}

void UKodJadeiteTunerWidget::HandleSliderCaptureEnd()
{
	if (UKodJadeitePaletteSubsystem* Source = Palette())
	{
		Source->SetHueShift(Source->GetHueShift());
	}
	if (GConfig)
	{
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void UKodJadeiteTunerWidget::HandleReset()
{
	if (UKodJadeitePaletteSubsystem* Source = Palette())
	{
		Source->ResetDefaults();
	}
	PullFromPalette();
}

void UKodJadeiteTunerWidget::HandleCopyHex()
{
	const UKodJadeitePaletteSubsystem* Source = Palette();
	if (!Source)
	{
		return;
	}
	const FString Hex = Source->FormatHex();
	FPlatformApplicationMisc::ClipboardCopy(*Hex);
	if (HexText)
	{
		HexText->SetText(FText::FromString(Hex + TEXT("\nCopied")));
	}
}
