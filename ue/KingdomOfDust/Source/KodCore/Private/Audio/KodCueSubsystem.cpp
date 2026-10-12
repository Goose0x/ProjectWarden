#include "Audio/KodCueSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Sim/KodSimSubsystem.h"
#include "Sound/SoundBase.h"

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
}

void UKodCueSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (bBound)
	{
		return;
	}
	if (UKodSimSubsystem* Sim = InWorld.GetSubsystem<UKodSimSubsystem>())
	{
		Sim->OnCue.AddDynamic(this, &UKodCueSubsystem::HandleCue);
		bBound = true;
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

	FString Path;
	for (const FKodCueBinding& Binding : CuePaths)
	{
		if (Binding.CueId == CueId)
		{
			Path = Binding.Path;
			break;
		}
	}
	if (Path.IsEmpty())
	{
		Path = FString::Printf(TEXT("/Game/Audio/TEMP/CoreLoop/%s.%s"), *CueId.ToString(), *CueId.ToString());
	}
	USoundBase* Sound = Cast<USoundBase>(FSoftObjectPath(Path).TryLoad());
	if (!Sound)
	{
		return;
	}
	if (bAtLocation)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	}
	else
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}
