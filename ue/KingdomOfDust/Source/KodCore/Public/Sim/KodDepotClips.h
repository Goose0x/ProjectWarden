#pragma once

#include "CoreMinimal.h"
#include "Sim/KodBuildTicks.h"
#include "Sim/KodProductionTypes.h"

/** Supply depot pose. Stored on FKodStructureState::DepotPose. */
enum class EKodDepotPose : uint8
{
	Raised = 0,
	Lowering = 1,
	Lowered = 2,
	Raising = 3,
	Dying = 4
};

/** Clip and notify names. The anim team file may override frames and sockets. */
namespace KodDepotClipId
{
	inline constexpr TCHAR Idle[] = TEXT("A_SupplyDepot_Idle");
	inline constexpr TCHAR Lower[] = TEXT("A_SupplyDepot_Lower");
	inline constexpr TCHAR LoweredIdle[] = TEXT("A_SupplyDepot_Lowered_Idle");
	inline constexpr TCHAR Raise[] = TEXT("A_SupplyDepot_Raise");
	inline constexpr TCHAR Death[] = TEXT("A_SupplyDepot_Death");
	inline constexpr TCHAR DeathLowered[] = TEXT("A_SupplyDepot_Death_Lowered");
	inline constexpr TCHAR Skeleton[] = TEXT("SK_Vanguard_SupplyDepot");

	inline constexpr TCHAR NavWalkable[] = TEXT("AN_Depot_NavWalkable");
	inline constexpr TCHAR RaiseStart[] = TEXT("AN_Depot_RaiseStart");
	inline constexpr TCHAR LowerStart[] = TEXT("AN_Depot_LowerStart");
	inline constexpr TCHAR VentTick[] = TEXT("AN_Depot_VentTick");
	inline constexpr TCHAR VentHiss[] = TEXT("AN_Depot_VentHiss");
	inline constexpr TCHAR Dust[] = TEXT("AN_Depot_Dust");
	inline constexpr TCHAR Land[] = TEXT("AN_Depot_Land");
	inline constexpr TCHAR Clunk[] = TEXT("AN_Depot_Clunk");
	inline constexpr TCHAR DeathExplosion[] = TEXT("AN_Depot_Death_Explosion");
	inline constexpr TCHAR DeathSecondary[] = TEXT("AN_Depot_Death_Secondary");

	/** Masked opacity threshold. World position offset stays off. Not a translucent pass. */
	inline constexpr TCHAR ClipLocalZ[] = TEXT("ClipLocalZ");
	inline constexpr float ClipLocalZCm = -20.f;
	inline constexpr TCHAR Amber[] = TEXT("DepotAmber");
}

struct KODCORE_API FKodDepotNotify
{
	FName Name;
	int32 Frame = 0;
	float TimeSeconds = 0.f;
	TArray<FName> Sockets;
	float Chance = 1.f;

	int32 Tick() const
	{
		return FMath::Max(0, KodBuildTicks::SecondsToBuildTicks(TimeSeconds));
	}
};

struct KODCORE_API FKodDepotClip
{
	FName Id;
	int32 Frames = 1;
	bool bLoop = false;
	TArray<FKodDepotNotify> Notifies;

	/** 30 fps authoring clock, converted onto the 16 Hz sim. */
	int32 Ticks() const
	{
		const float Seconds = static_cast<float>(FMath::Max(1, Frames)) / 30.f;
		return FMath::Max(1, KodBuildTicks::SecondsToBuildTicks(Seconds));
	}
};

/** Empty Niagara and Sound are no-ops. */
struct KODCORE_API FKodDepotFxSlot
{
	FName Notify;
	FString Niagara;
	FString Sound;

	bool IsEmpty() const
	{
		return Niagara.IsEmpty() && Sound.IsEmpty();
	}
};

/**
 * Built-in depot clips, replaced by Config/SupplyDepotClips.json when that file parses.
 * Shape: {clips:{<clip>:{frames,loop,notifies:[{name,frame,time_s,sockets[],chance?}]}}}
 */
class KODCORE_API FKodDepotClipTable
{
public:
	static const FKodDepotClipTable& Get();

	const FKodDepotClip* FindClip(FName ClipId) const;
	const FKodDepotNotify* FindNotify(FName ClipId, FName NotifyName) const;
	const FKodDepotFxSlot* FindFx(FName NotifyName) const;
	int32 ClipTicks(FName ClipId) const;
	/** AN_Depot_NavWalkable on Lower. 16 ticks (1.0 s) when the notify is missing. */
	int32 WalkableTick() const;
	/** AN_Depot_RaiseStart. 0 when the notify is missing, so a raise blocks on the command. */
	int32 RaiseBlockTick() const;

private:
	FKodDepotClipTable();
	void AddDefaults();
	void LoadJson();
	void LoadFxIni();
	void UpsertClip(FKodDepotClip&& Clip);
	FKodDepotFxSlot& FxSlot(FName NotifyName);

	TArray<FKodDepotClip> ClipList;
	TArray<FKodDepotFxSlot> FxSlots;
};

/** Clip the presentation should be playing for this pose. */
inline FName KodDepotActiveClip(const FKodStructureState& Structure)
{
	const EKodDepotPose Pose = static_cast<EKodDepotPose>(Structure.DepotPose);
	if (Pose == EKodDepotPose::Dying)
	{
		if (Structure.bDepotDeathLowered)
		{
			return FName(KodDepotClipId::DeathLowered);
		}
		return FName(KodDepotClipId::Death);
	}
	if (Pose == EKodDepotPose::Lowering)
	{
		return FName(KodDepotClipId::Lower);
	}
	if (Pose == EKodDepotPose::Lowered)
	{
		return FName(KodDepotClipId::LoweredIdle);
	}
	if (Pose == EKodDepotPose::Raising)
	{
		return FName(KodDepotClipId::Raise);
	}
	return FName(KodDepotClipId::Idle);
}
