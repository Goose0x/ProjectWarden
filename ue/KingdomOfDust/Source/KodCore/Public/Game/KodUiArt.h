#pragma once

#include "CoreMinimal.h"
#include "Game/KodWireTint.h"

/**
 * Wireframe and command-icon lookup. Presentation only.
 * The wireframe slot uses the unit icon. Optional zone masks live under
 * /Game/Warden/UI/Wireframes. Icons live under /Game/Warden/UI/Icons.
 * A soft ref registered from the definition is used when the manifest has no icon.
 * Ranger is Marine. There is no separate Ranger wireframe.
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
