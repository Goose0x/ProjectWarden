#pragma once

#include "CoreMinimal.h"
#include "KodCommandTypes.h"
#include "KodCommandSerializers.generated.h"

/**
 * Future replication / replay helpers for FKodCommand.
 * M1: stubs only — local exec does not need net serialize yet.
 */
UCLASS()
class KODNET_API UKodCommandSerializers : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|Net")
	static bool SerializeCommandToBytes(const FKodCommand& Command, TArray<uint8>& OutBytes);

	UFUNCTION(BlueprintCallable, Category = "Kod|Net")
	static bool DeserializeCommandFromBytes(const TArray<uint8>& Bytes, FKodCommand& OutCommand);
};
