// Copyright Template_God. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Binding.hpp"       // MUST be included before Object.hpp to resolve circular dependencies
#include "Object.hpp"        // puerts::Function
#include "UEDataBinding.hpp" // puerts::Converter<UStruct>
#include "V8Utils.h"         // FV8Utils helpers

class UUserWidget;

namespace PTSEx
{
class FPTSExSlateWidget;

/**
 * RAII scope that enters the isolate + context of a stored TS function so it
 * can be invoked safely from any game-thread call site (menu callbacks,
 * console commands, tab spawners, ...). Mirrors what the engine's own
 * delegate-to-JS bridges do.
 */
class FTsCallScope
{
public:
	explicit FTsCallScope(const puerts::Function& InFunc);
	~FTsCallScope() = default;

	v8::Isolate* GetIsolate() const { return IsolatePtr; }
	v8::Local<v8::Context> GetContext() const { return Ctx.Get(IsolatePtr); }
	v8::Local<v8::Function> GetFunction() const { return Func.GObject.Get(IsolatePtr).As<v8::Function>(); }

private:
	puerts::Function Func;
	v8::Isolate* IsolatePtr;
	v8::Global<v8::Context> Ctx;
	v8::Isolate::Scope IsolateScope;
	v8::HandleScope HandleScope;
	v8::Context::Scope ContextScope;
};

/** Invoke with no args. Returns false if the call threw (already logged). */
bool Invoke(const puerts::Function& InFunc);

/** Invoke with a single UObject argument. */
bool InvokeWithUObject(const puerts::Function& InFunc, UObject* InObject);

/** Invoke with a JS array of UObjects. */
bool InvokeWithUObjects(const puerts::Function& InFunc, const TArray<UObject*>& InObjects);

/** Invoke with an FToolMenuContext (menu/context-menu callbacks). */
bool InvokeWithMenuContext(const puerts::Function& InFunc, const FToolMenuContext& InContext);

/** Invoke with an int32 value. */
bool InvokeWithInt(const puerts::Function& InFunc, int32 InVal);

/** Invoke with a string value. */
bool InvokeWithString(const puerts::Function& InFunc, const FString& InStr);

/** Invoke a tab spawn callback that returns a UUserWidget. Null on error/empty. */
UUserWidget* InvokeSpawnWidget(const puerts::Function& InFunc);

/** Invoke a tab spawn callback that returns a Slate widget. */
FPTSExSlateWidget InvokeSpawnSlateWidget(const puerts::Function& InFunc);

/** Log a caught v8 exception under the Puerts category. */
void LogCaughtException(v8::Isolate* InIsolate, v8::TryCatch* InTryCatch);
}
