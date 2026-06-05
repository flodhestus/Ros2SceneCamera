#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ros2SceneCameraTypes.h"
#include "Ros2SceneCameraPublisher.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UCLASS()
class ROS2SCENECAMERA_API ARos2SceneCameraPublisher : public AActor
{
	GENERATED_BODY()

public:
	ARos2SceneCameraPublisher();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera")
	FString TopicName = TEXT("rt/sensor_image");

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera")
	FString FrameId = TEXT("camera_link");

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera", meta = (ClampMin = "1", ClampMax = "60"))
	float PublishRateHz = 20.f;

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera", meta = (ClampMin = "640", ClampMax = "1920"))
	int32 ImageWidth = ROS2_CAMERA_FULLHD_WIDTH;

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera", meta = (ClampMin = "480", ClampMax = "1080"))
	int32 ImageHeight = ROS2_CAMERA_FULLHD_HEIGHT;

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera")
	bool bCaptureEveryFrame = true;

protected:
	void CaptureAndPublish();
	void ShutdownDds();

	UPROPERTY(VisibleAnywhere, Category = "ROS2 Camera")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	float PublishAccumulator = 0.f;
	int32 DdsWriter = 0;
	void* DdsImageSample = nullptr;
};
