#pragma once

#include "CoreMinimal.h"

class UWorld;

/** Selects active sensor backends and ensures a single CycloneDDS participant per process. */
class ROS2DDSSHAREDCAMERA_API FRos2SensorCoordinator
{
public:
	static bool IsOptiXLidarEnabled();
	static bool IsGpuLidarEnabled();
	static bool IsSceneCameraEnabled();
	/** OptiX LiDAR wins when both LiDAR plugins are enabled. */
	static bool ShouldRunOptiXLidar();
	static bool ShouldRunGpuLidar();
	static bool EnsureDdsInitialized();
	static void OnPieWorldStarted(UWorld* World);
};
