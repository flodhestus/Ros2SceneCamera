#include "Ros2ImageCodec.h"

#if WITH_ROS2_DDS
THIRD_PARTY_INCLUDES_START
#include "Image.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	static char* DupAnsi(const char* S)
	{
		const size_t N = strlen(S) + 1;
		char* P = static_cast<char*>(dds_alloc(N));
		if (P) { FCStringAnsi::Strcpy(P, S); }
		return P;
	}
}
#endif

void FRos2ImageCodec::InitImageSample(sensor_msgs_msg_Image& Sample, int32 MaxWidth, int32 MaxHeight)
{
#if WITH_ROS2_DDS
	const uint32 MaxBytes = static_cast<uint32>(FMath::Max(1, MaxWidth) * FMath::Max(1, MaxHeight) * 3);
	if (Sample.data._maximum < MaxBytes)
	{
		if (Sample.data._buffer && Sample.data._release)
		{
			dds_free(Sample.data._buffer);
		}
		Sample.data._maximum = MaxBytes;
		Sample.data._buffer = static_cast<uint8_t*>(dds_alloc(MaxBytes));
		Sample.data._release = true;
	}
	if (Sample.encoding) { dds_free(Sample.encoding); }
	Sample.encoding = DupAnsi("rgb8");
	Sample.is_bigendian = 0;
#endif
}

bool FRos2ImageCodec::FillRgb8(
	sensor_msgs_msg_Image& Sample,
	const uint8* Rgb,
	int32 Width,
	int32 Height,
	const FString& FrameId)
{
#if WITH_ROS2_DDS
	if (!Rgb || Width <= 0 || Height <= 0 || !Sample.data._buffer)
	{
		return false;
	}
	const uint32 Bytes = static_cast<uint32>(Width * Height * 3);
	if (Bytes > Sample.data._maximum)
	{
		return false;
	}
	FMemory::Memcpy(Sample.data._buffer, Rgb, Bytes);
	Sample.height = static_cast<uint32_t>(Height);
	Sample.width = static_cast<uint32_t>(Width);
	Sample.step = static_cast<uint32_t>(Width * 3);
	Sample.data._length = Bytes;
	Sample.header.stamp.sec = static_cast<int32_t>(FPlatformTime::Seconds());
	Sample.header.stamp.nanosec = 0;
	if (Sample.header.frame_id) { dds_free(Sample.header.frame_id); }
	FTCHARToUTF8 FrameUtf8(*FrameId);
	Sample.header.frame_id = DupAnsi(FrameUtf8.Get());
	return true;
#else
	return false;
#endif
}

bool FRos2ImageCodec::CommitImageMetadata(
	sensor_msgs_msg_Image& Sample,
	int32 Width,
	int32 Height,
	const FString& FrameId)
{
#if WITH_ROS2_DDS
	if (Width <= 0 || Height <= 0 || !Sample.data._buffer)
	{
		return false;
	}
	const uint32 Bytes = static_cast<uint32>(Width * Height * 3);
	if (Bytes > Sample.data._maximum)
	{
		return false;
	}
	Sample.height = static_cast<uint32_t>(Height);
	Sample.width = static_cast<uint32_t>(Width);
	Sample.step = static_cast<uint32_t>(Width * 3);
	Sample.data._length = Bytes;
	Sample.header.stamp.sec = static_cast<int32_t>(FPlatformTime::Seconds());
	Sample.header.stamp.nanosec = 0;
	if (Sample.header.frame_id) { dds_free(Sample.header.frame_id); }
	FTCHARToUTF8 FrameUtf8(*FrameId);
	Sample.header.frame_id = DupAnsi(FrameUtf8.Get());
	return true;
#else
	return false;
#endif
}
