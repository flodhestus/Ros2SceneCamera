#include "UnrealGpuCameraSubscriber.h"
#include "Lidar360Dds.h"
#include "Ros2ImageViewport.h"
#include "Ros2SensorCoordinator.h"
#include "Async/Async.h"

AUnrealGpuCameraSubscriber::AUnrealGpuCameraSubscriber()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AUnrealGpuCameraSubscriber::BeginPlay()
{
	Super::BeginPlay();
	if (!bEnabled) { return; }
	if (!FRos2SensorCoordinator::EnsureDdsInitialized()) { bEnabled = false; return; }
	if (!FLidar360Dds::CreateImageReader(TopicName, DdsReader)) { bEnabled = false; return; }
	Viewport = MakeShared<FRos2ImageViewport>();
	Viewport->StartViewport(ViewportTitle);
	GetWorld()->GetTimerManager().SetTimer(PollTimer, this, &AUnrealGpuCameraSubscriber::PollDds, 0.05f, true);
}

void AUnrealGpuCameraSubscriber::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(PollTimer); }
	if (Viewport.IsValid()) { Viewport->StopViewport(); Viewport.Reset(); }
	FLidar360Dds::DestroyEndpoint(DdsReader);
	Super::EndPlay(EndPlayReason);
}

void AUnrealGpuCameraSubscriber::PollDds()
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis = TWeakObjectPtr<AUnrealGpuCameraSubscriber>(this)]()
	{
		AUnrealGpuCameraSubscriber* Self = WeakThis.Get();
		if (!Self || !Self->bEnabled || Self->DdsReader <= 0) { return; }
		FRos2ImageFrame Frame;
		if (!FLidar360Dds::TakeLatestImage(Self->DdsReader, Frame)) { return; }
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Frame = MoveTemp(Frame)]() mutable
		{
			if (AUnrealGpuCameraSubscriber* Sub = WeakThis.Get()) { Sub->OnFrame(Frame); }
		});
	});
}

void AUnrealGpuCameraSubscriber::OnFrame(const FRos2ImageFrame& Frame)
{
	if (Viewport.IsValid() && Frame.Data.Num() > 0)
	{
		Viewport->SubmitImage(Frame.Data.GetData(), Frame.Width, Frame.Height);
	}
}
