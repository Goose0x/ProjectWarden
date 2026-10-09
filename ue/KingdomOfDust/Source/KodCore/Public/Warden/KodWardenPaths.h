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
	constexpr const TCHAR* JadeiteNode = TEXT("/Game/Warden/Data/JadeiteNode.JadeiteNode");
	constexpr const TCHAR* LumineneVent = TEXT("/Game/Warden/Data/LumineneVent.LumineneVent");
	/** Deprecated alias. Resolve still finds the vent when no OilSource uasset is loaded. */
	constexpr const TCHAR* OilSource = TEXT("/Game/Warden/Data/OilSource.OilSource");

	/** Bare FName / Primary Asset Name law strings. */
	constexpr const TCHAR* Id_RangerRifle = TEXT("RangerRifle");
	constexpr const TCHAR* Id_Ranger = TEXT("Ranger");
	constexpr const TCHAR* Id_Dozer = TEXT("Dozer");
	constexpr const TCHAR* Id_CommandCenter = TEXT("CommandCenter");
	constexpr const TCHAR* Id_Barracks = TEXT("Barracks");
	constexpr const TCHAR* Id_USA = TEXT("USA");
	constexpr const TCHAR* Id_JadeiteNode = TEXT("JadeiteNode");
	constexpr const TCHAR* Id_LumineneVent = TEXT("LumineneVent");
	/** Deprecated catalog key. Points at the same bootstrap vent as Id_LumineneVent. */
	constexpr const TCHAR* Id_OilSource = TEXT("OilSource");

	/**
	 * Ranger body presentation (not a DataAsset id).
	 * Mount folder is /Game/Warden/Characters/USA/Ranger/ — not /Game/Units/Meshes/.
	 * Soft Dev imports the LOW FBX locally and renames the static mesh to SM_Ranger_Body.
	 * Bootstrap must leave UKodUnitDefinition mesh refs null; this path 404s until that import.
	 * Skeletal mesh is a later anim pass. Slice 0 uses the static mesh on AKodUnit::UnitMesh.
	 */
	constexpr const TCHAR* RangerCharacterRoot = TEXT("/Game/Warden/Characters/USA/Ranger");
	constexpr const TCHAR* RangerBodyStaticMesh = TEXT("/Game/Warden/Characters/USA/Ranger/SM_Ranger_Body.SM_Ranger_Body");

	FORCEINLINE FSoftObjectPath MakeSoftPath(const TCHAR* AssetId)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), DataRoot, AssetId, AssetId));
	}
}
