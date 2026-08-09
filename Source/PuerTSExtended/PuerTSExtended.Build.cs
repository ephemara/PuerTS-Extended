// Copyright Template_God. All Rights Reserved.

using UnrealBuildTool;

public class PuerTSExtended : ModuleRules
{
	public PuerTSExtended(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableUndefinedIdentifierWarnings = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore",
			"ContentBrowserData",
			"DeveloperSettings",
			"InputCore",
			"InputCore",                // EKeys statics used by Slate widgets (SListView/SComboBox)
			"JsEnv",                    // PuerTS
			"LevelEditor",
			"Projects",                 // IPluginManager (plugin:// script scheme)
			"PropertyEditor",           // IDetailCustomization
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UMG",                      // UUserWidget hosting in editor windows/tabs/details
			"UnrealEd",
			"WorkspaceMenuStructure",   // nomad tab placement
		});
	}
}
