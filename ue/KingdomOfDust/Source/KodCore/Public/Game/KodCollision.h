#pragma once

#include "Engine/EngineTypes.h"

/**
 * Project trace channels from DefaultEngine.ini.
 * KodSelect is ignored by ground and greybox meshes. Units and buildings block it.
 * KodGround is ignored by units. Floors and the generated ground plane block it.
 */
namespace KodCollision
{
	inline constexpr ECollisionChannel Select = ECC_GameTraceChannel1;
	inline constexpr ECollisionChannel Ground = ECC_GameTraceChannel2;
}
