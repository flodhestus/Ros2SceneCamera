#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Lidar360Types.h"
#include "Lidar360PointCloud2Subscriber.generated.h"

UCLASS()
class ROS2SCENECAMERADDS_API ALidar360PointCloud2Subscriber : public AActor
{
	GENERATED_BODY()

public:
	ALidar360PointCloud2Subscriber();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "LiDAR360")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "LiDAR360")
	FString TopicName = TEXT("rt/sensor_pointcloud");

	UPROPERTY(EditAnywhere, Category = "LiDAR360")
	FString PluginName = TEXT("Ros2SceneCamera");

	UPROPERTY(EditAnywhere, Category = "LiDAR360")
	FString ViewportTitle = TEXT("LiDAR360 Point Cloud");

	UPROPERTY(EditAnywhere, Category = "LiDAR360")
	float MaxRangeMeters = LIDAR360_MAX_RANGE_M;

protected:
	void PollDds();
	void OnFrame(const FLidar360SensorFrame& Frame);

	FTimerHandle PollTimer;
	TSharedPtr<class FLidar360Win32Viewport> Viewport;
	int32 DdsReader = 0;
	TArray<float> RenderScratch;
};
