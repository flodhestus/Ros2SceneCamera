using UnrealBuildTool;
using System.IO;

public class Ros2SceneCamera : ModuleRules
{
	public Ros2SceneCamera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "RenderCore", "RHI", "Renderer", "Ros2SceneCameraDds"
		});

		PrivateDependencyModuleNames.Add("Projects");
	}
}
