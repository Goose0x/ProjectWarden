#include "Game/KodHUD.h"

#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "Sim/KodResourceTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	FName PlayerFacingName(const UKodSimSubsystem* Sim, FName EngineId)
	{
		FName Label = EngineId;
		FKodTrainSpec Spec;
		if (Sim && Sim->TryGetTrainSpec(EngineId, Spec) && !Spec.DisplayId.IsNone())
		{
			Label = Spec.DisplayId;
		}
		if (Label == FName(KodWardenPaths::Id_Dozer) || EngineId == FName(KodWardenPaths::Id_Dozer))
		{
			return FName(TEXT("AMP"));
		}
		if (Label == FName(KodWardenPaths::Id_Ranger) || EngineId == FName(KodWardenPaths::Id_Ranger))
		{
			return FName(TEXT("Marine"));
		}
		return Label;
	}
}

AKodHUD::AKodHUD()
{
	bShowHUD = true;
}

void AKodHUD::DrawHUD()
{
	Super::DrawHUD();
	bShowHUD = true;

	if (!bLoggedFirstDraw)
	{
		bLoggedFirstDraw = true;
		const float SizeX = Canvas ? Canvas->SizeX : 0.f;
		const float SizeY = Canvas ? Canvas->SizeY : 0.f;
		UE_LOG(LogTemp, Log, TEXT("KodHUD DrawHUD First SizeX=%.0f SizeY=%.0f"), SizeX, SizeY);
	}

	CacheSimSnapshot();
	RefreshReadout();
	DrawProductionPanel();
	DrawCommandCard();
	DrawMinimap();
	DrawIdleWorkerButton();
	DrawControlGroupTabs();
	DrawPortrait();
	DrawSelectionPanel();
	DrawResourceFlash();
	DrawMatchOutcome();
	DrawUnitHealthBars();
	DrawSelectedResourceNodes();
	DrawSelectionMarquee();
	DrawHotkeyLegend();
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
		int32 SupplyUsed = 0;
		int32 SupplyCap = 0;
		if (const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr)
		{
			int32 TeamId = 0;
			if (const APlayerController* PC = GetOwningPlayerController())
			{
				if (const AKodPlayerState* KodPS = PC->GetPlayerState<AKodPlayerState>())
				{
					TeamId = KodPS->TeamId;
				}
			}
			Sim->TryGetSupply(TeamId, SupplyUsed, SupplyCap);
		}
		if (Jadeite != ShownJadeite || Luminene != ShownLuminene)
		{
			ShownJadeite = Jadeite;
			ShownLuminene = Luminene;
			UE_LOG(LogTemp, Log, TEXT("KodHUD Readout Update J=%d L=%d"), Jadeite, Luminene);
		}
		ShownSupplyUsed = SupplyUsed;
		ShownSupplyCap = SupplyCap;
	}

	const int32 DrawJadeite = ShownJadeite == MIN_int32 ? 0 : ShownJadeite;
	const int32 DrawLuminene = ShownLuminene == MIN_int32 ? 0 : ShownLuminene;
	DrawCanvasReadout(DrawJadeite, DrawLuminene, ShownSupplyUsed, ShownSupplyCap);
}

void AKodHUD::DrawCanvasReadout(int32 Jadeite, int32 Luminene, int32 SupplyUsed, int32 SupplyCap)
{
	if (!Canvas)
	{
		return;
	}

	UFont* Font = nullptr;
	if (GEngine)
	{
		Font = GEngine->GetMediumFont();
		if (!Font)
		{
			Font = GEngine->GetLargeFont();
		}
		if (!Font)
		{
			Font = GEngine->GetSmallFont();
		}
	}

	const FString Credits = FString::Printf(TEXT("Credits %d"), Jadeite);
	const FString LumineneText = FString::Printf(TEXT("Luminene %d"), Luminene);
	bool bSupplyBlocked = SupplyCap > 0 && SupplyUsed >= SupplyCap;
	if (!bSupplyBlocked)
	{
		const int32 TeamId = ReadLocalTeamId();
		for (const FKodStructureState& Structure : FrameStructures)
		{
			if (Structure.TeamId == TeamId && Structure.bSupplyBlocked)
			{
				bSupplyBlocked = true;
				break;
			}
		}
	}
	const FString Supply = FString::Printf(TEXT("Supply %d/%d"), SupplyUsed, SupplyCap);
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
	DrawText(LumineneText, KodResourceColors::LumineneCore(), X + CreditsW + Gap, Y, Font, 1.f, false);
	const FLinearColor SupplyInk = bSupplyBlocked
		? FLinearColor(0.95f, 0.22f, 0.18f, 1.f)
		: FLinearColor(0.78f, 0.78f, 0.78f, 1.f);
	DrawText(Supply, SupplyInk, X + CreditsW + Gap + LumineneW + Gap, Y, Font, 1.f, false);
}

