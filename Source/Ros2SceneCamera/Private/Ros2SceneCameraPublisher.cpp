#include "Ros2SceneCameraPublisher.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Ros2ImageCodec.h"
#include "Ros2SceneCameraDds.h"
#include "SceneCameraCapture.h"

#if WITH_ROS2_SCENE_CAMERA_DDS
THIRD_PARTY_INCLUDES_START
#include "Image.h"
THIRD_PARTY_INCLUDES_END
#endif

ARos2SceneCameraPublisher::ARos2SceneCameraPublisher()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SetRootComponent(SceneCapture);
}

void ARos2SceneCameraPublisher::BeginPlay()
{
	Super::BeginPlay();
	if (!bEnabled || !FRos2SceneCameraDds::Init())
	{
		bEnabled = false;
		return;
	}
	if (!FRos2SceneCameraDds::CreateImageWriter(TopicName, DdsWriter))
	{
		bEnabled = false;
		return;
	}

	ImageWidth = FMath::Clamp(ImageWidth, 640, ROS2_CAMERA_MAX_WIDTH);
	ImageHeight = FMath::Clamp(ImageHeight, 480, ROS2_CAMERA_MAX_HEIGHT);
	FSceneCameraCapture::Init(ImageWidth, ImageHeight);

	DdsImageSample = FRos2SceneCameraDds::AllocImageSample();
	if (DdsImageSample)
	{
		FRos2ImageCodec::InitImageSample(
			*static_cast<sensor_msgs_msg_Image*>(DdsImageSample), ImageWidth, ImageHeight);
	}

	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->InitCustomFormat(ImageWidth, ImageHeight, PF_B8G8R8A8, false);
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->UpdateResourceImmediate(true);

	SceneCapture->TextureTarget = RenderTarget;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->bCaptureEveryFrame = bCaptureEveryFrame;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->bAlwaysPersistRenderingState = true;
}

void ARos2SceneCameraPublisher::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FSceneCameraCapture::Shutdown();
	ShutdownDds();
	Super::EndPlay(EndPlayReason);
}

void ARos2SceneCameraPublisher::Tick(float DeltaSeconds)
{
	if (!bEnabled) { return; }
	PublishAccumulator += DeltaSeconds;
	const float Interval = 1.f / FMath::Max(1.f, PublishRateHz);
	if (PublishAccumulator < Interval) { return; }
	PublishAccumulator = 0.f;
	CaptureAndPublish();
}

void ARos2SceneCameraPublisher::ShutdownDds()
{
	FRos2SceneCameraDds::DestroyEndpoint(DdsWriter);
	if (DdsImageSample)
	{
		FRos2SceneCameraDds::FreeImageSample(static_cast<sensor_msgs_msg_Image*>(DdsImageSample));
		DdsImageSample = nullptr;
	}
}

void ARos2SceneCameraPublisher::CaptureAndPublish()
{
	if (!RenderTarget || !DdsImageSample || DdsWriter <= 0) { return; }

	if (!bCaptureEveryFrame)
	{
		SceneCapture->CaptureScene();
	}

	auto* Sample = static_cast<sensor_msgs_msg_Image*>(DdsImageSample);
	int32 W = 0;
	int32 H = 0;
	if (!FSceneCameraCapture::CaptureRgb8Into(
		RenderTarget,
		Sample->data._buffer,
		static_cast<int32>(Sample->data._maximum),
		W,
		H))
	{
		return;
	}
	if (!FRos2ImageCodec::CommitImageMetadata(*Sample, W, H, FrameId)) { return; }
	FRos2SceneCameraDds::PublishImageAsync(DdsWriter, Sample);
}
