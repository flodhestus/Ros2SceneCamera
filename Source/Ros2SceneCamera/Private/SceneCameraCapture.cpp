#include "SceneCameraCapture.h"
#include "Ros2SceneCameraTypes.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

class FSceneCameraCaptureCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSceneCameraCaptureCS);
	SHADER_USE_PARAMETER_STRUCT(FSceneCameraCaptureCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, InputTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, OutRgb8)
		SHADER_PARAMETER(FUintVector2, InputSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), 16);
	}
};

IMPLEMENT_GLOBAL_SHADER(
	FSceneCameraCaptureCS,
	"/Ros2SceneCameraShaders/SceneCameraCapture.usf",
	"SceneCameraCaptureCS",
	SF_Compute);

namespace
{
	int32 GCachedW = 0;
	int32 GCachedH = 0;
	FRHIGPUBufferReadback* GReadbacks[2] = { nullptr, nullptr };
	int32 GReadbackToggle = 0;
	FCriticalSection GCaptureLock;
}

void FSceneCameraCapture::Init(int32 Width, int32 Height)
{
	FScopeLock Lock(&GCaptureLock);
	GCachedW = FMath::Clamp(Width, 1, ROS2_CAMERA_MAX_WIDTH);
	GCachedH = FMath::Clamp(Height, 1, ROS2_CAMERA_MAX_HEIGHT);
	for (int32 i = 0; i < 2; ++i)
	{
		if (!GReadbacks[i])
		{
			GReadbacks[i] = new FRHIGPUBufferReadback(TEXT("SceneCameraReadback"));
		}
	}
}

void FSceneCameraCapture::Shutdown()
{
	FScopeLock Lock(&GCaptureLock);
	for (int32 i = 0; i < 2; ++i)
	{
		delete GReadbacks[i];
		GReadbacks[i] = nullptr;
	}
	GCachedW = 0;
	GCachedH = 0;
}

bool FSceneCameraCapture::CaptureRgb8Into(
	UTextureRenderTarget2D* RenderTarget,
	uint8* Dest,
	int32 DestCapacityBytes,
	int32& OutWidth,
	int32& OutHeight)
{
	if (!RenderTarget || !Dest) { return false; }
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!Resource) { return false; }

	OutWidth = RenderTarget->SizeX;
	OutHeight = RenderTarget->SizeY;
	const int32 Bytes = OutWidth * OutHeight * 3;
	if (Bytes <= 0 || Bytes > DestCapacityBytes || Bytes > ROS2_CAMERA_MAX_BYTES) { return false; }

	TShaderMapRef<FSceneCameraCaptureCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	if (!ComputeShader.IsValid()) { return false; }

	FRHIGPUBufferReadback* Readback = nullptr;
	{
		FScopeLock Lock(&GCaptureLock);
		Readback = GReadbacks[GReadbackToggle & 1];
		GReadbackToggle++;
	}
	if (!Readback) { return false; }

	FEvent* Done = FPlatformProcess::GetSynchEventFromPool(true);

	ENQUEUE_RENDER_COMMAND(SceneCameraCapture)(
		[Resource, OutWidth, OutHeight, Bytes, ComputeShader, Readback, Done](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			FRDGTextureRef InputTexture = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(Resource->GetRenderTargetTexture(), TEXT("SceneCameraInput")));
			FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateByteAddressDesc(static_cast<uint32>(Bytes)),
				TEXT("SceneCameraRgb8"));

			FSceneCameraCaptureCS::FParameters* Params = GraphBuilder.AllocParameters<FSceneCameraCaptureCS::FParameters>();
			Params->InputTexture = InputTexture;
			Params->OutRgb8 = GraphBuilder.CreateUAV(OutputBuffer);
			Params->InputSize = FUintVector2(static_cast<uint32>(OutWidth), static_cast<uint32>(OutHeight));

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("SceneCameraCaptureCS"),
				ComputeShader,
				Params,
				FIntVector(
					FMath::DivideAndRoundUp(OutWidth, 16),
					FMath::DivideAndRoundUp(OutHeight, 16),
					1));

			AddEnqueueCopyPass(GraphBuilder, Readback, OutputBuffer, 0u);
			GraphBuilder.Execute();

			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			Done->Trigger();
		});

	Done->Wait();
	FPlatformProcess::ReturnSynchEventToPool(Done);

	const double Deadline = FPlatformTime::Seconds() + 1.0;
	while (!Readback->IsReady() && FPlatformTime::Seconds() < Deadline)
	{
		FPlatformProcess::SleepNoStats(0.0);
	}
	if (!Readback->IsReady()) { return false; }

	if (void* Data = Readback->Lock(static_cast<uint32>(Bytes)))
	{
		FMemory::Memcpy(Dest, Data, Bytes);
		Readback->Unlock();
	}
	return true;
}
