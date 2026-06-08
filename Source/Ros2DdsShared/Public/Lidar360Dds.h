#pragma once

#include "CoreMinimal.h"
#include "Lidar360Types.h"
#include "Ros2SensorTypes.h"

#if WITH_ROS2_DDS
struct sensor_msgs_msg_PointCloud2;
struct sensor_msgs_msg_Image;
#endif

class ROS2DDSSHAREDCAMERA_API FLidar360Dds
{
public:
	static bool Init(const FString& PluginName = TEXT("unreal_gpu_camera"));
	static void Shutdown();
	static bool IsInitialized();

	static bool CreateWriter(const FString& TopicName, int32& OutWriter);
	static bool CreateReader(const FString& TopicName, int32& OutReader);
	static void DestroyEndpoint(int32& Entity);

	static sensor_msgs_msg_PointCloud2* AllocSample();
	static void FreeSample(sensor_msgs_msg_PointCloud2* Sample);
	static void PublishAsync(int32 Writer, sensor_msgs_msg_PointCloud2* Sample, TFunction<void(bool)> OnDone = nullptr);
	static bool TakeLatest(int32 Reader, FLidar360SensorFrame& OutFrame);

	static bool CreateImageWriter(const FString& TopicName, int32& OutWriter);
	static bool CreateImageReader(const FString& TopicName, int32& OutReader);
	static sensor_msgs_msg_Image* AllocImageSample();
	static void FreeImageSample(sensor_msgs_msg_Image* Sample);
	static void PublishImageAsync(int32 Writer, sensor_msgs_msg_Image* Sample, TFunction<void(bool)> OnDone = nullptr);
	static bool TakeLatestImage(int32 Reader, FRos2ImageFrame& OutFrame);
};