void AKodHUD::DrawProductionPanel()
{
	if (!Canvas)
	{
		return;
	}
	const AKodPlayerController* PC = Cast<AKodPlayerController>(GetOwningPlayerController());
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!PC || !Sim)
	{
		return;
	}

	FKodStructureState Structure;
	bool bFound = false;
	for (AActor* Actor : PC->GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		if (Sim->TryGetStructure(Id, Structure))
		{
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}
	const FString StructureName = Structure.DisplayId.IsNone() ? Structure.DefinitionId.ToString() : Structure.DisplayId.ToString();
	FString Status = StructureName;
	FKodSimEntityState Combat;
	if (Sim->TryGetState(Structure.Id, Combat) && Combat.MaxHealth > 0.f)
	{
		Status += FString::Printf(TEXT("   HP %.0f/%.0f"), Combat.Health, Combat.MaxHealth);
	}
	if (!Structure.bComplete && Structure.BuildTimeTotal > 0)
	{
		const int32 Percent = (Structure.BuildTicksDone * 100) / Structure.BuildTimeTotal;
		Status += FString::Printf(TEXT("   Build %d%%"), Percent);
	}
	DrawText(Status, FLinearColor::White, 24.f, 80.f, Font, 1.f, false);
	if (!Structure.bComplete || Structure.Trains.Num() <= 0)
	{
		return;
	}

	const float X = 24.f;
	float Y = 108.f;
	const FString TrainName = PlayerFacingName(Sim, Structure.Trains[0]).ToString();
	const FString Title = FString::Printf(TEXT("S Train %s  Backspace Cancel"), *TrainName);
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.82f), X - 8.f, Y - 8.f, 280.f, 36.f + 22.f * 5.f);
	DrawText(Title, FLinearColor::White, X, Y, Font, 1.f, false);
	Y += 22.f;
	if (Structure.bSupplyBlocked)
	{
		DrawText(TEXT("Supply blocked"), FLinearColor(0.9f, 0.25f, 0.2f, 1.f), X, Y, Font, 1.f, false);
		Y += 18.f;
	}
	for (int32 Index = 0; Index < Structure.Slots.Num(); ++Index)
	{
		const FKodProductionSlot& Slot = Structure.Slots[Index];
		const FString Name = PlayerFacingName(Sim, Slot.DisplayId.IsNone() ? Slot.UnitId : Slot.DisplayId).ToString();
		DrawText(Name, KodResourceColors::Jadeite(), X, Y, Font, 1.f, false);
		float Fraction = 0.f;
		if (Index == 0 && Slot.bStarted && Structure.ActiveBuildTime > 0)
		{
			Fraction = 1.f - (static_cast<float>(Structure.ActiveTicksRemaining) / static_cast<float>(Structure.ActiveBuildTime));
			Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
		}
		DrawRect(FLinearColor(0.15f, 0.15f, 0.15f, 1.f), X + 70.f, Y + 2.f, 140.f, 10.f);
		DrawRect(KodResourceColors::Jadeite(), X + 70.f, Y + 2.f, 140.f * Fraction, 10.f);
		Y += 20.f;
	}
}

void AKodHUD::DrawBuildHint()
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
	const FString Hint = PC->GetBuildMenuHint();
	if (Hint.IsEmpty())
	{
		return;
	}
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.82f), 16.f, 140.f, 720.f, 28.f);
	DrawText(Hint, KodResourceColors::Jadeite(), 24.f, 144.f, Font, 1.f, false);
}

