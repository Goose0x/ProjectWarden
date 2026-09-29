#pragma once

#include "CoreMinimal.h"
#include "KodEntityId.generated.h"

/**
 * Opaque stable id for sim entities (units, buildings, projectiles later).
 * Decouples command payloads from raw Actor pointers for net-ready design.
 */
USTRUCT(BlueprintType)
struct KODCORE_API FKodEntityId
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod")
	int32 Value = 0;

	bool IsValid() const { return Value != 0; }

	bool operator==(const FKodEntityId& Other) const { return Value == Other.Value; }
	bool operator!=(const FKodEntityId& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FKodEntityId& Id)
	{
		return ::GetTypeHash(Id.Value);
	}
};
