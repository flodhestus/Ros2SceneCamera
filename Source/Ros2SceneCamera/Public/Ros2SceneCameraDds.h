#pragma once

#include "CoreMinimal.h"
#include "Ros2SceneCameraTypes.h"

#if WITH_ROS2_SCENE_CAMERA_DDS
struct sensor_msgs_msg_Image;
#endif

class ROS2SCENECAMERA_API FRos2SceneCameraDds
{
public:
	static bool Init();
	static void Shutdown();
	static bool CreateImageWriter(const FString& TopicName, int32& OutWriter);
	static bool CreateImageReader(const FString& TopicName, int32& OutReader);
	static void DestroyEndpoint(int32& Entity);
	static sensor_msgs_msg_Image* AllocImageSample();
	static void FreeImageSample(sensor_msgs_msg_Image* Sample);
	static void PublishImageAsync(int32 Writer, sensor_msgs_msg_Image* Sample, TFunction<void(bool)> OnDone = nullptr);
	static bool TakeLatestImage(int32 Reader, FRos2ImageFrame& OutFrame);
};
