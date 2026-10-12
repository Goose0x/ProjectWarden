#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "KodSpectatorController.generated.h"

/**
 * Spectator session controller. Camera input lives on AKodSpectatorPawn.
 * Clicks do not select or order. The canvas HUD still draws the bank.
 */
UCLASS(Blueprintable)
class KODCORE_API AKodSpectatorController : public APlayerController
{
	GENERATED_BODY()

public:
	AKodSpectatorController();

	virtual void BeginPlay() override;
	virtual void UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents) override;
};
