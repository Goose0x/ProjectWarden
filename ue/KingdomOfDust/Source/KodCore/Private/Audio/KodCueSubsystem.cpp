#include "Audio/KodCueSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "Sim/KodSimSubsystem.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

bool UKodCueSubsystem::IsVoiceCue(FName CueId)
{
	const FString Name = CueId.ToString();
	return Name.StartsWith(TEXT("vo_"))
		|| Name.StartsWith(TEXT("unit_ready_"))
		|| Name.StartsWith(TEXT("not_enough_"))
		|| CueId == FName(TEXT("base_under_attack"))
		|| CueId == FName(TEXT("units_under_attack"))
		|| CueId == FName(TEXT("research_complete"))
		|| CueId == FName(TEXT("victory"))
		|| CueId == FName(TEXT("defeat"))
		|| CueId == FName(TEXT("structure_complete"));
}

namespace
{
	bool IsAnnouncerCue(FName CueId)
	{
		return UKodCueSubsystem::IsVoiceCue(CueId);
	}

	const TCHAR* KnownCueIds[] = {
		TEXT("unit_ready_amp"),
		TEXT("unit_ready_marine"),
		TEXT("unit_ready_medic"),
		TEXT("structure_complete"),
		TEXT("not_enough_credits"),
		TEXT("not_enough_luminene"),
		TEXT("not_enough_supply"),
		TEXT("base_under_attack"),
		TEXT("units_under_attack"),
		TEXT("research_complete"),
		TEXT("combat_stim"),
		TEXT("marine_fire"),
		TEXT("unit_death"),
		TEXT("building_placed"),
		TEXT("building_destroyed"),
		TEXT("victory"),
		TEXT("defeat"),
		TEXT("ui_click"),
		TEXT("cant_build"),
		TEXT("alert_ping"),
		TEXT("depot_raise_blocked"),
		TEXT("expand_or_be_exposed"),
		TEXT("exposed"),
		TEXT("artillery_strike"),
		TEXT("recon_sweep"),
		TEXT("resource_drop"),
		TEXT("command_move"),
		TEXT("command_attack"),
		TEXT("cheat_enabled"),
		TEXT("objective_complete")
	};
}

void UKodCueSubsystem::RememberCue(FName CueId, const FString& Path)
{
	if (CueId.IsNone() || CueCache.Contains(CueId))
	{
		return;
	}
	FKodCachedCue Entry;
	const FString UsePath = Path.IsEmpty()
		? FString::Printf(TEXT("/Game/Audio/TEMP/CoreLoop/%s.%s"), *CueId.ToString(), *CueId.ToString())
		: Path;
	Entry.Path = FSoftObjectPath(UsePath);
	CueCache.Add(CueId, Entry);
}

void UKodCueSubsystem::NoteMissing(FName CueId, const FSoftObjectPath& Path)
{
	FKodCachedCue& Entry = CueCache.FindOrAdd(CueId);
	Entry.Path = Path;
	Entry.bFailed = true;
	if (!Entry.bLogged)
	{
		Entry.bLogged = true;
		UE_LOG(LogTemp, Warning, TEXT("KodCue Missing Id=%s Path=%s"), *CueId.ToString(), *Path.ToString());
	}
}

void UKodCueSubsystem::Deinitialize()
{
	if (CueHandle.IsValid())
	{
		CueHandle->CancelHandle();
		CueHandle.Reset();
	}
	CueCache.Reset();
	VoiceBags.Reset();
	SelectVoiceUntil.Reset();
	SelectClicks.Reset();
	bVoiceDiscovered = false;
	bPreloadFinished = false;
	bBound = false;
	Super::Deinitialize();
}

void UKodCueSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!bBound)
	{
		if (UKodSimSubsystem* Sim = InWorld.GetSubsystem<UKodSimSubsystem>())
		{
			Sim->OnCue.AddDynamic(this, &UKodCueSubsystem::HandleCue);
			bBound = true;
		}
	}
	if (CueHandle.IsValid())
	{
		return;
	}
	for (const FKodCueBinding& Binding : CuePaths)
	{
		RememberCue(Binding.CueId, Binding.Path);
	}
	for (const TCHAR* Id : KnownCueIds)
	{
		RememberCue(FName(Id), FString());
	}
	TArray<FSoftObjectPath> Paths;
	for (const TPair<FName, FKodCachedCue>& Pair : CueCache)
	{
		Paths.Add(Pair.Value.Path);
	}
	if (Paths.Num() == 0)
	{
		bPreloadFinished = true;
		return;
	}
	CueHandle = CueStream.RequestAsyncLoad(
		Paths,
		FStreamableDelegate::CreateUObject(this, &UKodCueSubsystem::OnCuesPreloaded),
		0,
		false,
		false,
		TEXT("KodCues"));
}

