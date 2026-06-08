#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

class UNREAL_GPU_CAMERA_API FRos2ImageViewport : public FRunnable
{
public:
	FRos2ImageViewport();
	virtual ~FRos2ImageViewport() override;

	bool StartViewport(const FString& Title, int32 Width = 960, int32 Height = 540);
	void StopViewport();
	void SubmitImage(const uint8* Rgb, int32 Width, int32 Height);

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	void RenderLocked();

	FString WindowTitle;
	int32 WindowWidth = 960;
	int32 WindowHeight = 540;
	FRunnableThread* Thread = nullptr;
	FThreadSafeCounter StopTask;
	FCriticalSection BufferLock;
	TArray<uint8> FrontBuffer;
	TArray<uint8> BackBuffer;
	int32 PendingWidth = 0;
	int32 PendingHeight = 0;
};
