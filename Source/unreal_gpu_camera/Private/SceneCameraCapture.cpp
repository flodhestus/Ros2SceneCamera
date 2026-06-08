#include "SceneCameraCapture.h"
#include "UnrealGpuCameraTypes.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Containers/Ticker.h"
#include "Async/Async.h"
#include "RHIResources.h"

class FSceneCameraCaptureCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSceneCameraCaptureCS);
	SHADER_USE_PARAMETER_STRUCT(FSceneCameraCaptureCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, InputTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutRgb8Packed)
		SHADER_PARAMETER(FUintVector2, InputSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6)
			|| IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_X"), 8);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_Y"), 8);
		if (FDataDrivenShaderPlatformInfo::GetSupportsWaveOperations(Parameters.Platform))
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(
	FSceneCameraCaptureCS,
	"/UnrealGpuCameraShaders/SceneCameraCapture.usf",
	"SceneCameraCaptureCS",
	SF_Compute);

namespace
{
	constexpr int32 kNumPipelineSlots = 2;
	constexpr int32 kThreadGroupSizeX = 8;
	constexpr int32 kThreadGroupSizeY = 8;

	struct FPendingGpuCapture
	{
		FRHIGPUBufferReadback* Readback = nullptr;
		uint8* Dest = nullptr;
		int32 DestBytes = 0;
		int32 Width = 0;
		int32 Height = 0;
		TFunction<void(bool, int32, int32)> OnComplete;
	};

	int32 GCachedW = 0;
	int32 GCachedH = 0;
	int32 GSlotToggle = 0;
	FRHIGPUBufferReadback* GReadbacks[kNumPipelineSlots] = { nullptr, nullptr };
	FBufferRHIRef GOutputBuffers[kNumPipelineSlots];
	TArray<FPendingGpuCapture> GPendingCaptures;
	FTSTicker::FDelegateHandle GReadbackTickerHandle;
	FCriticalSection GCaptureLock;

	void UnpackRgb8Packed(const uint32* Src, uint8* Dest, int32 NumPixels)
	{
		for (int32 i = 0; i < NumPixels; ++i)
		{
			const uint32 Packed = Src[i];
			const int32 Base = i * 3;
			Dest[Base + 0] = static_cast<uint8>(Packed & 0xFFu);
			Dest[Base + 1] = static_cast<uint8>((Packed >> 8) & 0xFFu);
			Dest[Base + 2] = static_cast<uint8>((Packed >> 16) & 0xFFu);
		}
	}

	bool PollPendingReadbacks(float)
	{
		TArray<FPendingGpuCapture> ReadyCaptures;
		{
			FScopeLock Lock(&GCaptureLock);
			for (int32 i = GPendingCaptures.Num() - 1; i >= 0; --i)
			{
				FPendingGpuCapture& Pending = GPendingCaptures[i];
				if (!Pending.Readback || !Pending.Readback->IsReady())
				{
					continue;
				}
				ReadyCaptures.Add(MoveTemp(Pending));
				GPendingCaptures.RemoveAtSwap(i);
			}
		}

		for (FPendingGpuCapture& Pending : ReadyCaptures)
		{
			const int32 NumPixels = Pending.Width * Pending.Height;
			const int32 PackedBytes = NumPixels * static_cast<int32>(sizeof(uint32));
			const int32 W = Pending.Width;
			const int32 H = Pending.Height;
			TFunction<void(bool, int32, int32)> OnComplete = MoveTemp(Pending.OnComplete);
			uint8* Dest = Pending.Dest;

			if (!Pending.Readback)
			{
				if (OnComplete) { OnComplete(false, 0, 0); }
				continue;
			}

			if (void* Data = Pending.Readback->Lock(PackedBytes))
			{
				TArray<uint32> PackedCopy;
				PackedCopy.SetNumUninitialized(NumPixels);
				FMemory::Memcpy(PackedCopy.GetData(), Data, PackedBytes);
				Pending.Readback->Unlock();

				AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
					[PackedCopy = MoveTemp(PackedCopy), Dest, NumPixels, W, H, OnComplete = MoveTemp(OnComplete)]() mutable
					{
						UnpackRgb8Packed(PackedCopy.GetData(), Dest, NumPixels);
						if (OnComplete)
						{
							AsyncTask(ENamedThreads::GameThread, [OnComplete = MoveTemp(OnComplete), W, H]()
							{
								OnComplete(true, W, H);
							});
						}
					});
			}
			else if (OnComplete)
			{
				OnComplete(false, 0, 0);
			}
		}

		FScopeLock Lock(&GCaptureLock);
		return !GPendingCaptures.IsEmpty();
	}

	void EnsureReadbackTicker()
	{
		if (GReadbackTickerHandle.IsValid())
		{
			return;
		}
		GReadbackTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateStatic(&PollPendingReadbacks), 0.0f);
	}

	void ReleaseReadbackTicker()
	{
		if (!GReadbackTickerHandle.IsValid())
		{
			return;
		}
		FTSTicker::GetCoreTicker().RemoveTicker(GReadbackTickerHandle);
		GReadbackTickerHandle.Reset();
	}
}

