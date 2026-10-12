#pragma once

class UWorld;

/** Faultline Training. JSON lives in Config/KodDress/faultline_training/. */
namespace KodFaultline
{
	bool IsTrainingMap(const UWorld* World);
	/** Seat the octagon CC, place the jade arc, and spawn Scinder proxies. */
	void Begin(UWorld* World);
	/** One counter-wave after every KodEnemyOutpost is gone. No economy. */
	void TickWave(UWorld* World);
}
