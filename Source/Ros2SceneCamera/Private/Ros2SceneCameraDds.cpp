#include "Ros2SceneCameraDds.h"
#include "Async/Async.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#if WITH_ROS2_SCENE_CAMERA_DDS
THIRD_PARTY_INCLUDES_START
#include "dds/dds.h"
#include "Image.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	dds_entity_t GParticipant = 0;
	dds_entity_t GDomain = 0;

	FString ConfigUri()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Ros2SceneCamera"));
		if (!Plugin.IsValid())
		{
			return FString();
		}
		const FString Path = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config/CycloneDDS.xml"));
		return FString::Printf(TEXT("file:///%s"), *Path.Replace(TEXT("\\"), TEXT("/")));
	}
}
#endif

bool FRos2SceneCameraDds::Init()
{
#if WITH_ROS2_SCENE_CAMERA_DDS
	if (GParticipant > 0)
	{
		return true;
	}
	const FString Uri = ConfigUri();
	if (!Uri.IsEmpty())
	{
		FPlatformMisc::SetEnvironmentVar(TEXT("CYCLONEDDS_URI"), *Uri);
	}
	GDomain = dds_create_domain(DDS_DOMAIN_DEFAULT, nullptr);
	GParticipant = dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr);
	return GParticipant > 0;
#else
	return false;
#endif
}

void FRos2SceneCameraDds::Shutdown()
{
#if WITH_ROS2_SCENE_CAMERA_DDS
	if (GParticipant > 0)
	{
		dds_delete(GParticipant);
		GParticipant = 0;
	}
	if (GDomain > 0)
	{
		dds_delete(GDomain);
		GDomain = 0;
	}
#endif
}

bool FRos2SceneCameraDds::CreateImageWriter(const FString& TopicName, int32& OutWriter)
{
#if WITH_ROS2_SCENE_CAMERA_DDS
	if (GParticipant <= 0)
	{
		return false;
	}
	FTCHARToUTF8 TopicUtf8(*TopicName);
	dds_entity_t Topic = dds_create_topic(
		GParticipant, &sensor_msgs_msg_Image_desc, TopicUtf8.Get(), nullptr, nullptr);
	if (Topic < 0)
	{
		return false;
	}
	dds_entity_t Writer = dds_create_writer(GParticipant, Topic, nullptr, nullptr);
	if (Writer < 0)
	{
		dds_delete(Topic);
		return false;
	}
	OutWriter = static_cast<int32>(Writer);
	return true;
#else
	return false;
#endif
}

bool FRos2SceneCameraDds::CreateImageReader(const FString& TopicName, int32& OutReader)
{
#if WITH_ROS2_SCENE_CAMERA_DDS
	if (GParticipant <= 0)
	{
		return false;
	}
	FTCHARToUTF8 TopicUtf8(*TopicName);
	dds_entity_t Topic = dds_create_topic(
		GParticipant, &sensor_msgs_msg_Image_desc, TopicUtf8.Get(), nullptr, nullptr);
	if (Topic < 0)
	{
		return false;
	}
	dds_entity_t Reader = dds_create_reader(GParticipant, Topic, nullptr, nullptr);
	if (Reader < 0)
	{
		dds_delete(Topic);
		return false;
	}
	OutReader = static_cast<int32>(Reader);
	return true;
#else
	return false;
#endif
}

void FRos2SceneCameraDds::DestroyEndpoint(int32& Entity)
{
#if WITH_ROS2_SCENE_CAMERA_DDS
	if (Entity > 0)
	{
		dds_delete(static_cast<dds_entity_t>(Entity));
		Entity = 0;
	}
#endif
}

sensor_msgs_msg_Image* FRos2SceneCameraDds::AllocImageSample()
{
#if WITH_ROS2_SCENE_CAMERA_DDS
	auto* Sample = sensor_msgs_msg_Image__alloc();
	if (!Sample)
	{
		return nullptr;
	}
	Sample->data._maximum = ROS2_CAMERA_MAX_BYTES;
	Sample->data._buffer = static_cast<uint8_t*>(dds_alloc(ROS2_CAMERA_MAX_BYTES));
	Sample->data._release = true;
	return Sample;
#else
	return nullptr;
#endif
}

void FRos2SceneCameraDds::FreeImageSample(sensor_msgs_msg_Image* Sample)
{
#if WITH_ROS2_SCENE_CAMERA_DDS
	if (Sample)
	{
		sensor_msgs_msg_Image_free(Sample, DDS_FREE_ALL);
	}
#endif
}

void FRos2SceneCameraDds::PublishImageAsync(int32 Writer, sensor_msgs_msg_Image* Sample, TFunction<void(bool)> OnDone)
{
#if WITH_ROS2_SCENE_CAMERA_DDS
	if (Writer <= 0 || !Sample)
	{
		if (OnDone) { OnDone(false); }
		return;
	}
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Writer, Sample, OnDone = MoveTemp(OnDone)]()
	{
		const bool bOk = dds_write(static_cast<dds_entity_t>(Writer), Sample) >= 0;
		if (OnDone)
		{
			AsyncTask(ENamedThreads::GameThread, [OnDone = MoveTemp(OnDone), bOk]() { OnDone(bOk); });
		}
	});
#else
	if (OnDone) { OnDone(false); }
#endif
}

bool FRos2SceneCameraDds::TakeLatestImage(int32 Reader, FRos2ImageFrame& OutFrame)
{
#if WITH_ROS2_SCENE_CAMERA_DDS
	if (Reader <= 0)
	{
		return false;
	}
	dds_sample_info_t Info;
	void* Buffer = sensor_msgs_msg_Image__alloc();
	const int32 N = dds_take(static_cast<dds_entity_t>(Reader), &Buffer, &Info, 1, 1);
	if (N <= 0 || !Buffer)
	{
		if (Buffer) { sensor_msgs_msg_Image_free(Buffer, DDS_FREE_ALL); }
		return false;
	}
	auto* Msg = static_cast<sensor_msgs_msg_Image*>(Buffer);
	OutFrame.Width = static_cast<int32>(Msg->width);
	OutFrame.Height = static_cast<int32>(Msg->height);
	OutFrame.Step = static_cast<int32>(Msg->step);
	const uint32 Len = Msg->data._length;
	OutFrame.Data.SetNumUninitialized(Len);
	if (Len > 0 && Msg->data._buffer)
	{
		FMemory::Memcpy(OutFrame.Data.GetData(), Msg->data._buffer, Len);
	}
	sensor_msgs_msg_Image_free(Buffer, DDS_FREE_ALL);
	return OutFrame.Width > 0 && OutFrame.Height > 0 && OutFrame.Data.Num() > 0;
#else
	return false;
#endif
}
