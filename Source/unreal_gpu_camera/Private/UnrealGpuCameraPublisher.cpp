#include "UnrealGpuCameraPublisher.h"
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

AUnrealGpuCameraPublisher::AUnrealGpuCameraPublisher()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SetRootComponent(SceneCapture);
}

void AUnrealGpuCameraPublisher::InitializeRenderTarget(int32 Index)
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

void AUnrealGpuCameraPublisher::ConfigureSceneCapture()
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

UTextureRenderTarget2D* AUnrealGpuCameraPublisher::GetActiveRenderTarget() const
{
	const int32 Index = WriteIndex.load(std::memory_order_relaxed) % 2;
	return RenderTargets[Index];
}

void AUnrealGpuCameraPublisher::SwapSamples()
{
	WriteIndex.store((WriteIndex.load(std::memory_order_relaxed) + 1) % 2, std::memory_order_relaxed);
}

bool AUnrealGpuCameraPublisher::IsSlotAvailable(int32 Index) const
{
	return !SlotReadbackPending[Index];
}

void AUnrealGpuCameraPublisher::BeginPlay()
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

	const float PublishInterval = 1.0f / PublishRateHz;
	GetWorld()->GetTimerManager().SetTimer(
		PublishTimer, this, &AUnrealGpuCameraPublisher::OnPublishTimer, PublishInterval, true);
}

void AUnrealGpuCameraPublisher::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PublishTimer);
	}
	bDeferredCapturePending = false;
	bSceneCaptureInFlight = false;
	SlotReadbackPending[0] = false;
	SlotReadbackPending[1] = false;
	FSceneCameraCapture::Shutdown();
	ShutdownDds();
	Super::EndPlay(EndPlayReason);
}

void AUnrealGpuCameraPublisher::OnPublishTimer()
{
	if (!bEnabled) { return; }
	if (FramesInFlight.load(std::memory_order_acquire) >= MaxFramesInFlight) { return; }
	if (bSceneCaptureInFlight || bDeferredCapturePending) { return; }

	const int32 Index = WriteIndex.load(std::memory_order_relaxed) % 2;
	if (!IsSlotAvailable(Index)) { return; }

	StartDeferredCapture();
}

void AUnrealGpuCameraPublisher::ShutdownDds()
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

void AUnrealGpuCameraPublisher::StartDeferredCapture()
{
	const int32 Index = WriteIndex.load(std::memory_order_relaxed) % 2;
	UTextureRenderTarget2D* ActiveTarget = RenderTargets[Index];
	if (!ActiveTarget || !DdsImageSamples[Index] || DdsWriter <= 0) { return; }

	SceneCapture->TextureTarget = ActiveTarget;
	SceneCapture->CaptureSceneDeferred();
	bDeferredCapturePending = true;
	bSceneCaptureInFlight = true;

	TWeakObjectPtr<AUnrealGpuCameraPublisher> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis, Index]()
	{
		if (AUnrealGpuCameraPublisher* Self = WeakThis.Get())
		{
			Self->BeginGpuReadback(Index);
		}
	});
}

void AUnrealGpuCameraPublisher::BeginGpuReadback(int32 Index)
{
	bDeferredCapturePending = false;
	bSceneCaptureInFlight = false;
	if (!bEnabled) { return; }

	UTextureRenderTarget2D* ActiveTarget = RenderTargets[Index];
	auto* Sample = static_cast<sensor_msgs_msg_Image*>(DdsImageSamples[Index]);
	if (!ActiveTarget || !Sample || DdsWriter <= 0) { return; }

	SlotReadbackPending[Index] = true;
	SwapSamples();
	SceneCapture->TextureTarget = GetActiveRenderTarget();

	TWeakObjectPtr<AUnrealGpuCameraPublisher> WeakThis(this);
	FSceneCameraCapture::EnqueueRgb8Capture(
		ActiveTarget,
		Sample->data._buffer,
		static_cast<int32>(Sample->data._maximum),
		[WeakThis, Index](bool bSuccess, int32 Width, int32 Height)
		{
			if (AUnrealGpuCameraPublisher* Self = WeakThis.Get())
			{
				Self->SlotReadbackPending[Index] = false;
				if (bSuccess)
				{
					Self->PublishCapturedFrame(Index, Width, Height);
				}
			}
		});
}

void AUnrealGpuCameraPublisher::PublishCapturedFrame(int32 Index, int32 Width, int32 Height)
{
	if (!bEnabled) { return; }

	auto* Sample = static_cast<sensor_msgs_msg_Image*>(DdsImageSamples[Index]);
	if (!Sample || DdsWriter <= 0) { return; }
	if (!FRos2ImageCodec::CommitImageMetadata(*Sample, Width, Height, FrameId)) { return; }

	FramesInFlight.fetch_add(1, std::memory_order_relaxed);

	TWeakObjectPtr<AUnrealGpuCameraPublisher> WeakThis(this);
	FLidar360Dds::PublishImageAsync(DdsWriter, Sample, [WeakThis](bool)
	{
		if (AUnrealGpuCameraPublisher* Self = WeakThis.Get())
		{
			Self->FramesInFlight.fetch_sub(1, std::memory_order_relaxed);
		}
	});
}
