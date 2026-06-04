#pragma once

#include "CoreMinimal.h"

static constexpr int32 ROS2_CAMERA_MAX_WIDTH = 1920;
static constexpr int32 ROS2_CAMERA_MAX_HEIGHT = 1080;
static constexpr int32 ROS2_CAMERA_MAX_BYTES = ROS2_CAMERA_MAX_WIDTH * ROS2_CAMERA_MAX_HEIGHT * 3;

struct FRos2ImageFrame
{
	TArray<uint8> Data;
	int32 Width = 0;
	int32 Height = 0;
	int32 Step = 0;
};
