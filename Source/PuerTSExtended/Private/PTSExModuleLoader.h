// Copyright Template_God. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "JSModuleLoader.h"

namespace PTSEx
{
/**
 * Module loader for the PuerTSExtended editor env.
 *
 * Search order for `require(...)`:
 *   1. `plugin://PluginName/Sub/Path` — resolved against <Plugin>/Content/
 *      (falls back from <Plugin>/Content/PuertsExtended/Sub/Path to
 *      <Plugin>/Content/Sub/Path; empty path means Main). This is the
 *      swappability primitive: reusable tool scripts ship inside their own
 *      plugin and are pulled in by name.
 *   2. Relative to the requiring module's own directory (default behavior).
 *   3. Configured script roots under the project Content dir.
 *   4. This plugin's bundled <PuerTSExtended>/Content/<ScriptRoot> — default
 *      scripts that projects can shadow by defining the same path in (3).
 *   5. <Project>/Content/JavaScript — puerts runtime infra (modular.js etc.)
 *      plus any shared modules the project already maintains.
 *   6. Default upward search (node_modules etc.).
 */
class FPTSExModuleLoader : public puerts::DefaultJSModuleLoader
{
public:
	FPTSExModuleLoader(const FString& InScriptRoot, const TArray<FString>& InExtraRoots);

	virtual bool Search(const FString& RequiredDir, const FString& RequiredModule, FString& Path, FString& AbsolutePath) override;

private:
	bool TryPluginScheme(const FString& InRequiredModule, FString& Path, FString& AbsolutePath);

	/** Project content subdirs (ScriptRoot + extras). */
	TArray<FString> RootDirs;

	/** This plugin's bundled scripts dir. */
	FString BundledDir;

	/** Project Content/JavaScript fallback. */
	FString JsFallbackDir;
};
}
