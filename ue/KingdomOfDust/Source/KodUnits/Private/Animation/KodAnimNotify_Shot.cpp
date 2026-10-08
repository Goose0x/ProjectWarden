#include "Animation/KodAnimNotify_Shot.h"
#include "Actors/KodUnit.h"
#include "Components/SkeletalMeshComponent.h"

void UKodAnimNotify_Shot::Notify(USkeletalMeshComponent* SkelComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(SkelComp, Animation, EventReference);
	if (!SkelComp)
	{
		return;
	}
	if (AKodUnit* UnitPawn = Cast<AKodUnit>(SkelComp->GetOwner()))
	{
		UnitPawn->PlayMuzzleFromShotNotify();
	}
}
