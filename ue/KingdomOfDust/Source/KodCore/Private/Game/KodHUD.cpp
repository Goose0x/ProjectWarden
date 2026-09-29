#include "Game/KodHUD.h"
#include "Blueprint/UserWidget.h"

AKodHUD::AKodHUD()
{
}

void AKodHUD::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (UClass* WidgetClass = HudWidgetClass.LoadSynchronous())
		{
			HudWidgetInstance = CreateWidget<UUserWidget>(PC, WidgetClass);
			if (HudWidgetInstance)
			{
				HudWidgetInstance->AddToViewport();
			}
		}
	}
}
