#pragma once

#include "CoreMinimal.h"

class AActor;
class UMeshComponent;

/**
 * Presentation-only Jadeite colour. Reads UKodJadeitePaletteSubsystem, which
 * pushes the same values into MPC_KodJadeite. Does not write sim state.
 *
 * Jadeite nodes use M_KodJadeite when Tools/kod_jadeite_mpc_setup.py has
 * created it (that material calls MF_KodJadeiteTint). Otherwise a dynamic
 * tint of the tuned base and emissive is applied.
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
	KODUNITS_API void ApplyJadeiteNode(UMeshComponent* Mesh);
	KODUNITS_API void ApplyTunedEmissive(UMeshComponent* Mesh);
}
