// Copyright Template_God. All Rights Reserved.

#include "PTSExInvoke.h"

#include "PTSExSlate.h" // FPTSExSlateWidget complete type

#include "Blueprint/UserWidget.h" // Complete type for Converter<UUserWidget*>
#include "ToolMenuContext.h"

namespace PTSEx
{
FTsCallScope::FTsCallScope(const puerts::Function& InFunc)
	: Func(InFunc)
	, IsolatePtr(InFunc.Isolate)
	, IsolateScope(InFunc.Isolate)
	, HandleScope(InFunc.Isolate)
	, ContextScope(InFunc.GContext.Get(InFunc.Isolate))
{
	Ctx.Reset(IsolatePtr, InFunc.GContext.Get(IsolatePtr));
}

void LogCaughtException(v8::Isolate* InIsolate, v8::TryCatch* InTryCatch)
{
	UE_LOG(Puerts, Error, TEXT("PTSEx call function throw: %s"),
		*puerts::FV8Utils::TryCatchToString(InIsolate, InTryCatch));
}

bool Invoke(const puerts::Function& InFunc)
{
	if (!InFunc.Isolate || InFunc.GObject.IsEmpty())
	{
		return false;
	}

	FTsCallScope Scope(InFunc);
	v8::Isolate* Isolate = Scope.GetIsolate();
	v8::TryCatch TryCatch(Isolate);

	v8::Local<v8::Value> Result;
	if (!Scope.GetFunction()->Call(Scope.GetContext(), v8::Undefined(Isolate), 0, nullptr).ToLocal(&Result))
	{
		LogCaughtException(Isolate, &TryCatch);
		return false;
	}

	return true;
}

bool InvokeWithUObject(const puerts::Function& InFunc, UObject* InObject)
{
	if (!InFunc.Isolate || InFunc.GObject.IsEmpty())
	{
		return false;
	}

	FTsCallScope Scope(InFunc);
	v8::Isolate* Isolate = Scope.GetIsolate();
	v8::Local<v8::Context> Ctx = Scope.GetContext();
	v8::TryCatch TryCatch(Isolate);

	v8::Local<v8::Value> Args[] = {
		puerts::v8_impl::Converter<UObject*>::toScript(Ctx, InObject),
	};

	v8::Local<v8::Value> Result;
	if (!Scope.GetFunction()->Call(Ctx, v8::Undefined(Isolate), 1, Args).ToLocal(&Result))
	{
		LogCaughtException(Isolate, &TryCatch);
		return false;
	}

	return true;
}

bool InvokeWithUObjects(const puerts::Function& InFunc, const TArray<UObject*>& InObjects)
{
	if (!InFunc.Isolate || InFunc.GObject.IsEmpty())
	{
		return false;
	}

	FTsCallScope Scope(InFunc);
	v8::Isolate* Isolate = Scope.GetIsolate();
	v8::Local<v8::Context> Ctx = Scope.GetContext();
	v8::TryCatch TryCatch(Isolate);

	v8::Local<v8::Array> ObjArr = v8::Array::New(Isolate, InObjects.Num());
	for (int32 i = 0; i < InObjects.Num(); ++i)
	{
		ObjArr->Set(Ctx, i, puerts::v8_impl::Converter<UObject*>::toScript(Ctx, InObjects[i]));
	}

	v8::Local<v8::Value> Args[] = { ObjArr };

	v8::Local<v8::Value> Result;
	if (!Scope.GetFunction()->Call(Ctx, v8::Undefined(Isolate), 1, Args).ToLocal(&Result))
	{
		LogCaughtException(Isolate, &TryCatch);
		return false;
	}

	return true;
}

bool InvokeWithMenuContext(const puerts::Function& InFunc, const FToolMenuContext& InContext)
{
	if (!InFunc.Isolate || InFunc.GObject.IsEmpty())
	{
		return false;
	}

	FTsCallScope Scope(InFunc);
	v8::Isolate* Isolate = Scope.GetIsolate();
	v8::Local<v8::Context> Ctx = Scope.GetContext();
	v8::TryCatch TryCatch(Isolate);

	v8::Local<v8::Value> Args[] = {
		puerts::v8_impl::Converter<FToolMenuContext>::toScript(Ctx, InContext),
	};

	v8::Local<v8::Value> Result;
	if (!Scope.GetFunction()->Call(Ctx, v8::Undefined(Isolate), 1, Args).ToLocal(&Result))
	{
		LogCaughtException(Isolate, &TryCatch);
		return false;
	}

	return true;
}

UUserWidget* InvokeSpawnWidget(const puerts::Function& InFunc)
{
	if (!InFunc.Isolate || InFunc.GObject.IsEmpty())
	{
		return nullptr;
	}

	FTsCallScope Scope(InFunc);
	v8::Isolate* Isolate = Scope.GetIsolate();
	v8::Local<v8::Context> Ctx = Scope.GetContext();
	v8::TryCatch TryCatch(Isolate);

	v8::Local<v8::Value> Result;
	if (!Scope.GetFunction()->Call(Ctx, v8::Undefined(Isolate), 0, nullptr).ToLocal(&Result))
	{
		LogCaughtException(Isolate, &TryCatch);
		return nullptr;
	}

	if (Result.IsEmpty() || Result->IsNullOrUndefined())
	{
		return nullptr;
	}

	return puerts::v8_impl::Converter<UUserWidget*>::toCpp(Ctx, Result);
}

bool InvokeWithInt(const puerts::Function& InFunc, int32 InVal)
{
	if (!InFunc.Isolate || InFunc.GObject.IsEmpty())
	{
		return false;
	}

	FTsCallScope Scope(InFunc);
	v8::Isolate* Isolate = Scope.GetIsolate();
	v8::Local<v8::Context> Ctx = Scope.GetContext();
	v8::TryCatch TryCatch(Isolate);

	v8::Local<v8::Value> Args[] = {
		v8::Integer::New(Isolate, InVal),
	};

	v8::Local<v8::Value> Result;
	if (!Scope.GetFunction()->Call(Ctx, v8::Undefined(Isolate), 1, Args).ToLocal(&Result))
	{
		LogCaughtException(Isolate, &TryCatch);
		return false;
	}

	return true;
}

bool InvokeWithFloat(const puerts::Function& InFunc, float InVal)
{
	if (!InFunc.Isolate || InFunc.GObject.IsEmpty())
	{
		return false;
	}

	FTsCallScope Scope(InFunc);
	v8::Isolate* Isolate = Scope.GetIsolate();
	v8::Local<v8::Context> Ctx = Scope.GetContext();
	v8::TryCatch TryCatch(Isolate);

	v8::Local<v8::Value> Args[] = {
		v8::Number::New(Isolate, InVal),
	};

	v8::Local<v8::Value> Result;
	if (!Scope.GetFunction()->Call(Ctx, v8::Undefined(Isolate), 1, Args).ToLocal(&Result))
	{
		LogCaughtException(Isolate, &TryCatch);
		return false;
	}

	return true;
}

bool InvokeWithString(const puerts::Function& InFunc, const FString& InStr)
{
	if (!InFunc.Isolate || InFunc.GObject.IsEmpty())
	{
		return false;
	}

	FTsCallScope Scope(InFunc);
	v8::Isolate* Isolate = Scope.GetIsolate();
	v8::Local<v8::Context> Ctx = Scope.GetContext();
	v8::TryCatch TryCatch(Isolate);

	v8::Local<v8::Value> Args[] = {
		puerts::FV8Utils::ToV8String(Isolate, InStr),
	};

	v8::Local<v8::Value> Result;
	if (!Scope.GetFunction()->Call(Ctx, v8::Undefined(Isolate), 1, Args).ToLocal(&Result))
	{
		LogCaughtException(Isolate, &TryCatch);
		return false;
	}

	return true;
}

FPTSExSlateWidget InvokeSpawnSlateWidget(const puerts::Function& InFunc)
{
	if (!InFunc.Isolate || InFunc.GObject.IsEmpty())
	{
		return FPTSExSlateWidget();
	}

	FTsCallScope Scope(InFunc);
	v8::Isolate* Isolate = Scope.GetIsolate();
	v8::Local<v8::Context> Ctx = Scope.GetContext();
	v8::TryCatch TryCatch(Isolate);

	v8::Local<v8::Value> Result;
	if (!Scope.GetFunction()->Call(Ctx, v8::Undefined(Isolate), 0, nullptr).ToLocal(&Result))
	{
		LogCaughtException(Isolate, &TryCatch);
		return FPTSExSlateWidget();
	}

	if (Result.IsEmpty() || Result->IsNullOrUndefined())
	{
		return FPTSExSlateWidget();
	}

	return puerts::v8_impl::Converter<FPTSExSlateWidget>::toCpp(Ctx, Result);
}
}
