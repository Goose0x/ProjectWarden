#pragma once

#include "CoreMinimal.h"

/**
 * Soft path contract for Slice 0 Warden DataAssets.
 * Law: bare asset ids (no DA_ prefix) under /Game/Warden/Data/.
 * SoftObjectPath form: /Game/Warden/Data/<Id>.<Id>
 */
namespace KodWardenPaths
{
	constexpr const TCHAR* DataRoot = TEXT("/Game/Warden/Data");

	constexpr const TCHAR* RangerRifle = TEXT("/Game/Warden/Data/RangerRifle.RangerRifle");
	constexpr const TCHAR* Ranger = TEXT("/Game/Warden/Data/Ranger.Ranger");
	constexpr const TCHAR* Dozer = TEXT("/Game/Warden/Data/Dozer.Dozer");
	constexpr const TCHAR* CommandCenter = TEXT("/Game/Warden/Data/CommandCenter.CommandCenter");
	constexpr const TCHAR* Barracks = TEXT("/Game/Warden/Data/Barracks.Barracks");
	constexpr const TCHAR* USA = TEXT("/Game/Warden/Data/USA.USA");

	/** Bare FName / Primary Asset Name law strings. */
	constexpr const TCHAR* Id_RangerRifle = TEXT("RangerRifle");
	constexpr const TCHAR* Id_Ranger = TEXT("Ranger");
	constexpr const TCHAR* Id_Dozer = TEXT("Dozer");
	constexpr const TCHAR* Id_CommandCenter = TEXT("CommandCenter");
	constexpr const TCHAR* Id_Barracks = TEXT("Barracks");
	constexpr const TCHAR* Id_USA = TEXT("USA");

	FORCEINLINE FSoftObjectPath MakeSoftPath(const TCHAR* AssetId)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), DataRoot, AssetId, AssetId));
	}
}
