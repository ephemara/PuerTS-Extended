// Copyright Template_God. All Rights Reserved.

#include "PuerTSExtended.h"

#include "PTSExInternal.h"
#include "PTSExModuleLoader.h"
#include "PTSExSlate.h"
#include "PTSExSettings.h"
#include "PTSExInvoke.h"

#include "JsEnv.h"
#include "JSLogger.h"
#include "SourceFileWatcher.h"
#include "V8Utils.h"

#include "Containers/Ticker.h"
#include "LevelEditor.h"
#include "Misc/FileHelper.h"
#include "ToolMenus.h"

#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

#include "v8.h"

#define LOCTEXT_NAMESPACE "FPuerTSExtendedModule"

void FPuerTSExtendedModule::StartupModule()
{
	char GCFlags[] = "--expose-gc";
	v8::V8::SetFlagsFromString(GCFlags, sizeof(GCFlags));

	const UPTSExSettings* Settings = GetDefault<UPTSExSettings>();

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FPuerTSExtendedModule::OnPostEngineInit);

	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		MapChangedHandle = LevelEditor->OnMapChanged().AddRaw(this, &FPuerTSExtendedModule::HandleMapChanged);
	}

	if (Settings->DebugPort >= 0)
	{
		UE_LOG(LogTemp, Log, TEXT("PTSEx: debug port %d (attach inspector before it loads Main)"), Settings->DebugPort);
	}
}

void FPuerTSExtendedModule::ShutdownModule()
{
	if (MapChangedHandle.IsValid())
	{
		if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
		{
			LevelEditor->OnMapChanged().Remove(MapChangedHandle);
		}
		MapChangedHandle.Reset();
	}

	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	UnInitJsEnv();
}

void FPuerTSExtendedModule::OnPostEngineInit()
{
	const UPTSExSettings* Settings = GetDefault<UPTSExSettings>();

	InitJsEnv();
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FPuerTSExtendedModule::Tick));

	// "TypeScript" string commands (e.g. menu entries with Command = "TypeScript <code>")
	// route into the env's eval hook.
	UToolMenus::Get()->RegisterStringCommandHandler(TEXT("TypeScript"),
		FToolMenuExecuteString::CreateLambda([this](const FString& InString, const FToolMenuContext&)
		{
			EvalScript(InString);
		}));

	if (Settings->bHotReload)
	{
		UE_LOG(LogTemp, Log, TEXT("PTSEx: hot reload enabled (edit scripts, they reload live)"));
	}

	// Runtime knobs (also usable from the editor console):
	//   PuerTSEx.Reload          full env restart
	//   PuerTSEx.Eval <code>     eval a string in the env
	//   PuerTSEx.Status          print env diagnostics
	//   PuerTSEx.DebugPort <n>   set V8 inspector port, then reload
	//   PuerTSEx.ScriptRoot <d>  add an extra script root, then reload
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("PuerTSEx.Reload"), TEXT("Restart the PuerTSExtended script env"),
		FConsoleCommandDelegate::CreateRaw(this, &FPuerTSExtendedModule::RestartEnv));

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("PuerTSEx.Eval"), TEXT("PuerTSEx.Eval <code> — evaluate a string in the PuerTSExtended env"),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& InArgs)
		{
			FString Code = FString::Join(InArgs, TEXT(" "));
			if (Code.IsEmpty())
			{
				UE_LOG(LogTemp, Warning, TEXT("PTSEx: PuerTSEx.Eval requires code"));
				return;
			}
			EvalScript(Code);
		}));

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("PuerTSEx.Status"), TEXT("Print PuerTSExtended env status"),
		FConsoleCommandDelegate::CreateLambda([this]()
		{
			const UPTSExSettings* S = GetDefault<UPTSExSettings>();
			UE_LOG(LogTemp, Log, TEXT("PTSEx: env=%s scriptRoot=%s hotReload=%d debugPort=%d extraRoots=%d"),
				JsEnv.IsValid() ? TEXT("alive") : TEXT("dead"), *S->ScriptRoot, S->bHotReload ? 1 : 0,
				S->DebugPort, S->ExtraScriptRoots.Num());
		}));

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("PuerTSEx.DebugPort"), TEXT("PuerTSEx.DebugPort <n> — set inspector port and reload"),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& InArgs)
		{
			if (InArgs.Num() < 1)
			{
				return;
			}
			UPTSExSettings* S = GetMutableDefault<UPTSExSettings>();
			S->DebugPort = FCString::Atoi(*InArgs[0]);
			S->SaveConfig();
			RestartEnv();
		}));

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("PuerTSEx.ScriptRoot"), TEXT("PuerTSEx.ScriptRoot <dir> — add an extra script root and reload"),
		FConsoleCommandWithArgsDelegate::CreateLambda([this](const TArray<FString>& InArgs)
		{
			if (InArgs.Num() < 1)
			{
				return;
			}
			UPTSExSettings* S = GetMutableDefault<UPTSExSettings>();
			S->ExtraScriptRoots.AddUnique(InArgs[0]);
			S->SaveConfig();
			RestartEnv();
		}));
}

