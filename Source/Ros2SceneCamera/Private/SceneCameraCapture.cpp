#include "SceneCameraCapture.h"
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
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutPackedRgb)
		SHADER_PARAMETER(FUintVector2, InputSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(
	FSceneCameraCaptureCS,
	"/Ros2SceneCameraShaders/SceneCameraCapture.usf",
	"SceneCameraCaptureCS",
	SF_Compute);

static void UnpackPackedPixels(const TArray<uint32>& Packed, TArray<uint8>& OutRgb)
{
	OutRgb.SetNumUninitialized(Packed.Num() * 3);
	for (int32 i = 0; i < Packed.Num(); ++i)
	{
		const uint32 P = Packed[i];
		OutRgb[i * 3 + 0] = static_cast<uint8>(P & 0xFF);
		OutRgb[i * 3 + 1] = static_cast<uint8>((P >> 8) & 0xFF);
		OutRgb[i * 3 + 2] = static_cast<uint8>((P >> 16) & 0xFF);
	}
}

static bool ReadPixelsCpu(FTextureRenderTargetResource* Resource, int32 Width, int32 Height, TArray<uint8>& OutRgb)
{
	TArray<FColor> Surface;
	Resource->ReadPixels(Surface, FReadSurfaceDataFlags());
	if (Surface.Num() < Width * Height) { return false; }
	OutRgb.SetNumUninitialized(Width * Height * 3);
	for (int32 i = 0; i < Width * Height; ++i)
	{
		OutRgb[i * 3 + 0] = Surface[i].R;
		OutRgb[i * 3 + 1] = Surface[i].G;
		OutRgb[i * 3 + 2] = Surface[i].B;
	}
	return true;
}

bool FSceneCameraCapture::CaptureRgb8(
	UTextureRenderTarget2D* RenderTarget,
	TArray<uint8>& OutRgb,
	int32& OutWidth,
	int32& OutHeight)
{
	if (!RenderTarget) { return false; }
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!Resource) { return false; }

	OutWidth = RenderTarget->SizeX;
	OutHeight = RenderTarget->SizeY;
	const int32 PixelCount = OutWidth * OutHeight;
	if (PixelCount <= 0) { return false; }

	TShaderMapRef<FSceneCameraCaptureCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	if (!ComputeShader.IsValid())
	{
		return ReadPixelsCpu(Resource, OutWidth, OutHeight, OutRgb);
	}

	TArray<uint32> Packed;
	Packed.SetNumZeroed(PixelCount);
	FEvent* Done = FPlatformProcess::GetSynchEventFromPool(true);

	ENQUEUE_RENDER_COMMAND(SceneCameraCapture)(
		[Resource, &Packed, OutWidth, OutHeight, ComputeShader, Done](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			FRDGTextureRef InputTexture = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(Resource->GetRenderTargetTexture(), TEXT("SceneCameraInput")));
			FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
				FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), static_cast<uint32>(OutWidth * OutHeight)),
				TEXT("SceneCameraPacked"));

			FSceneCameraCaptureCS::FParameters* Params = GraphBuilder.AllocParameters<FSceneCameraCaptureCS::FParameters>();
			Params->InputTexture = InputTexture;
			Params->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			Params->OutPackedRgb = GraphBuilder.CreateUAV(OutputBuffer);
			Params->InputSize = FUintVector2(static_cast<uint32>(OutWidth), static_cast<uint32>(OutHeight));

			FComputeShaderUtils::AddPass(
				GraphBuilder, RDG_EVENT_NAME("SceneCameraCaptureCS"), ComputeShader, Params,
				FIntVector(FMath::DivideAndRoundUp(OutWidth, 8), FMath::DivideAndRoundUp(OutHeight, 8), 1));

			FRHIGPUBufferReadback* Readback = new FRHIGPUBufferReadback(TEXT("SceneCameraReadback"));
			AddEnqueueCopyPass(GraphBuilder, Readback, OutputBuffer, 0u);
			GraphBuilder.Execute();

			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			const double Deadline = FPlatformTime::Seconds() + 2.0;
			while (!Readback->IsReady() && FPlatformTime::Seconds() < Deadline)
			{
				FPlatformProcess::SleepNoStats(0.0005f);
			}
			if (Readback->IsReady())
			{
				const uint32 Bytes = static_cast<uint32>(Packed.Num() * sizeof(uint32));
				if (void* Data = Readback->Lock(Bytes))
				{
					FMemory::Memcpy(Packed.GetData(), Data, Bytes);
					Readback->Unlock();
				}
			}
			delete Readback;
			Done->Trigger();
		});

	Done->Wait();
	FPlatformProcess::ReturnSynchEventToPool(Done);
	UnpackPackedPixels(Packed, OutRgb);
	return OutRgb.Num() == PixelCount * 3;
}
