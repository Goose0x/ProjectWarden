#include "Game/KodHUD.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "Game/KodResourceReadoutWidget.h"
#include "Sim/KodResourceTypes.h"
#include "Sim/KodSimSubsystem.h"

AKodHUD::AKodHUD()
{
}

void AKodHUD::DrawHUD()
{
	Super::DrawHUD();
	EnsureReadout();
	RefreshReadout();
	DrawUnitHealthBars();
	DrawSelectedResourceNodes();
	DrawSelectionMarquee();
}

void AKodHUD::EnsureReadout()
{
	if (bReadoutCreateLogged && ResourceReadout && ResourceReadout->IsInViewport())
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !PC->IsLocalPlayerController())
	{
		return;
	}
	const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer || !LocalPlayer->ViewportClient)
	{
		return;
	}

	if (!ResourceReadout)
	{
		ResourceReadout = CreateWidget<UKodResourceReadoutWidget>(PC, UKodResourceReadoutWidget::StaticClass());
	}
	if (!ResourceReadout)
	{
		if (!bReadoutCreateLogged)
		{
			bReadoutCreateLogged = true;
			UE_LOG(LogTemp, Log, TEXT("KodHUD Readout Created Owner=%s"), *PC->GetName());
		}
		return;
	}

	ResourceReadout->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!ResourceReadout->IsInViewport())
	{
		ResourceReadout->AddToViewport(100);
	}
	if (UCanvasPanelSlot* ViewportSlot = Cast<UCanvasPanelSlot>(ResourceReadout->Slot))
	{
		ViewportSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		ViewportSlot->SetAlignment(FVector2D(1.f, 0.f));
		ViewportSlot->SetAutoSize(true);
		ViewportSlot->SetPosition(FVector2D(-24.f, 16.f));
		ViewportSlot->SetZOrder(100);
	}
	ResourceReadout->ForceLayoutPrepass();

	if (!bReadoutCreateLogged)
	{
		bReadoutCreateLogged = true;
		UE_LOG(LogTemp, Log, TEXT("KodHUD Readout Created Owner=%s"), *PC->GetName());
	}
}

bool AKodHUD::ReadTeamBank(int32& OutJadeite, int32& OutLuminene) const
{
	OutJadeite = 0;
	OutLuminene = 0;
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return false;
	}

	int32 TeamId = 0;
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		if (const AKodPlayerState* KodPS = PC->GetPlayerState<AKodPlayerState>())
		{
			TeamId = KodPS->TeamId;
		}
	}

	FKodResourceCost Bank;
	if (!Sim->TryGetBank(TeamId, Bank))
	{
		return false;
	}
	OutJadeite = Bank.Jadeite;
	OutLuminene = Bank.Luminene;
	return true;
}

void AKodHUD::RefreshReadout()
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const bool bDue = LastReadoutPollSeconds < 0.f || (Now - LastReadoutPollSeconds) >= 0.25f;
	if (bDue)
	{
		LastReadoutPollSeconds = Now;
		int32 Jadeite = 0;
		int32 Luminene = 0;
		ReadTeamBank(Jadeite, Luminene);
		if (Jadeite != ShownJadeite || Luminene != ShownLuminene)
		{
			ShownJadeite = Jadeite;
			ShownLuminene = Luminene;
			UE_LOG(LogTemp, Log, TEXT("KodHUD Readout Update J=%d L=%d"), Jadeite, Luminene);
			if (ResourceReadout)
			{
				ResourceReadout->SetDisplayed(Jadeite, Luminene);
			}
		}
	}

	const int32 DrawJadeite = ShownJadeite == MIN_int32 ? 0 : ShownJadeite;
	const int32 DrawLuminene = ShownLuminene == MIN_int32 ? 0 : ShownLuminene;
	const bool bWidgetOnScreen = ResourceReadout
		&& ResourceReadout->IsInViewport()
		&& ResourceReadout->IsReadoutBuilt();
	if (!bWidgetOnScreen)
	{
		DrawCanvasReadout(DrawJadeite, DrawLuminene);
	}
}