void FPuerTSExtendedModule::InitJsEnv()
{
	const UPTSExSettings* Settings = GetDefault<UPTSExSettings>();

	if (Settings->bHotReload)
	{
		SourceFileWatcher = MakeShared<puerts::FSourceFileWatcher>(
			[this](const FString& InPath)
			{
				if (JsEnv.IsValid())
				{
					TArray<uint8> Source;
					if (FFileHelper::LoadFileToArray(Source, *InPath))
					{
						JsEnv->ReloadSource(InPath, puerts::PString((const char*)Source.GetData(), Source.Num()));
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("PTSEx: read file fail for %s"), *InPath);
					}
				}
			});
	}

	auto Loader = std::make_shared<PTSEx::FPTSExModuleLoader>(Settings->ScriptRoot, Settings->ExtraScriptRoots);

	JsEnv = MakeShared<puerts::FJsEnv>(Loader, std::make_shared<puerts::FDefaultLogger>(), Settings->DebugPort,
		[this](const FString& InPath)
		{
			if (SourceFileWatcher.IsValid())
			{
				SourceFileWatcher->OnSourceLoaded(InPath);
			}
		},
		TEXT("--expose-gc"));
}

void FPuerTSExtendedModule::UnInitJsEnv()
{
	PTSEx::ClearAllState();

	if (PTSEx::CleanupHook())
	{
		PTSEx::CleanupHook()();
	}

	PTSEx::ClearAllHooks();
	TsConsoleCommands.Reset();

	if (JsEnv.IsValid())
	{
		JsEnv.Reset();
	}
	if (SourceFileWatcher.IsValid())
	{
		SourceFileWatcher.Reset();
	}
}

bool FPuerTSExtendedModule::Tick(float)
{
	if (!bStartupScriptCalled)
	{
		bStartupScriptCalled = true;
		if (JsEnv.IsValid())
		{
			JsEnv->Start(TEXT("Main"));
		}
	}
	else if (JsEnv.IsValid())
	{
		// Keep V8 GC happy; timers are driven natively by puerts.
		JsEnv->IdleNotificationDeadline(0.0);
	}
	return true;
}

void FPuerTSExtendedModule::HandleMapChanged(UWorld* InWorld, EMapChangeType InMapChangeType)
{
	if (InMapChangeType == EMapChangeType::TearDownWorld && JsEnv.IsValid())
	{
		JsEnv->RequestFullGarbageCollectionForTesting();
	}

	if (PTSEx::MapChangedHook() && InWorld)
	{
		PTSEx::MapChangedHook()(InWorld->GetMapName(), (int32)InMapChangeType);
	}
}

void FPuerTSExtendedModule::RestartEnv()
{
	if (PTSEx::PreReloadHook())
	{
		PTSEx::PreReloadHook()();
	}

	UnInitJsEnv();
	InitJsEnv();
	bStartupScriptCalled = false; // next tick starts Main again

	UE_LOG(LogTemp, Log, TEXT("PTSEx: env restarted"));
}

void FPuerTSExtendedModule::EvalScript(const FString& InCode)
{
	if (PTSEx::EvalHook())
	{
		PTSEx::EvalHook()(InCode);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PTSEx: eval requested but no eval hook set (call cpp.PTS_Core.SetEval from Main)"));
	}
}

bool FPuerTSExtendedModule::IsEnvRunning() const
{
	return JsEnv.IsValid() && bStartupScriptCalled;
}

int32 FPuerTSExtendedModule::RegisterTsConsoleCommand(const FString& InName, const FString& InHelp,
	puerts::Function InCommand)
{
	if (InName.IsEmpty() || InCommand.GObject.IsEmpty())
	{
		return INDEX_NONE;
	}

	TsConsoleCommands.Add(MakeUnique<FAutoConsoleCommand>(*InName, *InHelp,
		FConsoleCommandWithArgsDelegate::CreateLambda(
			[InCommand](const TArray<FString>& InArgs)
			{
				if (!InCommand.Isolate || InCommand.GObject.IsEmpty())
				{
					return;
				}

				PTSEx::FTsCallScope Scope(InCommand);
				v8::Isolate* Isolate = Scope.GetIsolate();
				v8::Local<v8::Context> Ctx = Scope.GetContext();
				v8::TryCatch TryCatch(Isolate);

				v8::Local<v8::Value>* JsArgs =
					static_cast<v8::Local<v8::Value>*>(FMemory_Alloca(sizeof(v8::Local<v8::Value>) * InArgs.Num()));

				for (int i = 0; i < InArgs.Num(); i++)
				{
					JsArgs[i] = puerts::FV8Utils::ToV8String(Isolate, InArgs[i]);
				}

				v8::Local<v8::Value> Result;
				if (!Scope.GetFunction()->Call(Ctx, v8::Undefined(Isolate), InArgs.Num(), JsArgs).ToLocal(&Result))
				{
					PTSEx::LogCaughtException(Isolate, &TryCatch);
				}
			})));
	return TsConsoleCommands.Num() - 1;
}

void FPuerTSExtendedModule::UnregisterTsConsoleCommand(int32 InHandle)
{
	if (TsConsoleCommands.IsValidIndex(InHandle))
	{
		TsConsoleCommands.RemoveAt(InHandle);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPuerTSExtendedModule, PuerTSExtended);
