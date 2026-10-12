#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"

class AActor;
class USceneComponent;

/** Places Config/KodDress/dress_list.json instances. Collision stays off. */
namespace KodDress
{
	void Apply(AActor* Owner, TArray<TObjectPtr<USceneComponent>>& OutDressing);
}
