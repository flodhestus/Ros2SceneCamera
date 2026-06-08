using UnrealBuildTool;
using System.IO;

public class unreal_gpu_camera : ModuleRules
{
	public unreal_gpu_camera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "RenderCore", "RHI", "Renderer", "Ros2DdsSharedCamera"
		});

		PrivateDependencyModuleNames.AddRange(new[] { "Projects", "RHICore" });
	}
}
