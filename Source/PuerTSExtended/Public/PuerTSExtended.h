// Copyright Template_God. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include <functional>

namespace puerts
{
	class FJsEnv;
	class FSourceFileWatcher;
	namespace v8_impl { class Function; }
	using Function = v8_impl::Function;
}

/**
 * PuerTSExtended — scriptable editor tooling on PuerTS.
 *
 * Owns a dedicated PuerTS JsEnv (isolated from the game env) that runs
 * TypeScript/JavaScript editor tools with hot reload. The TS-facing API
 * surface lives in cpp.PTS_* namespaces (see PTSExBindings.cpp) and covers:
 *   - PTS_Core      env lifecycle, eval, notifications, message boxes, map hooks
 *   - PTS_Menus     menu / toolbar / combo / context entries (TS callbacks)
 *   - PTS_Console   console command registration
 *   - PTS_Windows   editor sub-windows hosting TS-built UMG widgets
 *   - PTS_Tabs      nomad workspace tabs hosting TS-built UMG widgets
 *   - PTS_Details   detail panel customizations rendered from TS-built UMG
 *
 * This C++ API exists so OTHER native plugins can hook the env lifecycle
 * (e.g. clean up their own state before a script reload) without reaching
 * into the private env.
 */
class PUERTSEXTENDED_API FPuerTSExtendedModule : public IModuleInterface
{
public:
	FPuerTSExtendedModule() = default;
	FPuerTSExtendedModule(const FPuerTSExtendedModule&) = delete;
	FPuerTSExtendedModule& operator=(const FPuerTSExtendedModule&) = delete;

	static FPuerTSExtendedModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FPuerTSExtendedModule>(TEXT("PuerTSExtended"));
	}

	//~ IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	/** Full env restart: fires the TS pre-reload hook, tears down, re-inits, starts Main again. */
	void RestartEnv();

	/** Route a string into the env's eval hook (PTS_Core.SetEval). Safe no-op if unset. */
	void EvalScript(const FString& InCode);

	/** True once the JsEnv exists and has been started. */
	bool IsEnvRunning() const;

	/**
	 * Register a console command whose handler is a TS function.
	 * Returns a handle usable with UnregisterTsConsoleCommand.
	 */
	int32 RegisterTsConsoleCommand(const FString& InName, const FString& InHelp,
		puerts::Function InCommand);

	/** Remove a previously registered TS console command. */
	void UnregisterTsConsoleCommand(int32 InHandle);

private:
	void OnPostEngineInit();
	void InitJsEnv();
	void UnInitJsEnv();
	bool Tick(float InDelta);
	void HandleMapChanged(UWorld* InWorld, EMapChangeType InMapChangeType);

	TSharedPtr<puerts::FJsEnv> JsEnv;
	TSharedPtr<puerts::FSourceFileWatcher> SourceFileWatcher;

	TArray<TUniquePtr<class FAutoConsoleCommand>> TsConsoleCommands;

	bool bStartupScriptCalled = false;
	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle MapChangedHandle;
};
