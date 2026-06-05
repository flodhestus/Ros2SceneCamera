#include "Ros2SceneCameraDds.h"
#include "Engine/Engine.h"
#include "Lidar360Dds.h"
#include "Ros2SensorCoordinator.h"

IMPLEMENT_MODULE(FRos2SceneCameraDdsModule, Ros2SceneCameraDds)

void FRos2SceneCameraDdsModule::StartupModule()
{
	FWorldDelegates::OnPIEStarted.AddLambda([](const bool)
	{
		if (!GEngine)
		{
			return;
		}
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (Ctx.World() && Ctx.WorldType == EWorldType::PIE)
			{
				FRos2SensorCoordinator::OnPieWorldStarted(Ctx.World());
			}
		}
	});
}

void FRos2SceneCameraDdsModule::ShutdownModule()
{
	FLidar360Dds::Shutdown();
}
