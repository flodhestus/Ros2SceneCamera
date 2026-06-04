#pragma once

#include "CoreMinimal.h"

class UTextureRenderTarget2D;

class ROS2SCENECAMERA_API FSceneCameraCapture
{
public:
	static bool CaptureRgb8(UTextureRenderTarget2D* RenderTarget, TArray<uint8>& OutRgb, int32& OutWidth, int32& OutHeight);
};
