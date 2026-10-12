#pragma once

#include "CoreMinimal.h"
#include "GameFramework/WorldSettings.h"
#include "KodWorldSettings.generated.h"

/**
 * Optional tutorial switch. Set the map's World Settings class to
 * /Script/KingdomOfDust.KodWorldSettings and enable bFaultlineTraining.
 * Maps listed on AKodSlice0GameMode::FaultlineMaps match without this class.
 */
UCLASS()
class KINGDOMOFDUST_API AKodWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Map")
	bool bFaultlineTraining = false;
};
