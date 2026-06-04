#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Ros2SceneCameraTypes.h"
#include "Ros2SceneCameraSubscriber.generated.h"

UCLASS()
class ROS2SCENECAMERA_API ARos2SceneCameraSubscriber : public AActor
{
	GENERATED_BODY()

public:
	ARos2SceneCameraSubscriber();
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
