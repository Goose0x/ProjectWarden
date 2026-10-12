#pragma once

#include "CoreMinimal.h"

class AActor;
class UMeshComponent;
class UWorld;
class UMaterialInterface;

/**
 * Presentation-only Jadeite colour. Reads UKodJadeitePaletteSubsystem, which
 * pushes the same values into MPC_KodJadeite. Does not write sim state.
 *
 * Jadeite nodes use M_KodJadeite when Tools/kod_jadeite_mpc_setup.py has
 * created it (that material calls MF_KodJadeiteTint). Otherwise a dynamic
 * tint of tuned JadeBase is applied. A mesh that already uses
 * MI_Jadeite_Node (parent M_Crystal) is left alone so the collection drives
 * it and a dynamic tint is not stacked on top. Default JadeBase is #11928F.
 *
 * Scinder eyes and regen: any vector parameter whose name contains "Jade",
 * or is EyeEmissive, EyeGlow, EyeColor, RegenGlow, or RegenEmissive, receives
 * the tuned colour. Hook MF_KodJadeiteTint's Emissive output on the material
 * when the parameter should be retired.
 */
namespace KodJadeiteVisual
{
	KODUNITS_API bool IsScinderId(FName DefinitionId);
	KODUNITS_API bool IsScinderActor(const AActor* Actor);
	/** Tuned JadeBase from MPC_KodJadeite, or #11928F when the palette is absent. */
	KODUNITS_API FLinearColor TunedJadeBase(const UWorld* World);
	KODUNITS_API bool UsesJadeiteNodeMaterial(const UMaterialInterface* Material);
	KODUNITS_API bool UsesJadeiteNodeMaterial(const UMeshComponent* Mesh);
	KODUNITS_API void ApplyJadeiteNode(UMeshComponent* Mesh);
	KODUNITS_API void ApplyTunedEmissive(UMeshComponent* Mesh);
}
