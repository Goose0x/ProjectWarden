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
	constexpr const TCHAR* UiWireframeRoot = TEXT("/Game/Warden/UI/Wireframes");
	constexpr const TCHAR* UiIconRoot = TEXT("/Game/Warden/UI/Icons");
	constexpr const TCHAR* UiFrameRoot = TEXT("/Game/Warden/UI/Frames");
	constexpr const TCHAR* FrameVgMinimap = TEXT("/Game/Warden/UI/Frames/T_VG_minimap_wing.T_VG_minimap_wing");
	constexpr const TCHAR* FrameVgCentre = TEXT("/Game/Warden/UI/Frames/T_VG_centre_info.T_VG_centre_info");
	constexpr const TCHAR* FrameVgPortrait = TEXT("/Game/Warden/UI/Frames/T_VG_portrait_frame.T_VG_portrait_frame");
	constexpr const TCHAR* FrameVgCard = TEXT("/Game/Warden/UI/Frames/T_VG_cmdcard_wing.T_VG_cmdcard_wing");
	constexpr const TCHAR* FrameScMinimap = TEXT("/Game/Warden/UI/Frames/T_SC_minimap_wing.T_SC_minimap_wing");
	constexpr const TCHAR* FrameScCentre = TEXT("/Game/Warden/UI/Frames/T_SC_centre_info.T_SC_centre_info");
	constexpr const TCHAR* FrameScPortrait = TEXT("/Game/Warden/UI/Frames/T_SC_portrait_frame.T_SC_portrait_frame");
	constexpr const TCHAR* FrameScCard = TEXT("/Game/Warden/UI/Frames/T_SC_cmdcard_wing.T_SC_cmdcard_wing");
	/** Resource readout. 28 is the HUD slot. 64 is the fallback if the 28 texture is absent. */
	constexpr const TCHAR* IconJadeite28 = TEXT("/Game/Warden/UI/Icons/T_RES_Jadeite_28.T_RES_Jadeite_28");
	constexpr const TCHAR* IconJadeite64 = TEXT("/Game/Warden/UI/Icons/T_RES_Jadeite_64.T_RES_Jadeite_64");
	constexpr const TCHAR* IconLuminene28 = TEXT("/Game/Warden/UI/Icons/T_RES_Luminene_28.T_RES_Luminene_28");
	constexpr const TCHAR* IconLuminene64 = TEXT("/Game/Warden/UI/Icons/T_RES_Luminene_64.T_RES_Luminene_64");
	constexpr const TCHAR* IconSupplyVg28 = TEXT("/Game/Warden/UI/Icons/T_RES_Supply_VG_28.T_RES_Supply_VG_28");
	constexpr const TCHAR* IconSupplyVg64 = TEXT("/Game/Warden/UI/Icons/T_RES_Supply_VG_64.T_RES_Supply_VG_64");
	constexpr const TCHAR* IconSupplySc28 = TEXT("/Game/Warden/UI/Icons/T_RES_Supply_SC_28.T_RES_Supply_SC_28");
	constexpr const TCHAR* IconSupplySc64 = TEXT("/Game/Warden/UI/Icons/T_RES_Supply_SC_64.T_RES_Supply_SC_64");
	constexpr const TCHAR* IconWorkerVg28 = TEXT("/Game/Warden/UI/Icons/T_RES_Worker_VG_28.T_RES_Worker_VG_28");
	constexpr const TCHAR* IconWorkerVg64 = TEXT("/Game/Warden/UI/Icons/T_RES_Worker_VG_64.T_RES_Worker_VG_64");
	constexpr const TCHAR* IconWorkerSc28 = TEXT("/Game/Warden/UI/Icons/T_RES_Worker_SC_28.T_RES_Worker_SC_28");
	constexpr const TCHAR* IconWorkerSc64 = TEXT("/Game/Warden/UI/Icons/T_RES_Worker_SC_64.T_RES_Worker_SC_64");
	constexpr const TCHAR* WireSilhouetteMaterial = TEXT("/Game/Warden/UI/Wireframes/M_KodWireSilhouette.M_KodWireSilhouette");

	constexpr const TCHAR* RangerRifle = TEXT("/Game/Warden/Data/RangerRifle.RangerRifle");
	/** Marine infantry DataAsset. The old Ranger package permanently redirects here. */
	constexpr const TCHAR* Marine = TEXT("/Game/Warden/Data/Marine.Marine");
	/** Legacy package path. CoreRedirects send it to Marine. Ranger is Marine. */
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
	/** Vanguard infantry. Was the engine id Ranger through sim blob version 10. */
	constexpr const TCHAR* Id_Marine = TEXT("Marine");
	/** Legacy infantry id. Ranger is Marine. Used only to read blobs older than version 11. */
	constexpr const TCHAR* Id_Ranger = TEXT("Ranger");

	/** Current sim blob. Version 11 writes Marine. Version 12 adds the Faultline lane. Version 13 adds the wave focus id. */
	inline constexpr int32 SimSaveVersion = 13;

	/**
	 * Saves and replays below SimSaveVersion stored the Marine as Ranger.
	 * The alias is applied on load only. It is not part of the idle hash.
	 */
	inline FName AliasLegacyMarineId(FName Id, int32 LoadedVersion)
	{
		if (LoadedVersion < 11 && Id == FName(Id_Ranger))
		{
			return FName(Id_Marine);
		}
		return Id;
	}
	/** Cube proxy. Used when the enemy faction is Scinder. */
	constexpr const TCHAR* Id_ScinderInfantry = TEXT("ScinderInfantry");
	constexpr const TCHAR* Id_ScnFighter = TEXT("SCN_FIGHTER");
	constexpr const TCHAR* Id_ScnTwinblade = TEXT("SCN_TWINBLADE");
	constexpr const TCHAR* Id_ScnWorker = TEXT("SCN_WORKER");
	constexpr const TCHAR* Id_CombatStim = TEXT("CombatStim");
	constexpr const TCHAR* Id_Dozer = TEXT("Dozer");
	constexpr const TCHAR* Id_DozerMelee = TEXT("DozerMelee");
	constexpr const TCHAR* Id_CommandCenter = TEXT("CommandCenter");
	constexpr const TCHAR* Id_Barracks = TEXT("Barracks");
	constexpr const TCHAR* Id_SupplyDepot = TEXT("SupplyDepot");
	constexpr const TCHAR* Id_Rig = TEXT("Rig");
	constexpr const TCHAR* Id_EngineeringBay = TEXT("EngineeringBay");
	constexpr const TCHAR* Id_Bunker = TEXT("Bunker");
	constexpr const TCHAR* Id_Turret = TEXT("Turret");
	constexpr const TCHAR* Id_Medic = TEXT("Medic");
	constexpr const TCHAR* Id_InfantryWeapons = TEXT("InfantryWeapons");
	constexpr const TCHAR* Id_InfantryArmor = TEXT("InfantryArmor");
	constexpr const TCHAR* Id_SatelliteFeed = TEXT("SatelliteFeed");
	constexpr const TCHAR* Id_OrbitalGatherer = TEXT("OrbitalGatherer");
	constexpr const TCHAR* Id_OrbitalGathererDrop = TEXT("OrbitalGathererDrop");
	constexpr const TCHAR* Id_SupplyBoost = TEXT("SupplyBoost");
	constexpr const TCHAR* Id_IsrView = TEXT("IsrView");
	constexpr const TCHAR* Id_IsrPlatform = TEXT("IsrPlatform");
	constexpr const TCHAR* Id_ArtilleryStrike = TEXT("ArtilleryStrike");
	constexpr const TCHAR* Id_ReconSweep = TEXT("ReconSweep");
	constexpr const TCHAR* Id_FieldRepair = TEXT("FieldRepair");
	/** Balance row. Amount is the starting pool. CooldownTicks is the trickle interval. */
	constexpr const TCHAR* Id_PowerIncome = TEXT("PowerIncome");
	constexpr const TCHAR* Id_USA = TEXT("USA");
	constexpr const TCHAR* Id_JadeiteNode = TEXT("JadeiteNode");
	constexpr const TCHAR* Id_LumineneVent = TEXT("LumineneVent");
	/** Deprecated catalog key. Points at the same bootstrap vent as Id_LumineneVent. */
	constexpr const TCHAR* Id_OilSource = TEXT("OilSource");

	/**
	 * Current Marine body presentation (not a DataAsset id).
	 * Mount folder is /Game/Warden/Characters/USA/Ranger/ — not /Game/Units/Meshes/.
	 * Soft Dev imports the LOW FBX locally and renames the static mesh to SM_Ranger_Body.
	 * Bootstrap must leave UKodUnitDefinition mesh refs null; this path 404s until that import.
	 * Skeletal mesh is a later anim pass. Slice 0 uses the static mesh on AKodUnit::UnitMesh.
	 */
	constexpr const TCHAR* RangerCharacterRoot = TEXT("/Game/Warden/Characters/USA/Ranger");
	constexpr const TCHAR* RangerBodyStaticMesh = TEXT("/Game/Warden/Characters/USA/Ranger/SM_Ranger_Body.SM_Ranger_Body");

	/**
	 * Prototype visuals already in Content. Soft refs only. A failed load keeps the placeholder.
	 * AMP stays on the cube unless the editable Dozer DataAsset sets SkeletalMesh to ProtoAmpSkeletalMesh.
	 */
	constexpr const TCHAR* ProtoHqMesh = TEXT("/Game/RTS_Pack_B/Meshes/SM_general.SM_general");
	constexpr const TCHAR* ProtoDepotMesh = TEXT("/Game/RTS_Pack_B/Meshes/SM_storage.SM_storage");
	constexpr const TCHAR* ProtoBarracksMesh = TEXT("/Game/RTS_Pack_B/Meshes/SM_barracks_1.SM_barracks_1");
	constexpr const TCHAR* ProtoJadeiteMesh = TEXT("/Game/ModularSciFi/StaticMeshes/Mineral/SM_Mineral_1.SM_Mineral_1");
	constexpr const TCHAR* ProtoAmpSkeletalMesh = TEXT("/Game/RTS_Pack_B/SkeletalMeshes/SK_truck_a.SK_truck_a");

	FORCEINLINE FSoftObjectPath MakeSoftPath(const TCHAR* AssetId)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), DataRoot, AssetId, AssetId));
	}
}
