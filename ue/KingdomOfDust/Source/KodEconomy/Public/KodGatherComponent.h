#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KodGatherComponent.generated.h"

UCLASS(ClassGroup = (Kod), meta = (BlueprintSpawnableComponent))
class KODECONOMY_API UKodGatherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKodGatherComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Gather")
	int32 GatherAmount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Gather")
	float GatherIntervalSeconds = 1.f;

	UFUNCTION(BlueprintCallable, Category = "Kod|Gather")
	int32 GatherOnce();
};
