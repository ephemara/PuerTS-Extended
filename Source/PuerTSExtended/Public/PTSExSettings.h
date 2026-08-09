// Copyright Template_God. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PTSExSettings.generated.h"

/**
 * PuerTSExtended project settings.
 *
 * Config file: Config/DefaultEditor.ini, section [PuerTSExtended].
 * Editable from Project Settings > Plugins > PuerTSExtended.
 *
 * Script layout (search order, project wins):
 *   1. <Project>/Content/<ScriptRoot>/            (project tools)
 *   2. <PuerTSExtended>/Content/<ScriptRoot>/     (bundled defaults, overridable)
 *   3. <Project>/Content/JavaScript/              (puerts runtime infra fallback)
 * Plus `plugin://PluginName/Sub/Path` requires resolved against any plugin's
 * Content dir — that is how reusable per-tool plugins stay swappable.
 */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "PuerTSExtended"))
class UPTSExSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Root folder for editor tool scripts, relative to the project Content dir. */
	UPROPERTY(EditAnywhere, config, Category = "Runtime", meta = (DisplayName = "Script Root"))
	FString ScriptRoot = TEXT("PuertsExtended");

	/** Watch loaded script files and hot-reload them on change. */
	UPROPERTY(EditAnywhere, config, Category = "Runtime")
	bool bHotReload = true;

	/** V8 inspector debug port for this env (-1 = disabled). 8080 is taken by the game env. */
	UPROPERTY(EditAnywhere, config, Category = "Runtime", meta = (DisplayName = "Debug Port (-1 = disabled)"))
	int32 DebugPort = -1;

	/** Additional script roots, each relative to the project Content dir. */
	UPROPERTY(EditAnywhere, config, Category = "Runtime", meta = (DisplayName = "Extra Script Roots"))
	TArray<FString> ExtraScriptRoots;

	virtual FName GetCategoryName() const override { return TEXT("PuerTSExtended"); }
	virtual FName GetSectionName() const override { return TEXT("PuerTSExtended"); }
};
