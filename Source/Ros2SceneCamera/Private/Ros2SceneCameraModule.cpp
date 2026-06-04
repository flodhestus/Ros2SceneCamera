#include "Ros2SceneCamera.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Ros2SceneCameraPublisher.h"
#include "Ros2SceneCameraSubscriber.h"
#include "Ros2SensorCoordinator.h"

IMPLEMENT_MODULE(FRos2SceneCameraModule, Ros2SceneCamera)

static void SpawnPublisher(UWorld* World)
{
	if (!World || World->WorldType != EWorldType::PIE || !FRos2SensorCoordinator::IsSceneCameraEnabled())
	{
		return;
	}
	for (TActorIterator<ARos2SceneCameraPublisher> It(World); It; ++It) { return; }
	World->SpawnActor<ARos2SceneCameraPublisher>();
}

static void SpawnSubscriber(UWorld* World)
{
	if (!World || World->WorldType != EWorldType::PIE || !FRos2SensorCoordinator::IsSceneCameraEnabled())
	{
		return;
	}
	for (TActorIterator<ARos2SceneCameraSubscriber> It(World); It; ++It) { return; }
	World->SpawnActor<ARos2SceneCameraSubscriber>();
}

void FRos2SceneCameraModule::StartupModule()
{
	const FString ShaderDir = FPaths::Combine(
		IPluginManager::Get().FindPlugin(TEXT("Ros2SceneCamera"))->GetBaseDir(),
		TEXT("Shaders/Ros2SceneCamera/Private"));
	AddShaderSourceDirectoryMapping(TEXT("/Ros2SceneCameraShaders"), ShaderDir);

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

void FRos2SceneCameraModule::ShutdownModule() {}
