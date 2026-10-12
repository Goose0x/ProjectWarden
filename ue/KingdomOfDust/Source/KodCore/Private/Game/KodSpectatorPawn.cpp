#include "Game/KodSpectatorPawn.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Game/KodPlayerController.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

AKodSpectatorPawn::AKodSpectatorPawn()
{
	MinArmLength = 1200.f;
	MaxArmLength = 14000.f;
	bAllowAltDragRotate = true;
	bFrameOnCommandCenter = false;
	bSupportsUnitFollow = true;
}

AActor* AKodSpectatorPawn::FindFollowTarget() const
{
	if (const AKodPlayerController* PlayerPC = Cast<AKodPlayerController>(GetController()))
	{
		const TArray<AActor*> Selected = PlayerPC->GetLocalSelection();
		for (AActor* Actor : Selected)
		{
			if (Actor && Actor != this)
			{
				return Actor;
			}
		}
	}

	const APlayerController* PC = Cast<APlayerController>(GetController());
	UWorld* World = GetWorld();
	if (!PC || !World)
	{
		return nullptr;
	}
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	if (!PC->DeprojectMousePositionToWorld(Origin, Direction))
	{
		return nullptr;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(KodCamFollow), false);
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	const FVector End = Origin + Direction * 200000.f;
	if (!World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Pawn, Params))
	{
		return nullptr;
	}
	AActor* HitActor = Hit.GetActor();
	if (!HitActor || HitActor == this)
	{
		return nullptr;
	}
	const FString Label = HitActor->GetActorNameOrLabel();
	if (HitActor->ActorHasTag(FName(TEXT("KodGround"))) || Label.Contains(TEXT("PROXY_GROUND")))
	{
		return nullptr;
	}
	return HitActor;
}
