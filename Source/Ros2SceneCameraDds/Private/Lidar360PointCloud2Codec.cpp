#include "Lidar360PointCloud2Codec.h"

#if WITH_LIDAR360_DDS
THIRD_PARTY_INCLUDES_START
#include "sensor_msgs/msg/PointCloud2.h"
#include "sensor_msgs/msg/PointField.h"
THIRD_PARTY_INCLUDES_END
#endif

#if WITH_LIDAR360_DDS
namespace
{
	static char* DupAnsi(const char* S)
	{
		const size_t N = strlen(S) + 1;
		char* P = static_cast<char*>(dds_alloc(N));
		if (P) { FCStringAnsi::Strcpy(P, S); }
		return P;
	}

	static void SetField(sensor_msgs_msg_PointField& F, const char* Name, uint32 Offset)
	{
		if (F.name) { dds_free(F.name); }
		F.name = DupAnsi(Name);
		F.offset = Offset;
		F.datatype = 7;
		F.count = 1;
	}
}
#endif

void FLidar360PointCloud2Codec::InitSampleFields(sensor_msgs_msg_PointCloud2& Sample)
{
#if WITH_LIDAR360_DDS
	if (Sample.fields._length < 4 || !Sample.fields._buffer)
	{
		return;
	}
	SetField(Sample.fields._buffer[0], "x", 0);
	SetField(Sample.fields._buffer[1], "y", 4);
	SetField(Sample.fields._buffer[2], "z", 8);
	SetField(Sample.fields._buffer[3], "intensity", 12);
	Sample.point_step = LIDAR360_POINT_BYTES;
	Sample.is_bigendian = false;
	Sample.is_dense = true;
#endif
}

int32 FLidar360PointCloud2Codec::PackFromGpuHits(
	sensor_msgs_msg_PointCloud2& Sample,
	const uint8* GpuData,
	int32 NumRays,
	const FVector& OriginCm,
	const FVector& Forward,
	const FVector& Right,
	const FVector& Up,
	const FString& FrameId)
{
#if WITH_LIDAR360_DDS
	if (!GpuData || NumRays <= 0 || !Sample.data._buffer)
	{
		return 0;
	}
	const FVector Fwd = Forward.GetSafeNormal();
	const FVector Rt = Right.GetSafeNormal();
	const FVector UpN = Up.GetSafeNormal();
	const int32 MaxPoints = static_cast<int32>(Sample.data._maximum / LIDAR360_POINT_BYTES);
	const int32 Count = FMath::Min(NumRays, MaxPoints);
	float* Out = reinterpret_cast<float*>(Sample.data._buffer);
	int32 Written = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		const float* S = reinterpret_cast<const float*>(GpuData + i * LIDAR360_GPU_STRIDE);
		const FVector W(S[0], S[1], S[2]);
		const FVector R = W - OriginCm;
		const float Xm = FVector::DotProduct(R, Fwd) * 0.01f;
		const float Ym = FVector::DotProduct(R, Rt) * 0.01f;
		const float Zm = FVector::DotProduct(R, UpN) * 0.01f;
		if (!FMath::IsFinite(Xm) || !FMath::IsFinite(Ym) || !FMath::IsFinite(Zm))
		{
			continue;
		}
		const uint32 B = static_cast<uint32>(Written) * 4;
		Out[B + 0] = Xm;
		Out[B + 1] = Ym;
		Out[B + 2] = Zm;
		Out[B + 3] = FMath::Clamp(S[3], 0.f, 1.f);
		++Written;
	}
	Sample.height = 1;
	Sample.width = static_cast<uint32_t>(Written);
	Sample.row_step = LIDAR360_POINT_BYTES * Sample.width;
	Sample.data._length = Sample.row_step;
	Sample.header.stamp.sec = static_cast<int32_t>(FPlatformTime::Seconds());
	Sample.header.stamp.nanosec = 0;
	if (Sample.header.frame_id) { dds_free(Sample.header.frame_id); }
	FTCHARToUTF8 FrameUtf8(*FrameId);
	Sample.header.frame_id = DupAnsi(FrameUtf8.Get());
	return Written;
#else
	return 0;
#endif
}

int32 FLidar360PointCloud2Codec::DecodeToRenderBuffer(
	const sensor_msgs_msg_PointCloud2& Sample,
	TArray<float>& OutXyzIntensity,
	int32 MaxPoints)
{
#if WITH_LIDAR360_DDS
	const int32 Count = FMath::Min(static_cast<int32>(Sample.width), MaxPoints);
	const uint32 Bytes = Count * LIDAR360_POINT_BYTES;
	if (Count <= 0 || Sample.data._length < Bytes || !Sample.data._buffer)
	{
		return 0;
	}
	OutXyzIntensity.SetNumUninitialized(Count * 4);
	FMemory::Memcpy(OutXyzIntensity.GetData(), Sample.data._buffer, Bytes);
	return Count;
#else
	return 0;
#endif
}
