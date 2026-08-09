// Copyright Template_God. All Rights Reserved.

#include "PTSExModuleLoader.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace PTSEx
{
FPTSExModuleLoader::FPTSExModuleLoader(const FString& InScriptRoot, const TArray<FString>& InExtraRoots)
	: puerts::DefaultJSModuleLoader(InScriptRoot)
{
	// Project roots first (project wins over bundled defaults).
	RootDirs.Add(FPaths::ProjectContentDir() / InScriptRoot);
	for (const FString& Extra : InExtraRoots)
	{
		if (!Extra.IsEmpty())
		{
			RootDirs.Add(FPaths::ProjectContentDir() / Extra);
		}
	}

	// Bundled defaults shipped with this plugin.
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PuerTSExtended"));
	if (Plugin.IsValid())
	{
		BundledDir = Plugin->GetContentDir() / InScriptRoot;
	}

	// puerts runtime infra (modular.js, timer.mjs, ...) lives in the project's JavaScript dir.
	JsFallbackDir = FPaths::ProjectContentDir() / TEXT("JavaScript");
}

bool FPTSExModuleLoader::Search(const FString& RequiredDir, const FString& RequiredModule, FString& Path, FString& AbsolutePath)
{
	// 1. plugin:// scheme — reusable tools shipped inside their own plugin.
	if (TryPluginScheme(RequiredModule, Path, AbsolutePath))
	{
		return true;
	}

	// 2. Relative to the requiring module (mirrors default behavior).
	if (SearchModuleInDir(RequiredDir, RequiredModule, Path, AbsolutePath))
	{
		return true;
	}

	// 3. Configured project script roots.
	for (const FString& Root : RootDirs)
	{
		if (SearchModuleInDir(Root, RequiredModule, Path, AbsolutePath))
		{
			return true;
		}
	}

	// 4. Bundled defaults (overridable by a project file of the same name).
	if (SearchModuleInDir(BundledDir, RequiredModule, Path, AbsolutePath))
	{
		return true;
	}

	// 5. Project JavaScript fallback (puerts infra + shared project modules).
	if (SearchModuleInDir(JsFallbackDir, RequiredModule, Path, AbsolutePath))
	{
		return true;
	}

	// 6. Default behavior (upward search, node_modules, ...).
	return puerts::DefaultJSModuleLoader::Search(RequiredDir, RequiredModule, Path, AbsolutePath);
}

bool FPTSExModuleLoader::TryPluginScheme(const FString& InRequiredModule, FString& Path, FString& AbsolutePath)
{
	const FString Prefix = TEXT("plugin://");
	if (!InRequiredModule.StartsWith(Prefix))
	{
		return false;
	}

	FString Rest = InRequiredModule.RightChop(Prefix.Len());
	FString PluginName;
	FString SubPath;
	if (!Rest.Split(TEXT("/"), &PluginName, &SubPath))
	{
		PluginName = Rest;
		SubPath.Empty();
	}

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
	if (!Plugin.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PTSEx: plugin:// require of unknown plugin '%s'"), *PluginName);
		return false;
	}

	const FString ContentDir = Plugin->GetContentDir();
	const FString BundledSubDir = ContentDir / ScriptRoot;
	const TArray<FString> Bases = { BundledSubDir, ContentDir };

	for (const FString& Base : Bases)
	{
		if (SubPath.IsEmpty())
		{
			// No sub path -> the plugin's entry script (Main).
			if (SearchModuleInDir(Base, TEXT("Main"), Path, AbsolutePath))
			{
				return true;
			}
		}
		else if (SearchModuleInDir(Base, SubPath, Path, AbsolutePath))
		{
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("PTSEx: plugin:// require '%s' not found in plugin '%s'"), *SubPath, *PluginName);
	return false;
}
}
