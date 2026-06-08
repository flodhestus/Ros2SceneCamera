#include "UnrealGpuCamera.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "UnrealGpuCameraPublisher.h"
#include "UnrealGpuCameraSubscriber.h"
#include "Ros2SensorCoordinator.h"

IMPLEMENT_MODULE(FUnrealGpuCameraModule, unreal_gpu_camera)

static void SpawnPublisher(UWorld* World)
{
	if (!World || World->WorldType != EWorldType::PIE || !FRos2SensorCoordinator::IsSceneCameraEnabled())
	{
		return;
	}
	for (TActorIterator<AUnrealGpuCameraPublisher> It(World); It; ++It) { return; }
	World->SpawnActor<AUnrealGpuCameraPublisher>();
}

static void SpawnSubscriber(UWorld* World)
{
	if (!World || World->WorldType != EWorldType::PIE || !FRos2SensorCoordinator::IsSceneCameraEnabled())
	{
		return;
	}
	for (TActorIterator<AUnrealGpuCameraSubscriber> It(World); It; ++It) { return; }
	World->SpawnActor<AUnrealGpuCameraSubscriber>();
}

void FUnrealGpuCameraModule::StartupModule()
{
	const FString ShaderDir = FPaths::Combine(
		IPluginManager::Get().FindPlugin(TEXT("unreal_gpu_camera"))->GetBaseDir(),
		TEXT("Shaders/unreal_gpu_camera/Private"));
	AddShaderSourceDirectoryMapping(TEXT("/UnrealGpuCameraShaders"), ShaderDir);

	FWorldDelegates::OnPIEStarted.AddLambda([](const bool)
	{
		if (!GEngine) { return; }
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (Ctx.World() && Ctx.WorldType == EWorldType::PIE)
			{
				FRos2SensorCoordinator::EnsureDdsInitialized();
				SpawnPublisher(Ctx.World());
				SpawnSubscriber(Ctx.World());
			}
		}
	});
}

void FUnrealGpuCameraModule::ShutdownModule() {}
