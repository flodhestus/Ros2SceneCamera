#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnrealGpuCameraTypes.h"
#include "UnrealGpuCameraSubscriber.generated.h"

UCLASS()
class UNREAL_GPU_CAMERA_API AUnrealGpuCameraSubscriber : public AActor
{
	GENERATED_BODY()

public:
	AUnrealGpuCameraSubscriber();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera")
	FString TopicName = TEXT("rt/sensor_image");

	UPROPERTY(EditAnywhere, Category = "ROS2 Camera")
	FString ViewportTitle = TEXT("Scene Camera");

protected:
	void PollDds();
	void OnFrame(const FRos2ImageFrame& Frame);

	FTimerHandle PollTimer;
	TSharedPtr<class FRos2ImageViewport> Viewport;
	int32 DdsReader = 0;
};