void FSceneCameraCapture::Init(int32 Width, int32 Height)
{
	FScopeLock Lock(&GCaptureLock);
	GCachedW = FMath::Clamp(Width, 1, ROS2_CAMERA_MAX_WIDTH);
	GCachedH = FMath::Clamp(Height, 1, ROS2_CAMERA_MAX_HEIGHT);
	const int32 NumPixels = GCachedW * GCachedH;
	const uint32 PackedBytes = static_cast<uint32>(NumPixels * sizeof(uint32));

	for (int32 i = 0; i < kNumPipelineSlots; ++i)
	{
		if (!GReadbacks[i])
		{
			GReadbacks[i] = new FRHIGPUBufferReadback(TEXT("SceneCameraReadback"));
		}

		if (!GOutputBuffers[i].IsValid() || GOutputBuffers[i]->GetSize() < PackedBytes)
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("SceneCameraRgb8Packed"));
			GOutputBuffers[i] = RHICreateBuffer(
				PackedBytes,
				BUF_UnorderedAccess | BUF_ShaderResource | BUF_SourceCopy,
				sizeof(uint32),
				ERHIAccess::UAVCompute,
				CreateInfo);
		}
	}
}

void FSceneCameraCapture::Shutdown()
{
	ReleaseReadbackTicker();

	FScopeLock Lock(&GCaptureLock);
	GPendingCaptures.Reset();
	for (int32 i = 0; i < kNumPipelineSlots; ++i)
	{
		delete GReadbacks[i];
		GReadbacks[i] = nullptr;
		GOutputBuffers[i].SafeRelease();
	}
	GCachedW = 0;
	GCachedH = 0;
	GSlotToggle = 0;
}

void FSceneCameraCapture::EnqueueRgb8Capture(
	UTextureRenderTarget2D* RenderTarget,
	uint8* Dest,
	int32 DestCapacityBytes,
	TFunction<void(bool, int32, int32)> OnComplete)
{
	if (!RenderTarget || !Dest || !OnComplete)
	{
		if (OnComplete) { OnComplete(false, 0, 0); }
		return;
	}

	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!Resource)
	{
		OnComplete(false, 0, 0);
		return;
	}

	const int32 Width = RenderTarget->SizeX;
	const int32 Height = RenderTarget->SizeY;
	const int32 DestBytes = Width * Height * 3;
	const int32 NumPixels = Width * Height;
	const int32 PackedBytes = NumPixels * static_cast<int32>(sizeof(uint32));
	if (DestBytes <= 0 || DestBytes > DestCapacityBytes || DestBytes > ROS2_CAMERA_MAX_BYTES)
	{
		OnComplete(false, 0, 0);
		return;
	}

	const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
	TShaderMapRef<FSceneCameraCaptureCS> ComputeShader(GetGlobalShaderMap(FeatureLevel));
	if (!ComputeShader.IsValid())
	{
		OnComplete(false, 0, 0);
		return;
	}

	FRHIGPUBufferReadback* Readback = nullptr;
	FBufferRHIRef OutputBuffer;
	{
		FScopeLock Lock(&GCaptureLock);
		Readback = GReadbacks[GSlotToggle & 1];
		OutputBuffer = GOutputBuffers[GSlotToggle & 1];
		GSlotToggle++;
	}
	if (!Readback || !OutputBuffer.IsValid() || OutputBuffer->GetSize() < static_cast<uint32>(PackedBytes))
	{
		OnComplete(false, 0, 0);
		return;
	}

	{
		FScopeLock Lock(&GCaptureLock);
		FPendingGpuCapture Pending;
		Pending.Readback = Readback;
		Pending.Dest = Dest;
		Pending.DestBytes = DestBytes;
		Pending.Width = Width;
		Pending.Height = Height;
		Pending.OnComplete = MoveTemp(OnComplete);
		GPendingCaptures.Add(MoveTemp(Pending));
	}
	EnsureReadbackTicker();

	ENQUEUE_RENDER_COMMAND(SceneCameraCapture)(
		[Resource, Width, Height, NumPixels, ComputeShader, Readback, OutputBuffer](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			FRDGTextureRef InputTexture = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(Resource->GetRenderTargetTexture(), TEXT("SceneCameraInput")));
			FRDGBufferRef OutputBufferRDG = GraphBuilder.RegisterExternalBuffer(
				OutputBuffer,
				TEXT("SceneCameraRgb8Packed"));

			FSceneCameraCaptureCS::FParameters* Params = GraphBuilder.AllocParameters<FSceneCameraCaptureCS::FParameters>();
			Params->InputTexture = InputTexture;
			Params->OutRgb8Packed = GraphBuilder.CreateUAV(OutputBufferRDG);
			Params->InputSize = FUintVector2(static_cast<uint32>(Width), static_cast<uint32>(Height));

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("SceneCameraCaptureCS"),
				ComputeShader,
				Params,
				FIntVector(
					FMath::DivideAndRoundUp(Width, kThreadGroupSizeX),
					FMath::DivideAndRoundUp(Height, kThreadGroupSizeY),
					1));

			AddEnqueueCopyPass(GraphBuilder, Readback, OutputBufferRDG, 0u);
			GraphBuilder.Execute();
		});
}
