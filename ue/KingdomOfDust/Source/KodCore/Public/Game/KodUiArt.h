#pragma once

#include "CoreMinimal.h"

/**
 * Wireframe and command-icon lookup. Presentation only.
 * Order: a soft ref registered from the unit or building definition,
 * then Config/KodUI/ui_art_manifest.json, then
 * /Game/Warden/UI/Wireframes/<Id>.<Id> or /Game/Warden/UI/Icons/<Id>.<Id>.
 */
class KODCORE_API FKodUiArt
{
public:
	static void LoadManifest();
	static void RegisterWireframe(FName Id, const FSoftObjectPath& Path);
	static void RegisterIcon(FName Id, const FSoftObjectPath& Path);
	static FSoftObjectPath ResolveWireframe(FName Id);
	static FSoftObjectPath ResolveIcon(FName Id);
};
