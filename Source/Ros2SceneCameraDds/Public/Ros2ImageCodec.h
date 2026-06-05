#pragma once

#include "CoreMinimal.h"

#if WITH_ROS2_DDS
struct sensor_msgs_msg_Image;
#endif

class ROS2SCENECAMERADDS_API FRos2ImageCodec
{
public:
#if WITH_ROS2_DDS
	static void InitImageSample(sensor_msgs_msg_Image& Sample, int32 MaxWidth, int32 MaxHeight);
	static bool FillRgb8(
		sensor_msgs_msg_Image& Sample,
		const uint8* Rgb,
		int32 Width,
		int32 Height,
		const FString& FrameId);
	/** Sample.data already contains rgb8 pixels; updates header and dimensions only. */
	static bool CommitImageMetadata(
		sensor_msgs_msg_Image& Sample,
		int32 Width,
		int32 Height,
		const FString& FrameId);
#endif
};
