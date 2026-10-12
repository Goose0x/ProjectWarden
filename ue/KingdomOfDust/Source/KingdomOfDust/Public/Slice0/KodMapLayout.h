#pragma once

class UWorld;

/** Loads Config/KodDress/map_features.json into the sim and tags the world. */
namespace KodMapLayout
{
	void Load(UWorld* World);
}
