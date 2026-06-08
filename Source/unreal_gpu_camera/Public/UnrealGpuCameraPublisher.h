#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnrealGpuCameraTypes.h"
#include <atomic>
#include "UnrealGpuCameraPublisher.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UCLASS()
class UNREAL_GPU_CAMERA_API AUnrealGpuCameraPublisher : public AActor
{
	GENERATED_BODY()

public:
	AUnrealGpuCameraPublisher();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera")
	FString TopicName = TEXT("rt/sensor_image");

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera")
	FString FrameId = TEXT("camera_link");

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera", meta = (ClampMin = "640", ClampMax = "1920"))
	int32 ImageWidth = ROS2_CAMERA_FULLHD_WIDTH;

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera", meta = (ClampMin = "480", ClampMax = "1080"))
	int32 ImageHeight = ROS2_CAMERA_FULLHD_HEIGHT;

protected:
	void OnPublishTimer();
	void StartDeferredCapture();
	void BeginGpuReadback(int32 Index);
	void PublishCapturedFrame(int32 Index, int32 Width, int32 Height);
	void ShutdownDds();
	void ConfigureSceneCapture();
	void InitializeRenderTarget(int32 Index);
	UTextureRenderTarget2D* GetActiveRenderTarget() const;
	void SwapSamples();
	bool IsSlotAvailable(int32 Index) const;

	UPROPERTY(VisibleAnywhere, Category = "ROS2 Camera")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	TObjectPtr<UTextureRenderTarget2D> RenderTargets[2];

	void* DdsImageSamples[2] = { nullptr, nullptr };
	std::atomic<int32> WriteIndex{ 0 };
	std::atomic<int32> FramesInFlight{ 0 };
	static constexpr int32 MaxFramesInFlight = 2;
	static constexpr float PublishRateHz = 30.0f;

	FTimerHandle PublishTimer;
	bool bDeferredCapturePending = false;
	bool bSceneCaptureInFlight = false;
	bool SlotReadbackPending[2] = { false, false };
	int32 DdsWriter = 0;
};