int32 AKodHUD::ReadLocalTeamId() const
{
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		if (const AKodPlayerState* KodPS = PC->GetPlayerState<AKodPlayerState>())
		{
			return KodPS->TeamId;
		}
	}
	return 0;
}

FName AKodHUD::PeekResourceFail(int32 Credits, int32 Luminene, int32 Supply) const
{
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return NAME_None;
	}
	const int32 TeamId = ReadLocalTeamId();
	FKodResourceCost Bank;
	Sim->TryGetBank(TeamId, Bank);
	if (FMath::Max(0, Credits) > Bank.Jadeite)
	{
		return TEXT("Credits");
	}
	if (FMath::Max(0, Luminene) > Bank.Luminene)
	{
		return TEXT("Luminene");
	}
	if (Supply > 0)
	{
		int32 Used = 0;
		int32 Cap = 0;
		Sim->TryGetSupply(TeamId, Used, Cap);
		if (Used + Supply > Cap)
		{
			return TEXT("Supply");
		}
	}
	return NAME_None;
}

void AKodHUD::ShowResourceFlash(FName Reason)
{
	if (Reason == TEXT("Credits"))
	{
		FlashText = TEXT("Not enough Credits");
	}
	else if (Reason == TEXT("Luminene"))
	{
		FlashText = TEXT("Not enough Luminene");
	}
	else if (Reason == TEXT("Supply"))
	{
		FlashText = TEXT("Not enough supply");
	}
	else
	{
		return;
	}
	FlashUntilSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() + 1.6f : 0.f;
}

bool AKodHUD::GetCommandHit(float MouseX, float MouseY, FKodHudCommandHit& Out) const
{
	for (const FKodHudCommandHit& Hit : CommandHits)
	{
		if (MouseX >= Hit.MinX && MouseX <= Hit.MaxX && MouseY >= Hit.MinY && MouseY <= Hit.MaxY)
		{
			Out = Hit;
			Out.bInsideChrome = true;
			return true;
		}
	}
	if (bCommandChrome && MouseX >= ChromeMinX && MouseX <= ChromeMaxX && MouseY >= ChromeMinY && MouseY <= ChromeMaxY)
	{
		Out = FKodHudCommandHit();
		Out.bInsideChrome = true;
		return true;
	}
	return false;
}

