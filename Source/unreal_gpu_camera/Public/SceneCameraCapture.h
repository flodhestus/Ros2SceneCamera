#pragma once

#include "CoreMinimal.h"

class UTextureRenderTarget2D;

class UNREAL_GPU_CAMERA_API FSceneCameraCapture
{
public:
	static void Init(int32 Width, int32 Height);
	static void Shutdown();

	static void EnqueueRgb8Capture(
		UTextureRenderTarget2D* RenderTarget,
		uint8* Dest,
		int32 DestCapacityBytes,
		TFunction<void(bool bSuccess, int32 Width, int32 Height)> OnComplete);
};
