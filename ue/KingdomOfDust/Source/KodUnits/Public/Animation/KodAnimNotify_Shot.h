#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "KodAnimNotify_Shot.generated.h"

/**
 * Place on the Fire clip. Positions the muzzle flash at SOCKET_Muzzle,
 * then weapon_r. If neither socket exists, AKodUnit uses the timed body-front flash.
 * Static / cube units never play this notify and keep the immediate flash.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Kod Shot"))
class KODUNITS_API UKodAnimNotify_Shot : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* SkelComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
