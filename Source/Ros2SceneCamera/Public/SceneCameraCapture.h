#pragma once

#include "CoreMinimal.h"

class UTextureRenderTarget2D;

class ROS2SCENECAMERA_API FSceneCameraCapture
{
public:
	static void Init(int32 Width, int32 Height);
	static void Shutdown();
	static bool CaptureRgb8Into(
		UTextureRenderTarget2D* RenderTarget,
		uint8* Dest,
		int32 DestCapacityBytes,
		int32& OutWidth,
		int32& OutHeight);
};
