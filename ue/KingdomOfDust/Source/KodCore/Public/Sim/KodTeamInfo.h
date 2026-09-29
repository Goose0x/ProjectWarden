#pragma once

#include "CoreMinimal.h"
#include "KodTeamInfo.generated.h"

USTRUCT(BlueprintType)
struct KODCORE_API FKodTeamInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod")
	int32 TeamId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod")
	FLinearColor TeamColor = FLinearColor::White;
};
