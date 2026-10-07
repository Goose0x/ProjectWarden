#include "Actors/KodProxyUnit.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

AKodProxyUnit::AKodProxyUnit()
{
	ProxyBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProxyBody"));
	ProxyBody->SetupAttachment(GetCapsuleComponent());
	ProxyBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProxyBody->SetCanEverAffectNavigation(false);

	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetVisibility(false);
		Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}
