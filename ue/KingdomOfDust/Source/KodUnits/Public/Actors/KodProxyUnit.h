#pragma once

#include "CoreMinimal.h"
#include "Actors/KodUnit.h"
#include "KodProxyUnit.generated.h"

class UStaticMeshComponent;

/**
 * Slice 0 greybox unit placed on L_Slice0.
 * Subobject name ProxyBody is load-bearing: the local map serializes it.
 * Do not swap this class to Recon or Engineer.
 */
UCLASS(Blueprintable)
class KODUNITS_API AKodProxyUnit : public AKodUnit
{
	GENERATED_BODY()

public:
	AKodProxyUnit();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Proxy")
	TObjectPtr<UStaticMeshComponent> ProxyBody;
};
