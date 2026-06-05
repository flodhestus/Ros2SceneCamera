#pragma once

#include "CoreMinimal.h"
#include "Lidar360Types.h"

#if WITH_LIDAR360_DDS
struct sensor_msgs_msg_PointCloud2;
#endif

class ROS2DDSSHAREDCAMERA_API FLidar360PointCloud2Codec
{
public:
	static void InitSampleFields(sensor_msgs_msg_PointCloud2& Sample);
	static int32 PackFromGpuHits(
		sensor_msgs_msg_PointCloud2& Sample,
		const uint8* GpuData,
		int32 NumRays,
		const FVector& OriginCm,
		const FVector& Forward,
		const FVector& Right,
		const FVector& Up,
		const FString& FrameId);
	static int32 DecodeToRenderBuffer(
		const sensor_msgs_msg_PointCloud2& Sample,
		TArray<float>& OutXyzIntensity,
		int32 MaxPoints = LIDAR360_MAX_POINTS);
};
