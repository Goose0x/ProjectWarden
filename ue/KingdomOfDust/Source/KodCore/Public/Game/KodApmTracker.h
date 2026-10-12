#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KodApmTracker.generated.h"

/** Real-time command rate. Not part of the sim hash. */
UCLASS()
class KODCORE_API UKodApmTracker : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void NoteCommand();
	void GetApm(int32& OutAverage, int32& OutPeak) const;

private:
	TArray<float> CommandTimes;
	float StartedAt = -1.f;
};