void UKodCueSubsystem::OnCuesPreloaded()
{
	bPreloadFinished = true;
	for (TPair<FName, FKodCachedCue>& Pair : CueCache)
	{
		UObject* Loaded = Pair.Value.Path.ResolveObject();
		Pair.Value.Sound = Cast<USoundBase>(Loaded);
		if (!Pair.Value.Sound)
		{
			NoteMissing(Pair.Key, Pair.Value.Path);
		}
	}
}

void UKodCueSubsystem::HandleCue(FName CueId, FVector Location, bool bAtLocation)
{
	if (CueId.IsNone())
	{
		return;
	}
	if (const UWorld* World = GetWorld())
	{
		if (const UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
		{
			if (Sim->IsTestIsolation())
			{
				return;
			}
		}
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!bBound)
	{
		OnWorldBeginPlay(*World);
	}
	if (CueId == FName(TEXT("units_under_attack")) || CueId == FName(TEXT("base_under_attack")))
	{
		FTimerHandle VoiceTimer;
		World->GetTimerManager().SetTimer(
			VoiceTimer,
			FTimerDelegate::CreateUObject(this, &UKodCueSubsystem::PlayCueNow, CueId, Location, bAtLocation, true),
			0.45f,
			false);
		return;
	}
	PlayCueNow(CueId, Location, bAtLocation, false);
}

void UKodCueSubsystem::DiscoverVoiceLines()
{
	if (bVoiceDiscovered)
	{
		return;
	}
	bVoiceDiscovered = true;

	auto AddLine = [this](FName UnitId, FName Category, FName CueId, const FString& Path)
	{
		if (UnitId.IsNone() || Category.IsNone() || CueId.IsNone())
		{
			return;
		}
		VoiceDirector.AddLine(UnitId, Category, CueId);
		if (!Path.IsEmpty())
		{
			RememberCue(CueId, Path);
			if (bPreloadFinished)
			{
				FKodCachedCue& Entry = CueCache.FindOrAdd(CueId);
				if (!Entry.Sound)
				{
					Entry.Path = FSoftObjectPath(Path);
					Entry.Sound = LoadObject<USoundBase>(nullptr, *Path);
				}
			}
		}
	};
	auto CategoryOf = [](const FString& Raw) -> FName
	{
		if (Raw.Equals(TEXT("Select"), ESearchCase::IgnoreCase) || Raw.Equals(TEXT("Selected"), ESearchCase::IgnoreCase))
		{
			return FName(TEXT("Select"));
		}
		if (Raw.Equals(TEXT("Ack"), ESearchCase::IgnoreCase) || Raw.Equals(TEXT("Move"), ESearchCase::IgnoreCase) || Raw.Equals(TEXT("Yes"), ESearchCase::IgnoreCase))
		{
			return FName(TEXT("Ack"));
		}
		if (Raw.Equals(TEXT("Attack"), ESearchCase::IgnoreCase))
		{
			return FName(TEXT("Attack"));
		}
		if (Raw.Equals(TEXT("Annoyed"), ESearchCase::IgnoreCase) || Raw.Equals(TEXT("Pissed"), ESearchCase::IgnoreCase))
		{
			return FName(TEXT("Annoyed"));
		}
		return NAME_None;
	};
	auto UnitOf = [](const FString& Raw) -> FName
	{
		if (Raw.Equals(TEXT("AMP"), ESearchCase::IgnoreCase))
		{
			return FName(TEXT("Dozer"));
		}
		if (Raw.Equals(TEXT("Worker"), ESearchCase::IgnoreCase))
		{
			return FName(TEXT("SCN_WORKER"));
		}
		return FName(*Raw);
	};

	TArray<FString> ConfigLines;
	GConfig->GetArray(TEXT("KodVoice"), TEXT("Line"), ConfigLines, GGameIni);
	for (const FString& Text : ConfigLines)
	{
		FString Unit;
		FString Category;
		FString Cue;
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), false);
		for (const FString& Part : Parts)
		{
			FString Key;
			FString Value;
			if (!Part.Split(TEXT("="), &Key, &Value))
			{
				continue;
			}
			Key.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();
			if (Key.Equals(TEXT("Unit"), ESearchCase::IgnoreCase))
			{
				Unit = Value;
			}
			else if (Key.Equals(TEXT("Category"), ESearchCase::IgnoreCase))
			{
				Category = Value;
			}
			else if (Key.Equals(TEXT("Cue"), ESearchCase::IgnoreCase))
			{
				Cue = Value;
			}
		}
		AddLine(UnitOf(Unit), CategoryOf(Category), FName(*Cue), FString());
	}

	if (FModuleManager::Get().ModuleExists(TEXT("AssetRegistry")))
	{
		FAssetRegistryModule& Assets = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& Registry = Assets.Get();
		TArray<FString> Roots;
		Roots.Add(TEXT("/Game/Warden"));
		Roots.Add(TEXT("/Game/Audio/Voice"));
		Registry.ScanPathsSynchronous(Roots, true);
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add(TEXT("/Game/Warden"));
		Filter.PackagePaths.Add(TEXT("/Game/Audio/Voice"));
		Filter.ClassPaths.Add(USoundWave::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(USoundCue::StaticClass()->GetClassPathName());
		TArray<FAssetData> Found;
		Registry.GetAssets(Filter, Found);
		Found.Sort([](const FAssetData& A, const FAssetData& B)
		{
			return A.AssetName.LexicalLess(B.AssetName);
		});
		for (const FAssetData& Asset : Found)
		{
			const FString AssetPath = Asset.GetSoftObjectPath().ToString();
			if (!AssetPath.Contains(TEXT("/Voice/")))
			{
				continue;
			}
			TArray<FString> Segments;
			Asset.PackagePath.ToString().ParseIntoArray(Segments, TEXT("/"), true);
			FName UnitId;
			FName Category;
			for (int32 Index = 1; Index < Segments.Num(); ++Index)
			{
				const FName Maybe = CategoryOf(Segments[Index]);
				if (!Maybe.IsNone())
				{
					Category = Maybe;
					UnitId = UnitOf(Segments[Index - 1]);
					break;
				}
			}
			AddLine(UnitId, Category, Asset.AssetName, AssetPath);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("KodVoice Discovered"));
}

bool UKodCueSubsystem::PlayUnitVoice(FName UnitId, FName Category, int32 EntityId, FVector Location)
{
	if (UnitId.IsNone() || Category.IsNone())
	{
		return false;
	}
	DiscoverVoiceLines();
	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	const FKodVoiceChoice Choice = VoiceDirector.Choose(UnitId, Category, EntityId, Now);
	if (!Choice.bPlay)
	{
		return false;
	}
	PlayCueNow(Choice.Cue, Location, true, true);
	UE_LOG(LogTemp, Log, TEXT("KodVoice Unit=%s Category=%s Cue=%s Entity=%d"),
		*UnitId.ToString(),
		*Choice.Category.ToString(),
		*Choice.Cue.ToString(),
		EntityId);
	return true;
}

void UKodCueSubsystem::PlayCueNow(FName CueId, FVector Location, bool bAtLocation, bool bSkipAnnouncerGap)
{
	UWorld* World = GetWorld();
	if (!World || CueId.IsNone())
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (!bSkipAnnouncerGap && IsAnnouncerCue(CueId))
	{
		if (const float* Until = AnnouncerUntil.Find(CueId))
		{
			if (Now < *Until)
			{
				return;
			}
		}
		AnnouncerUntil.Add(CueId, Now + FMath::Max(0.1f, AnnouncerGapSeconds));
	}

	FKodCachedCue* Cached = CueCache.Find(CueId);
	const bool bUiFallback = CueId == FName(TEXT("alert_ping")) || CueId == FName(TEXT("objective_complete"));
	if (bUiFallback && (!Cached || !Cached->Sound))
	{
		if (FKodCachedCue* Click = CueCache.Find(FName(TEXT("ui_click"))))
		{
			if (Click->Sound)
			{
				UGameplayStatics::PlaySound2D(this, Click->Sound);
			}
		}
		return;
	}
	if (!Cached)
	{
		const FString Fallback = FString::Printf(TEXT("/Game/Audio/TEMP/CoreLoop/%s.%s"), *CueId.ToString(), *CueId.ToString());
		NoteMissing(CueId, FSoftObjectPath(Fallback));
		return;
	}
	if (!Cached->Sound)
	{
		if (bPreloadFinished)
		{
			NoteMissing(CueId, Cached->Path);
		}
		return;
	}
	if (bAtLocation)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Cached->Sound, Location);
	}
	else
	{
		UGameplayStatics::PlaySound2D(this, Cached->Sound);
	}
}
