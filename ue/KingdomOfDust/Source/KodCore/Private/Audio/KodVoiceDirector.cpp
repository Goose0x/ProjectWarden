#include "Audio/KodVoiceDirector.h"

bool FKodVoiceDirector::IsAltTake(FName Cue)
{
	return Cue.ToString().EndsWith(TEXT("_alt"));
}

void FKodVoiceDirector::AddLine(FName UnitId, FName Category, FName Cue)
{
	if (UnitId.IsNone() || Category.IsNone() || Cue.IsNone() || IsAltTake(Cue))
	{
		return;
	}
	FBag& Bag = Bags.FindOrAdd(Key(UnitId, Category));
	if (!Bag.Lines.Contains(Cue))
	{
		Bag.Lines.Add(Cue);
	}
}

bool FKodVoiceDirector::HasCategory(FName UnitId, FName Category) const
{
	const FBag* Bag = Bags.Find(Key(UnitId, Category));
	return Bag && Bag->Lines.Num() > 0;
}

FName FKodVoiceDirector::Key(FName UnitId, FName Category) const
{
	return FName(*FString::Printf(TEXT("%s|%s"), *UnitId.ToString(), *Category.ToString()));
}

float FKodVoiceDirector::CooldownFor(FName Category) const
{
	if (Category == FName(TEXT("Annoyed")))
	{
		return AnnoyedCooldown;
	}
	if (Category == FName(TEXT("Select")))
	{
		return SelectCooldown;
	}
	return AckCooldown;
}

FName FKodVoiceDirector::NextShuffled(FName UnitId, FName Category)
{
	if (!bSeeded)
	{
		Stream.Initialize(0x5C2A11);
		bSeeded = true;
	}
	FBag* Bag = Bags.Find(Key(UnitId, Category));
	if (!Bag || Bag->Lines.Num() == 0)
	{
		return NAME_None;
	}
	if (Bag->Order.Num() != Bag->Lines.Num() || Bag->Cursor >= Bag->Order.Num())
	{
		Bag->Lines.Sort([](const FName& A, const FName& B)
		{
			return A.LexicalLess(B);
		});
		Bag->Order.SetNum(Bag->Lines.Num());
		for (int32 Index = 0; Index < Bag->Order.Num(); ++Index)
		{
			Bag->Order[Index] = Index;
		}
		for (int32 Index = Bag->Order.Num() - 1; Index > 0; --Index)
		{
			const int32 Swap = Stream.RandRange(0, Index);
			Bag->Order.Swap(Index, Swap);
		}
		if (Bag->Lines.Num() > 1 && Bag->Lines[Bag->Order[0]] == Bag->Last)
		{
			Bag->Order.Swap(0, 1);
		}
		Bag->Cursor = 0;
	}
	const FName Cue = Bag->Lines[Bag->Order[Bag->Cursor]];
	++Bag->Cursor;
	Bag->Last = Cue;
	return Cue;
}

FKodVoiceChoice FKodVoiceDirector::Choose(FName UnitId, FName Category, int32 EntityId, float NowSeconds)
{
	FKodVoiceChoice Choice;
	Choice.Category = Category;
	if (UnitId.IsNone() || Category.IsNone())
	{
		return Choice;
	}

	bool bEscalate = false;
	FClicks* Click = nullptr;
	if (Category == FName(TEXT("Select")))
	{
		if (const float* Busy = BusyUntil.Find(UnitId))
		{
			const FClicks* Playing = Clicks.Find(EntityId);
			if (NowSeconds < *Busy && Playing && Playing->bAnnoyedLine)
			{
				return Choice;
			}
		}
		Click = &Clicks.FindOrAdd(EntityId);
		if (NowSeconds - Click->LastSeconds > AnnoyedWindow)
		{
			Click->Count = 0;
			Click->AnnoyedIndex = 0;
			Click->bAnnoyedLine = false;
		}
		++Click->Count;
		Click->LastSeconds = NowSeconds;
		bEscalate = Click->Count >= FMath::Max(1, AnnoyedAfter) && HasCategory(UnitId, FName(TEXT("Annoyed")));
	}
	if (const float* Until = BusyUntil.Find(UnitId))
	{
		if (NowSeconds < *Until)
		{
			return Choice;
		}
	}

	FName UseCategory = Category;
	FName Cue = NAME_None;
	if (bEscalate && Click)
	{
		UseCategory = FName(TEXT("Annoyed"));
		const FBag* Annoyed = Bags.Find(Key(UnitId, UseCategory));
		if (Annoyed && Annoyed->Lines.Num() > 0)
		{
			const int32 Index = FMath::Clamp(Click->AnnoyedIndex, 0, Annoyed->Lines.Num() - 1);
			Cue = Annoyed->Lines[Index];
			if (Click->AnnoyedIndex < Annoyed->Lines.Num() - 1)
			{
				++Click->AnnoyedIndex;
			}
			Click->bAnnoyedLine = true;
			Click->LastSeconds = NowSeconds + FMath::Max(0.f, AnnoyedCooldown);
		}
	}
	else
	{
		UseCategory = Category == FName(TEXT("Select")) ? FName(TEXT("Select")) : Category;
		Cue = NextShuffled(UnitId, UseCategory);
	}

	if (Cue.IsNone())
	{
		return Choice;
	}
	Choice.Cue = Cue;
	Choice.Category = UseCategory;
	Choice.bPlay = true;
	BusyUntil.Add(UnitId, NowSeconds + FMath::Max(0.f, CooldownFor(UseCategory)));
	return Choice;
}
