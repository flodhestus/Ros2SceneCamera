#include "Lidar360PointCloud2Subscriber.h"
#include "Lidar360Dds.h"
#include "Lidar360PointCloud2Codec.h"
#include "Lidar360Win32Viewport.h"
#include "Async/Async.h"

#if WITH_LIDAR360_DDS
THIRD_PARTY_INCLUDES_START
#include "sensor_msgs/msg/PointCloud2.h"
THIRD_PARTY_INCLUDES_END
#endif

ALidar360PointCloud2Subscriber::ALidar360PointCloud2Subscriber()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ALidar360PointCloud2Subscriber::BeginPlay()
{
	Super::BeginPlay();
	if (!bEnabled)
	{
		return;
	}
	if (!FLidar360Dds::Init())
	{
		bEnabled = false;
		return;
	}
	if (!FLidar360Dds::CreateReader(TopicName, DdsReader))
	{
		bEnabled = false;
		return;
	}
	Viewport = MakeShared<FLidar360Win32Viewport>();
	Viewport->StartViewport(ViewportTitle);
	GetWorld()->GetTimerManager().SetTimer(PollTimer, this, &ALidar360PointCloud2Subscriber::PollDds, 0.016f, true);
}

void ALidar360PointCloud2Subscriber::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PollTimer);
	}
	if (Viewport.IsValid())
	{
		Viewport->StopViewport();
		Viewport.Reset();
	}
	FLidar360Dds::DestroyEndpoint(DdsReader);
	Super::EndPlay(EndPlayReason);
}

void ALidar360PointCloud2Subscriber::PollDds()
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis = TWeakObjectPtr<ALidar360PointCloud2Subscriber>(this)]()
	{
		ALidar360PointCloud2Subscriber* Self = WeakThis.Get();
		if (!Self || !Self->bEnabled || Self->DdsReader <= 0)
		{
			return;
		}
		FLidar360SensorFrame Frame;
		if (!FLidar360Dds::TakeLatest(Self->DdsReader, Frame))
		{
			return;
		}
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Frame]() mutable
		{
			if (ALidar360PointCloud2Subscriber* Sub = WeakThis.Get())
			{
				Sub->OnFrame(Frame);
			}
		});
	});
}

void ALidar360PointCloud2Subscriber::OnFrame(const FLidar360SensorFrame& Frame)
{
#if WITH_LIDAR360_DDS
	sensor_msgs_msg_PointCloud2 View = {};
	View.data._buffer = Frame.Data.GetData();
	View.data._length = static_cast<uint32_t>(Frame.Data.Num());
	View.width = static_cast<uint32_t>(Frame.PointCount);
	const int32 Count = FLidar360PointCloud2Codec::DecodeToRenderBuffer(View, RenderScratch);
	if (Count > 0 && Viewport.IsValid())
	{
		Viewport->SubmitPoints(RenderScratch.GetData(), Count, MaxRangeMeters);
	}
#endif
}
