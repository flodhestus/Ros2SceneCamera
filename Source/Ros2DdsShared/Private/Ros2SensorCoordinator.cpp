#include "Ros2SensorCoordinator.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Lidar360Dds.h"

static bool IsPluginEnabled(const FName PluginName)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	return Plugin.IsValid() && Plugin->IsEnabled();
}

bool FRos2SensorCoordinator::IsOptiXLidarEnabled()
{
	return false;
}

bool FRos2SensorCoordinator::IsGpuLidarEnabled()
{
	return false;
}

bool FRos2SensorCoordinator::IsSceneCameraEnabled()
{
	return IsPluginEnabled(TEXT("Ros2SceneCamera"));
}

bool FRos2SensorCoordinator::ShouldRunOptiXLidar()
{
	return false;
}

bool FRos2SensorCoordinator::ShouldRunGpuLidar()
{
	return false;
}

bool FRos2SensorCoordinator::EnsureDdsInitialized()
{
	return FLidar360Dds::Init();
}

void FRos2SensorCoordinator::OnPieWorldStarted(UWorld* World)
{
	if (!World || World->WorldType != EWorldType::PIE)
	{
		return;
	}
	EnsureDdsInitialized();
}