void AKodHUD::DrawCommandCard()
{
	CommandHits.Reset();
	bCommandChrome = false;
	if (!Canvas)
	{
		return;
	}
	const AKodPlayerController* PC = Cast<AKodPlayerController>(GetOwningPlayerController());
	UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!PC || !Sim)
	{
		return;
	}

	TArray<FName> SubgroupIds;
	auto SubgroupOf = [Sim](AActor* Actor) -> FName
	{
		if (!Actor || !Sim)
		{
			return FName();
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		FKodStructureState Structure;
		if (Sim->TryGetStructure(Id, Structure))
		{
			return Structure.DefinitionId.IsNone() ? FName() : Structure.DefinitionId;
		}
		FKodSimEntityState State;
		if (Sim->TryGetState(Id, State))
		{
			return State.DefinitionId.IsNone() ? FName() : State.DefinitionId;
		}
		return FName();
	};
	for (AActor* Actor : PC->GetLocalSelection())
	{
		const FName Subgroup = SubgroupOf(Actor);
		if (!Subgroup.IsNone() && !SubgroupIds.Contains(Subgroup))
		{
			SubgroupIds.Add(Subgroup);
		}
	}
	SubgroupIds.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});
	CommandSubgroupCount = SubgroupIds.Num();
	const int32 SubgroupIndex = CommandSubgroupCount > 0 ? PC->GetCommandSubgroupIndex() % CommandSubgroupCount : 0;
	ActiveCommandSubgroup = CommandSubgroupCount > 0 ? SubgroupIds[SubgroupIndex] : FName();

	const FKodStructureState* Trainer = nullptr;
	FKodStructureState TrainerState;
	bool bWorker = false;
	bool bMarine = false;
	bool bStimReady = false;
	int32 StimRemaining = 0;
	int32 StimDuration = 1;
	for (AActor* Actor : PC->GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		if (CommandSubgroupCount > 1 && SubgroupOf(Actor) != ActiveCommandSubgroup)
		{
			continue;
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		FKodStructureState Structure;
		if (!Trainer && Sim->TryGetStructure(Id, Structure) && Structure.bComplete && Structure.Trains.Num() > 0)
		{
			TrainerState = Structure;
			Trainer = &TrainerState;
		}
		FKodSimEntityState State;
		if (!Sim->TryGetState(Id, State) || State.Health <= 0.f)
		{
			continue;
		}
		if (State.bCanGather)
		{
			bWorker = true;
		}
		if (Sim->UnitHasAbility(State.DefinitionId, FName(KodWardenPaths::Id_CombatStim)))
		{
			bMarine = true;
			StimRemaining = FMath::Max(StimRemaining, State.AbilityTicksRemaining);
			FKodAbilitySpec Ability;
			if (Sim->TryGetAbilitySpec(FName(KodWardenPaths::Id_CombatStim), Ability))
			{
				StimDuration = FMath::Max(1, Ability.DurationTicks);
				if (State.AbilityCooldownTicks <= 0 && State.Health > static_cast<float>(Ability.HealthCost))
				{
					bStimReady = true;
				}
			}
		}
	}

	const bool bBuildMenu = PC->IsWorkerBuildMenuOpen();
	const bool bTrainCard = Trainer != nullptr;
	const bool bBuildCard = !bTrainCard && bWorker;
	const bool bMarineCard = !bTrainCard && bMarine;
	if (!bTrainCard && !bBuildCard && !bMarineCard)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}

	constexpr float Button = 58.f;
	constexpr float Gap = 6.f;
	constexpr int32 Cols = 5;
	constexpr int32 Rows = 3;
	const float CardW = Cols * Button + (Cols - 1) * Gap;
	const float CardH = Rows * Button + (Rows - 1) * Gap;
	const float QueueH = bTrainCard ? 46.f : 0.f;
	const float OriginX = Canvas->SizeX - CardW - 24.f;
	const float OriginY = Canvas->SizeY - CardH - QueueH - 28.f;
	ChromeMinX = OriginX - 8.f;
	ChromeMinY = OriginY - 8.f;
	ChromeMaxX = OriginX + CardW + 8.f;
	ChromeMaxY = OriginY + CardH + QueueH + 8.f;
	bCommandChrome = true;
	DrawRect(FLinearColor(0.03f, 0.04f, 0.03f, 0.9f), ChromeMinX, ChromeMinY, ChromeMaxX - ChromeMinX, ChromeMaxY - ChromeMinY);

	auto AddButton = [this, Font, Button, Gap](int32 Col, int32 Row, float OriginXIn, float OriginYIn, const FString& Label, const FString& Hotkey, FName Action, FName Payload, int32 StructureId, int32 SlotIndex, bool bEnabled, FName Fail, const FString& Tooltip)
	{
		const float X = OriginXIn + Col * (Button + Gap);
		const float Y = OriginYIn + Row * (Button + Gap);
		const FLinearColor Plate = bEnabled
			? FLinearColor(0.08f, 0.16f, 0.1f, 1.f)
			: FLinearColor(0.12f, 0.12f, 0.12f, 1.f);
		const FLinearColor Ink = bEnabled ? FLinearColor::White : FLinearColor(0.45f, 0.45f, 0.45f, 1.f);
		DrawRect(Plate, X, Y, Button, Button);
		DrawText(Label, Ink, X + 6.f, Y + 8.f, Font, 0.8f, false);
		DrawText(Hotkey, KodResourceColors::LumineneCore(), X + Button - 16.f, Y + Button - 18.f, Font, 0.75f, false);
		FKodHudCommandHit Hit;
		Hit.bInsideChrome = true;
		Hit.bEnabled = bEnabled;
		Hit.Action = Action;
		Hit.Payload = Payload;
		Hit.SlotIndex = SlotIndex;
		Hit.StructureId = StructureId;
		Hit.FailReason = Fail;
		Hit.MinX = X;
		Hit.MinY = Y;
		Hit.MaxX = X + Button;
		Hit.MaxY = Y + Button;
		Hit.Tooltip = Tooltip;
		CommandHits.Add(Hit);
	};

	if (bTrainCard)
	{
		int32 Shown = 0;
		for (const FName& UnitId : Trainer->Trains)
		{
			if (Shown >= Cols)
			{
				break;
			}
			FKodTrainSpec Spec;
			const bool bHaveSpec = Sim->TryGetTrainSpec(UnitId, Spec);
			const FString Label = PlayerFacingName(Sim, (bHaveSpec && !Spec.DisplayId.IsNone()) ? Spec.DisplayId : UnitId).ToString();
			const int32 Credits = bHaveSpec ? Spec.Cost.Credits : 0;
			const int32 Luminene = bHaveSpec ? Spec.Cost.Luminene : 0;
			const int32 Supply = bHaveSpec ? Spec.Cost.Supply : 0;
			const int32 Ticks = bHaveSpec ? Spec.Cost.BuildTime : 0;
			FName Fail = bHaveSpec ? PeekResourceFail(Credits, Luminene, Supply) : FName();
			if (Fail.IsNone() && Trainer->bSupplyBlocked)
			{
				Fail = TEXT("Supply");
			}
			if (Fail.IsNone() && Trainer->Slots.Num() >= KodEconomyDefaults::ProductionSlotCap)
			{
				Fail = TEXT("Full");
			}
			const int32 Seconds = FMath::Max(0, Ticks) / 16;
			const FString Tooltip = FString::Printf(TEXT("%s\n%d Credits\n%d Luminene\n%d supply\n%ds"),
				*Label, Credits, Luminene, Supply, Seconds);
			AddButton(Shown, 0, OriginX, OriginY, Label, TEXT("S"), TEXT("Train"), UnitId, Trainer->Id.Value, INDEX_NONE, Fail.IsNone(), Fail, Tooltip);
			++Shown;
		}

		const float QueueY = OriginY + CardH + 8.f;
		for (int32 Slot = 0; Slot < KodEconomyDefaults::ProductionSlotCap; ++Slot)
		{
			const float X = OriginX + Slot * (Button + Gap);
			const bool bFilled = Slot < Trainer->Slots.Num();
			DrawRect(FLinearColor(0.08f, 0.08f, 0.08f, 1.f), X, QueueY, Button, 36.f);
			if (bFilled)
			{
				const FKodProductionSlot& Job = Trainer->Slots[Slot];
				const FString Name = PlayerFacingName(Sim, Job.DisplayId.IsNone() ? Job.UnitId : Job.DisplayId).ToString();
				DrawText(Name, FLinearColor::White, X + 4.f, QueueY + 2.f, Font, 0.7f, false);
				float Fraction = 0.f;
				if (Slot == 0 && Job.bStarted && Trainer->ActiveBuildTime > 0)
				{
					Fraction = 1.f - (static_cast<float>(Trainer->ActiveTicksRemaining) / static_cast<float>(Trainer->ActiveBuildTime));
					Fraction = FMath::Clamp(Fraction, 0.f, 1.f);
				}
				DrawRect(FLinearColor(0.15f, 0.15f, 0.15f, 1.f), X + 4.f, QueueY + 22.f, Button - 8.f, 8.f);
				DrawRect(KodResourceColors::Jadeite(), X + 4.f, QueueY + 22.f, (Button - 8.f) * Fraction, 8.f);
				FKodHudCommandHit Hit;
				Hit.bInsideChrome = true;
				Hit.bEnabled = true;
				Hit.Action = TEXT("CancelSlot");
				Hit.SlotIndex = Slot;
				Hit.StructureId = Trainer->Id.Value;
				Hit.MinX = X;
				Hit.MinY = QueueY;
				Hit.MaxX = X + Button;
				Hit.MaxY = QueueY + 36.f;
				Hit.Tooltip = TEXT("Cancel");
				CommandHits.Add(Hit);
			}
		}
	}
	else
	{
		if (bMarineCard)
		{
			FKodAbilitySpec Ability;
			const bool bHave = Sim->TryGetAbilitySpec(FName(KodWardenPaths::Id_CombatStim), Ability);
			const FString Label = (bHave && !Ability.DisplayName.IsNone()) ? Ability.DisplayName.ToString() : TEXT("Stim");
			const FString Hotkey = (bHave && !Ability.Hotkey.IsNone()) ? Ability.Hotkey.ToString() : TEXT("T");
			const int32 Seconds = (bHave ? Ability.DurationTicks : StimDuration) / 16;
			const FString Tooltip = FString::Printf(TEXT("%s\n%d HP\n%ds\n+50%% speed"), *Label, bHave ? Ability.HealthCost : 10, Seconds);
			const FName Fail = bStimReady ? FName() : FName(TEXT("Health"));
			AddButton(0, 0, OriginX, OriginY, Label, Hotkey, TEXT("Ability"), FName(KodWardenPaths::Id_CombatStim), 0, INDEX_NONE, bStimReady, Fail, Tooltip);
			if (StimRemaining > 0)
			{
				const float Fraction = FMath::Clamp(static_cast<float>(StimRemaining) / static_cast<float>(StimDuration), 0.f, 1.f);
				const float X = OriginX;
				const float Y = OriginY;
				DrawRect(FLinearColor(1.f, 0.45f, 0.1f, 0.45f), X, Y, Button, Button * Fraction);
			}
		}
		if (bWorker && !bBuildMenu)
		{
			AddButton(0, 2, OriginX, OriginY, TEXT("Build"), TEXT("B"), TEXT("BuildMenu"), NAME_None, 0, INDEX_NONE, true, NAME_None, TEXT("Build"));
		}
		else if (bWorker)
		{
			const FName Types[3] = {
				FName(KodWardenPaths::Id_SupplyDepot),
				FName(KodWardenPaths::Id_Barracks),
				FName(KodWardenPaths::Id_Rig)
			};
			const TCHAR* Keys[3] = { TEXT("D"), TEXT("R"), TEXT("E") };
			const TCHAR* Labels[3] = { TEXT("Depot"), TEXT("Barracks"), TEXT("Rig") };
			const int32 FirstCol = bMarineCard ? 1 : 0;
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FKodBuildSpec Spec;
				const bool bHave = Sim->TryGetBuildSpec(Types[Index], Spec);
				const int32 Credits = bHave ? Spec.Cost.Credits : 0;
				const int32 Luminene = bHave ? Spec.Cost.Luminene : 0;
				const int32 Ticks = bHave ? Spec.Cost.BuildTime : 0;
				const FName Fail = bHave ? PeekResourceFail(Credits, Luminene, 0) : FName();
				const FString Tooltip = FString::Printf(TEXT("%s\n%d Credits\n%d Luminene\n%ds"),
					Labels[Index], Credits, Luminene, FMath::Max(0, Ticks) / 16);
				AddButton(FirstCol + Index, 0, OriginX, OriginY, Labels[Index], Keys[Index], TEXT("Place"), Types[Index], 0, INDEX_NONE, Fail.IsNone(), Fail, Tooltip);
			}
		}
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (const APlayerController* Owning = GetOwningPlayerController())
	{
		if (Owning->GetMousePosition(MouseX, MouseY))
		{
			for (const FKodHudCommandHit& Hit : CommandHits)
			{
				if (Hit.Tooltip.IsEmpty() || MouseX < Hit.MinX || MouseX > Hit.MaxX || MouseY < Hit.MinY || MouseY > Hit.MaxY)
				{
					continue;
				}
				TArray<FString> Lines;
				Hit.Tooltip.ParseIntoArray(Lines, TEXT("\n"), true);
				const float TipH = FMath::Max(1, Lines.Num()) * 16.f + 10.f;
				DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.92f), MouseX + 16.f, MouseY - TipH - 8.f, 168.f, TipH);
				float TipY = MouseY - TipH - 2.f;
				for (const FString& Line : Lines)
				{
					DrawText(Line, FLinearColor::White, MouseX + 22.f, TipY, Font, 0.75f, false);
					TipY += 16.f;
				}
				break;
			}
		}
	}
}

