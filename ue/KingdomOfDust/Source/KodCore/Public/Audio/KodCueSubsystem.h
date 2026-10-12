#pragma once

#include "CoreMinimal.h"
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

/**
 * Plays soft sound paths for sim cue ids.
 * 2D for UI and announcer, 3D when the sim passes a world location.
 * Announcer lines are throttled. A missing asset plays nothing.
 */
UCLASS(Config = Game)
class KODCORE_API UKodCueSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	UFUNCTION()
	void HandleCue(FName CueId, FVector Location, bool bAtLocation);

	UPROPERTY(Config, EditAnywhere, Category = "Kod|Audio")
	TArray<FKodCueBinding> CuePaths;

	UPROPERTY(Config, EditAnywhere, Category = "Kod|Audio")
	float AnnouncerGapSeconds = 2.5f;

private:
	bool bBound = false;
	TMap<FName, float> AnnouncerUntil;
};
