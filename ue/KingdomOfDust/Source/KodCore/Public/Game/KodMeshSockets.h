#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

/** Bare socket names on the faction meshes. Missing sockets keep the old offsets. */
namespace KodMeshSockets
{
	inline const FName Rally(TEXT("Rally"));
	inline const FName HealthBar(TEXT("HealthBar"));
	inline const FName Door(TEXT("Door"));
	inline const FName WorkerExit(TEXT("WorkerExit"));

	inline bool TryGet(const AActor* Actor, FName Socket, FVector& OutWorld)
	{
		if (!Actor || Socket.IsNone())
		{
			return false;
		}
		TInlineComponentArray<USceneComponent*> Components;
		Actor->GetComponents(Components);
		for (USceneComponent* Component : Components)
		{
			if (Component && Component->DoesSocketExist(Socket))
			{
				OutWorld = Component->GetSocketLocation(Socket);
				return true;
			}
		}
		return false;
	}
}
