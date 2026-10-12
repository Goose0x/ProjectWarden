#pragma once

#include "CoreMinimal.h"
#include "Game/KodRTSCameraPawn.h"
#include "KodSpectatorPawn.generated.h"

/**
 * Same SC2 camera as the player, with a wider zoom and alt-drag rotate on.
 * Does not issue orders. F follows a selected unit, or the unit under the cursor.
 */
UCLASS(Blueprintable, config = Game)
class KODCORE_API AKodSpectatorPawn : public AKodRTSCameraPawn
{
	GENERATED_BODY()

public:
	AKodSpectatorPawn();

protected:
	virtual AActor* FindFollowTarget() const override;
};
