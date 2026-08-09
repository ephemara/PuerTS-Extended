// Copyright Template_God. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include <functional>

/**
 * Internal bridge between the TS-facing bindings (PTSExBindings.cpp) and the
 * module lifecycle (PuerTSExtended.cpp). Hooks are registered from TS via
 * cpp.PTS_Core.* and consumed by the module.
 */
namespace PTSEx
{
/** cpp.PTS_Core.SetEval — evaluates a string inside the env (implemented TS-side). */
std::function<void(const FString&)>& EvalHook();

/** cpp.PTS_Core.SetOnJsEnvPreReload — called before env teardown/restart. */
std::function<void()>& PreReloadHook();

/** cpp.PTS_Core.SetOnJsEnvCleanup — called after env teardown. */
std::function<void()>& CleanupHook();

/** cpp.PTS_Core.OnMapChanged — (world name, EMapChangeType as int). */
std::function<void(const FString&, int32)>& MapChangedHook();

/** Drop every hook (env teardown). */
void ClearAllHooks();
}
