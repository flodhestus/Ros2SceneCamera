#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

class ROS2DDSSHAREDCAMERA_API FLidar360Win32Viewport : public FRunnable, public TSharedFromThis<FLidar360Win32Viewport>
{
public:
	FLidar360Win32Viewport();
	virtual ~FLidar360Win32Viewport() override;

	bool StartViewport(const FString& Title, int32 Width = 1280, int32 Height = 720);
	void StopViewport();
	void SubmitPoints(const float* XyzIntensity, int32 PointCount, float MaxRangeM);

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	void RenderLocked();

	FRunnableThread* Thread = nullptr;
	FThreadSafeCounter StopTask;
	FCriticalSection BufferLock;
	TArray<float> FrontBuffer;
	TArray<float> BackBuffer;
	int32 PendingPointCount = 0;
	float PendingMaxRange = 200.f;
	int32 WindowWidth = 1280;
	int32 WindowHeight = 720;
	FString WindowTitle;
};
