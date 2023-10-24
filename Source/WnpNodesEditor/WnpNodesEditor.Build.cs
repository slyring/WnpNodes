using UnrealBuildTool;

public class WnpNodesEditor : ModuleRules
{
	public WnpNodesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[]
			{
				System.IO.Path.Combine(GetModuleDirectory("WnpNodes"), "Private"),
				System.IO.Path.Combine(GetModuleDirectory("ControlRig"), "Private"),

				// ... add other private include paths required here ...
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"MainFrame",
				"AppFramework",

				// ... add other public dependencies that you statically link with here ...
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"WnpNodes",

				"Core",
				"CoreUObject",
				"CurveEditor",
				"Slate",
				"SlateCore",
				"InputCore",
				"Engine",
				"EditorFramework",
				"UnrealEd",
				"KismetCompiler",
				"BlueprintGraph",
				"ControlRig",
				"ControlRigDeveloper",
				"ControlRigEditor",
				"Kismet",
				"KismetCompiler",
				"EditorStyle",
				"EditorWidgets",
				"ApplicationCore",
				"AnimationCore",
				"PropertyEditor",
				"AnimGraph",
				"AnimGraphRuntime",
				"ClassViewer",
				"AssetTools",
				"EditorInteractiveToolsFramework",
				"GraphEditor",
				"PropertyPath",
				"UMG",
				"TimeManagement",
				"PropertyPath",
				"WorkspaceMenuStructure",
				"Json",
				"DesktopPlatform",
				"ToolMenus",
				"RigVM",
				"RigVMDeveloper",
				"AnimationEditor",
				"PropertyAccessEditor",
				"KismetWidgets",
				"AdvancedPreviewScene",
				"ToolWidgets",
				"AnimationWidgets",
				"Constraints",
				"AnimationEditMode"

				// ... add private dependencies that you statically link with here ...	
			}
		);
	}
}