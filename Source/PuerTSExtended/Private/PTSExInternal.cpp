// Copyright Template_God. All Rights Reserved.

#include "PTSExInternal.h"

namespace PTSEx
{
namespace
{
std::function<void(const FString&)> GEvalHook;
std::function<void()> GPreReloadHook;
std::function<void()> GCleanupHook;
std::function<void(const FString&, int32)> GMapChangedHook;
}

std::function<void(const FString&)>& EvalHook() { return GEvalHook; }
std::function<void()>& PreReloadHook() { return GPreReloadHook; }
std::function<void()>& CleanupHook() { return GCleanupHook; }
std::function<void(const FString&, int32)>& MapChangedHook() { return GMapChangedHook; }

void ClearAllHooks()
{
	GEvalHook = nullptr;
	GPreReloadHook = nullptr;
	GCleanupHook = nullptr;
	GMapChangedHook = nullptr;
}
}