void AKodHUD::DrawSelectionSummary()
{
	if (!Canvas)
	{
		return;
	}
	const AKodPlayerController* PC = Cast<AKodPlayerController>(GetOwningPlayerController());
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!PC || !Sim)
	{
		return;
	}
	TMap<FName, int32> Counts;
	for (AActor* Actor : PC->GetLocalSelection())
	{
		if (!Actor)
		{
			continue;
		}
		const FKodEntityId Id = Sim->FindIdForActor(Actor);
		FKodStructureState Structure;
		if (Sim->TryGetStructure(Id, Structure))
		{
			const FName Label = Structure.DisplayId.IsNone() ? Structure.DefinitionId : Structure.DisplayId;
			if (!Label.IsNone())
			{
				Counts.FindOrAdd(Label) += 1;
			}
			continue;
		}
		FKodSimEntityState State;
		if (!Sim->TryGetState(Id, State))
		{
			continue;
		}
		const FName Label = PlayerFacingName(Sim, State.DefinitionId);
		if (!Label.IsNone())
		{
			Counts.FindOrAdd(Label) += 1;
		}
	}
	if (Counts.Num() <= 0)
	{
		return;
	}
	TArray<FName> Keys;
	Counts.GetKeys(Keys);
	Keys.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });
	FString Line;
	for (const FName& Key : Keys)
	{
		if (!Line.IsEmpty())
		{
			Line += TEXT("    ");
		}
		Line += FString::Printf(TEXT("%d %s"), Counts[Key], *Key.ToString());
	}
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetSmallFont();
	}
	float Width = 0.f;
	float Height = 0.f;
	Canvas->TextSize(Font, Line, Width, Height);
	const float X = (Canvas->SizeX - Width) * 0.5f;
	const float Y = Canvas->SizeY - 40.f;
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.82f), X - 10.f, Y - 6.f, Width + 20.f, Height + 12.f);
	DrawText(Line, FLinearColor::White, X, Y, Font, 1.f, false);
}

