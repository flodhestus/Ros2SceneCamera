#include "Ros2SceneCameraSubscriber.h"
#include "Ros2ImageViewport.h"
#include "Ros2SceneCameraDds.h"
#include "Async/Async.h"

ARos2SceneCameraSubscriber::ARos2SceneCameraSubscriber()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ARos2SceneCameraSubscriber::BeginPlay()
{
	Super::BeginPlay();
	if (!bEnabled) { return; }
	if (!FRos2SceneCameraDds::Init()) { bEnabled = false; return; }
	if (!FRos2SceneCameraDds::CreateImageReader(TopicName, DdsReader)) { bEnabled = false; return; }
	Viewport = MakeShared<FRos2ImageViewport>();
	Viewport->StartViewport(ViewportTitle);
	GetWorld()->GetTimerManager().SetTimer(PollTimer, this, &ARos2SceneCameraSubscriber::PollDds, 0.05f, true);
}

void ARos2SceneCameraSubscriber::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(PollTimer); }
	if (Viewport.IsValid()) { Viewport->StopViewport(); Viewport.Reset(); }
	FRos2SceneCameraDds::DestroyEndpoint(DdsReader);
	Super::EndPlay(EndPlayReason);
}

void ARos2SceneCameraSubscriber::PollDds()
{
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis = TWeakObjectPtr<ARos2SceneCameraSubscriber>(this)]()
	{
		ARos2SceneCameraSubscriber* Self = WeakThis.Get();
		if (!Self || !Self->bEnabled || Self->DdsReader <= 0) { return; }
		FRos2ImageFrame Frame;
		if (!FRos2SceneCameraDds::TakeLatestImage(Self->DdsReader, Frame)) { return; }
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Frame = MoveTemp(Frame)]() mutable
		{
			if (ARos2SceneCameraSubscriber* Sub = WeakThis.Get()) { Sub->OnFrame(Frame); }
		});
	});
}

void ARos2SceneCameraSubscriber::OnFrame(const FRos2ImageFrame& Frame)
{
	if (Viewport.IsValid() && Frame.Data.Num() > 0)
	{
		Viewport->SubmitImage(Frame.Data.GetData(), Frame.Width, Frame.Height);
	}
}
