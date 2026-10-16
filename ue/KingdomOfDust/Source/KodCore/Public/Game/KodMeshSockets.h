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
	/** Command center gatherers. Not the barracks exit. */
	inline const FName WorkerExit(TEXT("WorkerExit"));
	/** Trained unit spawn. Barracks uses this; older meshes may only have SOCKET_UnitExit. */
	inline const FName UnitExit(TEXT("UnitExit"));
	/** Vanguard command center bay. Visual exit. Not a sim spawn point. */
	inline const FName BayRamp(TEXT("BayRamp"));
	/** VG_BUNKER trained-unit and unload side. 3D owes this socket. */
	inline const FName Exit(TEXT("Exit"));
	/** VG_BUNKER turret piece. 3D may author TurretMount or TurretMount_0..3. */
	inline const FName TurretMount(TEXT("TurretMount"));
	/** VG_BUNKER muzzle flashes. One socket per port. */
	inline const FName Port0(TEXT("Port_0"));
	inline const FName Port1(TEXT("Port_1"));
	inline const FName Port2(TEXT("Port_2"));
	inline const FName Port3(TEXT("Port_3"));

	inline bool ComponentHasSocket(const USceneComponent* Component, FName Socket, FVector& OutWorld)
	{
		if (!Component || !Component->DoesSocketExist(Socket))
		{
			return false;
		}
		OutWorld = Component->GetSocketLocation(Socket);
		return true;
	}

	inline bool TryGet(const AActor* Actor, FName Socket, FVector& OutWorld)
	{
		if (!Actor || Socket.IsNone())
		{
			return false;
		}
		const FName Prefixed(*FString::Printf(TEXT("SOCKET_%s"), *Socket.ToString()));
		TInlineComponentArray<USceneComponent*> Components;
		Actor->GetComponents(Components);
		for (USceneComponent* Component : Components)
		{
			if (ComponentHasSocket(Component, Socket, OutWorld))
			{
				return true;
			}
		}
		for (USceneComponent* Component : Components)
		{
			if (ComponentHasSocket(Component, Prefixed, OutWorld))
			{
				return true;
			}
		}
		return false;
	}
}
