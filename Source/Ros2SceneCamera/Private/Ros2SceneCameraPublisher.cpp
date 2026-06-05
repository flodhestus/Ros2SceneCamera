#include "Ros2SceneCameraPublisher.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Lidar360Dds.h"
#include "Ros2ImageCodec.h"
#include "Ros2SensorCoordinator.h"
#include "SceneCameraCapture.h"

#if WITH_ROS2_DDS
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

void ARos2SceneCameraPublisher::InitializeRenderTarget(int32 Index)
{
	if (!RenderTargets[Index])
	{
		RenderTargets[Index] = NewObject<UTextureRenderTarget2D>(this);
	}
	UTextureRenderTarget2D* Target = RenderTargets[Index];
	Target->InitCustomFormat(ImageWidth, ImageHeight, PF_FloatRGBA, false);
	Target->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA32f;
	Target->bAutoGenerateMips = false;
	Target->TargetGamma = 1.0f;
	Target->bForceLinearGamma = true;
	Target->ClearColor = FLinearColor::Black;
	Target->bGPUSharedFlag = true;
	Target->bCanCreateUAV = true;
	Target->AddressX = TA_Clamp;
	Target->AddressY = TA_Clamp;
	Target->UpdateResourceImmediate(true);
}

void ARos2SceneCameraPublisher::ConfigureSceneCapture()
{
	SceneCapture->TextureTarget = GetActiveRenderTarget();
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;
	SceneCapture->bUseRayTracingIfEnabled = true;
	SceneCapture->bAlwaysPersistRenderingState = true;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;

	FEngineShowFlags& Flags = SceneCapture->ShowFlags;
	Flags.SetPostProcessing(true);
	Flags.SetToneCurve(true);
}

UTextureRenderTarget2D* ARos2SceneCameraPublisher::GetActiveRenderTarget() const
{
	const int32 Index = WriteIndex.load(std::memory_order_relaxed) % 2;
	return RenderTargets[Index];
}

void ARos2SceneCameraPublisher::SwapSamples()
{
	WriteIndex.store((WriteIndex.load(std::memory_order_relaxed) + 1) % 2, std::memory_order_relaxed);
}

void ARos2SceneCameraPublisher::BeginPlay()
{
	Super::BeginPlay();
	if (!bEnabled || !FRos2SensorCoordinator::EnsureDdsInitialized())
	{
		bEnabled = false;
		return;
	}
	if (!FLidar360Dds::CreateImageWriter(TopicName, DdsWriter))
	{
		bEnabled = false;
		return;
	}

	ImageWidth = FMath::Clamp(ImageWidth, 640, ROS2_CAMERA_MAX_WIDTH);
	ImageHeight = FMath::Clamp(ImageHeight, 480, ROS2_CAMERA_MAX_HEIGHT);
	FSceneCameraCapture::Init(ImageWidth, ImageHeight);

	for (int32 i = 0; i < 2; ++i)
	{
		InitializeRenderTarget(i);
		DdsImageSamples[i] = FLidar360Dds::AllocImageSample();
		if (DdsImageSamples[i])
		{
			FRos2ImageCodec::InitImageSample(
				*static_cast<sensor_msgs_msg_Image*>(DdsImageSamples[i]), ImageWidth, ImageHeight);
		}
	}

	WriteIndex.store(0, std::memory_order_relaxed);
	ConfigureSceneCapture();
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
	if (FramesInFlight.load(std::memory_order_acquire) >= MaxFramesInFlight) { return; }
	PublishAccumulator += DeltaSeconds;
	const float Interval = 1.f / FMath::Max(1.f, PublishRateHz);
	if (PublishAccumulator < Interval) { return; }
	PublishAccumulator = 0.f;
	CaptureAndPublish();
}

void ARos2SceneCameraPublisher::ShutdownDds()
{
	FLidar360Dds::DestroyEndpoint(DdsWriter);
	for (int32 i = 0; i < 2; ++i)
	{
		if (DdsImageSamples[i])
		{
			FLidar360Dds::FreeImageSample(static_cast<sensor_msgs_msg_Image*>(DdsImageSamples[i]));
			DdsImageSamples[i] = nullptr;
		}
		RenderTargets[i] = nullptr;
	}
}

void ARos2SceneCameraPublisher::CaptureAndPublish()
{
	const int32 Index = WriteIndex.load(std::memory_order_relaxed) % 2;
	UTextureRenderTarget2D* ActiveTarget = RenderTargets[Index];
	if (!ActiveTarget || !DdsImageSamples[Index] || DdsWriter <= 0) { return; }

	SceneCapture->TextureTarget = ActiveTarget;
	SceneCapture->CaptureScene();

	auto* Sample = static_cast<sensor_msgs_msg_Image*>(DdsImageSamples[Index]);
	int32 W = 0;
	int32 H = 0;
	if (!FSceneCameraCapture::CaptureRgb8Into(
		ActiveTarget,
		Sample->data._buffer,
		static_cast<int32>(Sample->data._maximum),
		W,
		H))
	{
		return;
	}
	if (!FRos2ImageCodec::CommitImageMetadata(*Sample, W, H, FrameId)) { return; }

	FramesInFlight.fetch_add(1, std::memory_order_relaxed);
	SwapSamples();
	SceneCapture->TextureTarget = GetActiveRenderTarget();

	TWeakObjectPtr<ARos2SceneCameraPublisher> WeakThis(this);
	FLidar360Dds::PublishImageAsync(DdsWriter, Sample, [WeakThis](bool)
	{
		if (ARos2SceneCameraPublisher* Self = WeakThis.Get())
		{
			Self->FramesInFlight.fetch_sub(1, std::memory_order_relaxed);
		}
	});
}
