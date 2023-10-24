// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WnpNodes : ModuleRules
{
	public WnpNodes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[]
			{
				// ... add public include paths required here ...
			}
		);


		PrivateIncludePaths.AddRange(
			new string[]
			{
				"WnpNodes/Private",
				// ... add other private include paths required here ...
			}
		);


		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"AnimationCore",
				"AnimGraphRuntime",
				"ControlRig",
				"RigVM",
				// ... add other public dependencies that you statically link with here ...
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"AnimGraphRuntime",
				"ControlRig"
				// ... add private dependencies that you statically link with here ...	
			}
		);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
		);

		if (Target.bBuildEditor == true)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Slate",
					"SlateCore",
					"RigVMDeveloper",
					"AnimGraph",
					"Json",
					"Serialization",
					"JsonUtilities",
					"AnimationBlueprintLibrary",
					"ControlRigEditor",
					"ControlRigDeveloper"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorFramework",
					"UnrealEd",
					"BlueprintGraph",
					"PropertyEditor",
					"RigVMDeveloper",
					"ControlRigEditor",
					"ControlRigDeveloper"
				}
			);
		}
	}
}