void AKodHUD::DrawMatchOutcome()
{
	if (!Canvas)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const UKodSimSubsystem* Sim = World ? World->GetSubsystem<UKodSimSubsystem>() : nullptr;
	if (!Sim || !Sim->IsMatchOver())
	{
		return;
	}
	const bool bVictory = Sim->GetMatchOutcome() == EKodMatchOutcome::Victory;
	const FString Line = bVictory ? TEXT("VICTORY") : TEXT("DEFEAT");
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetMediumFont();
	}
	const float Scale = 2.f;
	float Width = 0.f;
	float Height = 0.f;
	Canvas->TextSize(Font, Line, Width, Height);
	Width *= Scale;
	Height *= Scale;
	const float X = (Canvas->SizeX - Width) * 0.5f;
	const float Y = Canvas->SizeY * 0.38f;
	const FLinearColor Ink = bVictory ? FLinearColor(0.55f, 0.9f, 0.45f, 1.f) : FLinearColor(0.95f, 0.28f, 0.22f, 1.f);
	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 0.9f), X - 24.f, Y - 12.f, Width + 48.f, Height + 24.f);
	DrawText(Line, Ink, X, Y, Font, Scale, false);
}

void AKodHUD::DrawResourceFlash()
{
	if (!Canvas || FlashText.IsEmpty())
	{
		return;
	}
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	if (Now > FlashUntilSeconds)
	{
		return;
	}
	UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	if (!Font && GEngine)
	{
		Font = GEngine->GetMediumFont();
	}
	float Width = 0.f;
	float Height = 0.f;
	Canvas->TextSize(Font, FlashText, Width, Height);
	const float X = (Canvas->SizeX - Width) * 0.5f;
	const float Y = Canvas->SizeY * 0.42f;
	DrawRect(FLinearColor(0.15f, 0.02f, 0.02f, 0.88f), X - 16.f, Y - 8.f, Width + 32.f, Height + 16.f);
	DrawText(FlashText, FLinearColor(1.f, 0.85f, 0.35f, 1.f), X, Y, Font, 1.f, false);
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

	const TArray<FKodSimEntityState>& States = FrameStates;
	const TArray<AActor*>& BarActors = FrameActors;

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
	bShowHUD = true;

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