void AKodHUD::DrawCanvasReadout(int32 Jadeite, int32 Luminene)
{
	if (!Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}

	const FString Credits = FString::Printf(TEXT("Credits %d"), Jadeite);
	const FString LumineneText = FString::Printf(TEXT("Luminene %d"), Luminene);
	const FString Supply = TEXT("0/10");
	float CreditsW = 0.f;
	float CreditsH = 0.f;
	float LumineneW = 0.f;
	float LumineneH = 0.f;
	float SupplyW = 0.f;
	float SupplyH = 0.f;
	Canvas->TextSize(Font, Credits, CreditsW, CreditsH);
	Canvas->TextSize(Font, LumineneText, LumineneW, LumineneH);
	Canvas->TextSize(Font, Supply, SupplyW, SupplyH);

	const float Gap = 22.f;
	const float Pad = 10.f;
	const float TextH = FMath::Max3(CreditsH, LumineneH, SupplyH);
	const float TotalW = CreditsW + Gap + LumineneW + Gap + SupplyW;
	const float X = Canvas->SizeX - TotalW - 24.f;
	const float Y = 16.f;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.88f), X - Pad, Y - 6.f, TotalW + Pad * 2.f, TextH + 12.f);
	DrawText(Credits, KodResourceColors::Jadeite(), X, Y, Font, 1.f, false);
	DrawText(LumineneText, KodResourceColors::LumineneHighlight(), X + CreditsW + Gap, Y, Font, 1.f, false);
	DrawText(Supply, FLinearColor(0.72f, 0.72f, 0.72f, 1.f), X + CreditsW + Gap + LumineneW + Gap, Y, Font, 1.f, false);
}

void AKodHUD::DrawUnitHealthBars()
{
	if (!Canvas)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* OwningPC = GetOwningPlayerController();
	if (!World || !OwningPC)
	{
		return;
	}

	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	if (!Sim)
	{
		return;
	}

	TSet<const AActor*> Selected;
	if (const AKodPlayerController* KodPC = Cast<AKodPlayerController>(OwningPC))
	{
		for (const AActor* SelectedActor : KodPC->GetLocalSelection())
		{
			if (SelectedActor)
			{
				Selected.Add(SelectedActor);
			}
		}
	}

	TArray<FKodSimEntityState> States;
	TArray<AActor*> BarActors;
	Sim->CopyEntitySnapshot(States, BarActors);

	constexpr float BarWidth = 60.f;
	constexpr float BarHeight = 7.f;
	const int32 Count = FMath::Min(States.Num(), BarActors.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FKodSimEntityState& HpState = States[Index];
		AActor* BarActor = BarActors[Index];
		if (!BarActor || HpState.MaxHealth <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const bool bFull = HpState.Health >= HpState.MaxHealth - 0.5f;
		const bool bSelected = Selected.Contains(BarActor);
		if (bFull && !bSelected)
		{
			continue;
		}

		float ExtraZ = 120.f;
		if (const UCapsuleComponent* Capsule = BarActor->FindComponentByClass<UCapsuleComponent>())
		{
			ExtraZ = Capsule->GetScaledCapsuleHalfHeight() + 24.f;
		}
		const FVector Anchor = BarActor->GetActorLocation() + FVector(0.f, 0.f, ExtraZ);

		FVector2D Screen;
		if (!OwningPC->ProjectWorldLocationToScreen(Anchor, Screen, true))
		{
			continue;
		}

		const float Fraction = FMath::Clamp(HpState.Health / HpState.MaxHealth, 0.f, 1.f);
		const float Left = Screen.X - (BarWidth * 0.5f);
		const float Top = Screen.Y - BarHeight;
		const FLinearColor Background(0.04f, 0.04f, 0.04f, 0.85f);
		const FLinearColor Fill = (HpState.TeamId == 0)
			? FLinearColor(0.15f, 0.82f, 0.28f, 0.95f)
			: FLinearColor(0.86f, 0.12f, 0.1f, 0.95f);
		DrawRect(Background, Left, Top, BarWidth, BarHeight);
		DrawRect(Fill, Left, Top, BarWidth * Fraction, BarHeight);
	}
}

void AKodHUD::DrawSelectedResourceNodes()
{
	if (!Canvas)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* OwningPC = GetOwningPlayerController();
	if (!World || !OwningPC)
	{
		return;
	}

	const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	const AKodPlayerController* KodPC = Cast<AKodPlayerController>(OwningPC);
	if (!Sim || !KodPC)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}

	TArray<AActor*> LabelActors;
	for (AActor* SelectedActor : KodPC->GetLocalSelection())
	{
		if (!SelectedActor)
		{
			continue;
		}
		if (Sim->IsResourceNode(Sim->FindIdForActor(SelectedActor)))
		{
			LabelActors.AddUnique(SelectedActor);
		}
	}
	FHitResult CursorHit;
	if (OwningPC->GetHitResultUnderCursor(ECC_Pawn, true, CursorHit))
	{
		if (AActor* HoverActor = CursorHit.GetActor())
		{
			if (Sim->IsResourceNode(Sim->FindIdForActor(HoverActor)))
			{
				LabelActors.AddUnique(HoverActor);
			}
		}
	}

	for (AActor* SelectedActor : LabelActors)
	{
		if (!SelectedActor)
		{
			continue;
		}
		FKodResourceNodeState Node;
		if (!Sim->TryGetResourceNode(Sim->FindIdForActor(SelectedActor), Node))
		{
			continue;
		}

		const FBox Bounds = SelectedActor->GetComponentsBoundingBox(true);
		const FVector Anchor = Bounds.IsValid
			? FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z + 28.f)
			: SelectedActor->GetActorLocation() + FVector(0.f, 0.f, 160.f);
		FVector2D Screen;
		if (!OwningPC->ProjectWorldLocationToScreen(Anchor, Screen, true))
		{
			continue;
		}

		const FString Label = FString::Printf(TEXT("%s  %d"), KodResourceTypeName(Node.Type), Node.Remaining);
		const FLinearColor Color = (Node.Type == EKodResourceType::Luminene)
			? KodResourceColors::LumineneHighlight()
			: KodResourceColors::Jadeite();
		const float Width = 8.f * static_cast<float>(Label.Len()) + 10.f;
		DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.72f), Screen.X - 4.f, Screen.Y - 2.f, Width, 18.f);
		DrawText(Label, Color, Screen.X, Screen.Y, Font, 1.f, false);
	}
}

