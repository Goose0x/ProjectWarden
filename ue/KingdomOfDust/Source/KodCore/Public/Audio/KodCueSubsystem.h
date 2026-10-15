#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Sound/SoundBase.h"
#include "Subsystems/WorldSubsystem.h"
#include "KodCueSubsystem.generated.h"

/** One row of DefaultGame.ini. Missing files stay silent. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodCueBinding
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Kod|Audio")
	FName CueId;

	UPROPERTY(Config, EditAnywhere, Category = "Kod|Audio")
	FString Path;
};

/** Loaded sound or a recorded miss. The hot path only reads this map. */
USTRUCT()
struct FKodCachedCue
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USoundBase> Sound;

	FSoftObjectPath Path;
	bool bFailed = false;
	bool bLogged = false;
};

/**
 * Plays soft sound paths for sim cue ids.
 * Paths are async-preloaded at BeginPlay. A missing asset is logged once and then stays silent.
 */
UCLASS(Config = Game)
class KODCORE_API UKodCueSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION()
	void HandleCue(FName CueId, FVector Location, bool bAtLocation);

	void PlayCueNow(FName CueId, FVector Location, bool bAtLocation, bool bSkipAnnouncerGap);

	/**
	 * One line from the unit's shuffle bag for Category (Select, Ack, Attack, Annoyed).
	 * Select waits 1.5 s per entity. Six rapid selects play Annoyed when that bag exists.
	 * False when the category has no lines.
	 */
	bool PlayUnitVoice(FName UnitId, FName Category, int32 EntityId, FVector Location);

	/** Announcer lines and ids that start with vo_. Portrait talk uses this. */
	static bool IsVoiceCue(FName CueId);

	UFUNCTION()
	void OnCuesPreloaded();

	UPROPERTY(Config, EditAnywhere, Category = "Kod|Audio")
	TArray<FKodCueBinding> CuePaths;

	UPROPERTY(Config, EditAnywhere, Category = "Kod|Audio")
	float AnnouncerGapSeconds = 2.5f;

private:
	void RememberCue(FName CueId, const FString& Path);
	void NoteMissing(FName CueId, const FSoftObjectPath& Path);

	bool bBound = false;
	bool bPreloadFinished = false;
	TMap<FName, float> AnnouncerUntil;

	struct FVoiceClick
	{
		int32 Count = 0;
		float LastSeconds = -1000.f;
	};

	struct FVoiceBag
	{
		TArray<FName> Lines;
		TArray<int32> Order;
		int32 Cursor = 0;
		FName Last;
	};

	void DiscoverVoiceLines();
	FName PickVoiceLine(FName UnitId, FName Category);
	bool HasVoiceCategory(FName UnitId, FName Category) const;

	bool bVoiceDiscovered = false;
	TMap<FName, FVoiceBag> VoiceBags;
	TMap<int32, float> SelectVoiceUntil;
	TMap<int32, FVoiceClick> SelectClicks;
	FRandomStream VoiceStream;

	UPROPERTY()
	TMap<FName, FKodCachedCue> CueCache;

	FStreamableManager CueStream;
	TSharedPtr<FStreamableHandle> CueHandle;
};
