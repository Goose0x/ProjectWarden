#pragma once

#include "CoreMinimal.h"
#include "Game/KodWireTint.h"

/**
 * Wireframe and command-icon lookup. Presentation only.
 * The manifest maps an engine id to imported file stems.
 * 160 and 48 wireframes and zone masks live under /Game/Warden/UI/Wireframes.
 * 128 icons live under /Game/Warden/UI/Icons.
 * A soft ref registered from the unit or building definition is used when the
 * manifest has no file for that slot. VG_Ranger is the placeholder stem.
 */
class KODCORE_API FKodUiArt
{
public:
	enum class EKind : uint8
	{
		Wire160,
		Wire48,
		ZoneMask,
		Icon
	};

	static void LoadManifest();
	static void RegisterWireframe(FName Id, const FSoftObjectPath& Path);
	static void RegisterIcon(FName Id, const FSoftObjectPath& Path);
	static FSoftObjectPath Resolve(FName Id, EKind Kind);
	static FSoftObjectPath Resolve(FName Id, KodWireTint::ESlot Slot);
};