void AKodHUD::DrawSelectionMarquee()
{
	if (!Canvas)
	{
		return;
	}

	const AKodPlayerController* PC = Cast<AKodPlayerController>(GetOwningPlayerController());
	if (!PC)
	{
		return;
	}

	FVector2D Min;
	FVector2D Max;
	if (!PC->GetSelectionMarqueeRect(Min, Max))
	{
		return;
	}

	const float Width = Max.X - Min.X;
	const float Height = Max.Y - Min.Y;
	// Same orange as the selection tint. Canvas quad uses the engine white texture.
	const FLinearColor Fill(1.f, 0.42f, 0.05f, 0.18f);
	const FLinearColor Edge(1.f, 0.55f, 0.12f, 0.95f);
	DrawRect(Fill, Min.X, Min.Y, Width, Height);
	DrawLine(Min.X, Min.Y, Max.X, Min.Y, Edge, 2.f);
	DrawLine(Max.X, Min.Y, Max.X, Max.Y, Edge, 2.f);
	DrawLine(Max.X, Max.Y, Min.X, Max.Y, Edge, 2.f);
	DrawLine(Min.X, Max.Y, Min.X, Min.Y, Edge, 2.f);
}

void AKodHUD::BeginPlay()
{
	Super::BeginPlay();
	EnsureReadout();

	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (UClass* WidgetClass = HudWidgetClass.LoadSynchronous())
		{
			HudWidgetInstance = CreateWidget<UUserWidget>(PC, WidgetClass);
			if (HudWidgetInstance)
			{
				bool bHudRoot = false;
				for (UClass* Class = HudWidgetInstance->GetClass(); Class; Class = Class->GetSuperClass())
				{
					if (Class->GetFName() == FName(TEXT("KodHudRootWidget")))
					{
						bHudRoot = true;
						break;
					}
				}
				if (!bHudRoot)
				{
					UE_LOG(LogTemp, Warning, TEXT("AKodHUD: HudWidgetClass should derive from UKodHudRootWidget (/Game/UI/HUD/WBP_KodHUD)."));
				}
				HudWidgetInstance->AddToViewport();
				if (UFunction* ActivateWidget = HudWidgetInstance->FindFunction(TEXT("ActivateWidget")))
				{
					HudWidgetInstance->ProcessEvent(ActivateWidget, nullptr);
				}
			}
		}
	}
}
