#pragma once

#include "CoreMinimal.h"
#include "Game/KodPlayerController.h"
#include "KodSlice0PlayerController.generated.h"

/**
 * L_Slice0 PC: left select/box, right move/attack through UKodCommandSubsystem.
 *
 * Script class path: /Script/KingdomOfDust.KodSlice0PlayerController
 */
UCLASS(Blueprintable)
class KINGDOMOFDUST_API AKodSlice0PlayerController : public AKodPlayerController
{
	GENERATED_BODY()

public:
	AKodSlice0PlayerController();

protected:
	virtual void IssueMoveToSelection_Implementation(FVector WorldLocation) override;
	virtual void IssueAttackToSelection_Implementation(AActor* Target) override;
};
