using UnrealBuildTool;
using System.IO;

public class Ros2SceneCamera : ModuleRules
{
	public Ros2SceneCamera(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		string PluginDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
		string CycloneRoot = Path.Combine(PluginDir, "ThirdParty", "cyclonedds");
		string Ros2Gen = Path.Combine(PluginDir, "ThirdParty", "ros2idl", "generated");

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine", "RenderCore", "RHI", "Renderer"
		});

		PrivateDependencyModuleNames.Add("Projects");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.AddRange(new[] { "opengl32.lib", "gdi32.lib", "user32.lib" });
		}

		if (Directory.Exists(Path.Combine(CycloneRoot, "include", "dds")))
		{
			PublicSystemIncludePaths.Add(Path.Combine(CycloneRoot, "include"));
			string DdsLib = Path.Combine(CycloneRoot, "lib", "Win64", "ddsc.lib");
			if (File.Exists(DdsLib))
			{
				PublicAdditionalLibraries.Add(DdsLib);
				PublicDelayLoadDLLs.Add("ddsc.dll");
				RuntimeDependencies.Add("$(BinaryOutputDir)/ddsc.dll", Path.Combine(CycloneRoot, "bin", "Win64", "ddsc.dll"));
			}
			PublicDefinitions.Add("WITH_ROS2_SCENE_CAMERA_DDS=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ROS2_SCENE_CAMERA_DDS=0");
		}

		if (Directory.Exists(Ros2Gen))
		{
			PublicSystemIncludePaths.Add(Ros2Gen);
		}
	}
}
