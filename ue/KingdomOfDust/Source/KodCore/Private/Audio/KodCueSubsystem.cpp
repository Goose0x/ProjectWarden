#include "Audio/KodCueSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Sim/KodSimSubsystem.h"

namespace
{
	bool IsAnnouncerCue(FName CueId)
	{
		const FString Name = CueId.ToString();
		return Name.StartsWith(TEXT("unit_ready_"))
			|| Name.StartsWith(TEXT("not_enough_"))
			|| CueId == FName(TEXT("base_under_attack"))
			|| CueId == FName(TEXT("units_under_attack"))
			|| CueId == FName(TEXT("research_complete"))
			|| CueId == FName(TEXT("victory"))
			|| CueId == FName(TEXT("defeat"))
			|| CueId == FName(TEXT("structure_complete"));
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
		TEXT("cant_build")
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
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	if (!bBound)
	{
		OnWorldBeginPlay(*World);
	}
	const float Now = World->GetTimeSeconds();
	if (IsAnnouncerCue(CueId))
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
