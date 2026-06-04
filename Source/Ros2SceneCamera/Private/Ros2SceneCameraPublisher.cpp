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
	PrimaryActorTick.bCanEverTick = false;
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
	DdsImageSample = FRos2SceneCameraDds::AllocImageSample();
	if (DdsImageSample)
	{
		FRos2ImageCodec::InitImageSample(
			*static_cast<sensor_msgs_msg_Image*>(DdsImageSample), ImageWidth, ImageHeight);
	}
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->InitAutoFormat(ImageWidth, ImageHeight);
	RenderTarget->UpdateResourceImmediate(true);
	SceneCapture->TextureTarget = RenderTarget;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle, this, &ARos2SceneCameraPublisher::OnTimer,
		1.f / FMath::Max(1.f, PublishRateHz), true);
}

void ARos2SceneCameraPublisher::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(TimerHandle); }
	ShutdownDds();
	Super::EndPlay(EndPlayReason);
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

void ARos2SceneCameraPublisher::OnTimer()
{
	if (!bEnabled || !RenderTarget || !DdsImageSample || DdsWriter <= 0) { return; }
	SceneCapture->CaptureScene();
	TArray<uint8> Rgb;
	int32 W = 0, H = 0;
	if (!FSceneCameraCapture::CaptureRgb8(RenderTarget, Rgb, W, H)) { return; }
	auto* Sample = static_cast<sensor_msgs_msg_Image*>(DdsImageSample);
	if (!FRos2ImageCodec::FillRgb8(*Sample, Rgb.GetData(), W, H, FrameId)) { return; }
	FRos2SceneCameraDds::PublishImageAsync(DdsWriter, Sample);
}
