// Copyright Template_God. All Rights Reserved.

#include "Binding.hpp" // MUST be included first to resolve PuerTS circular dependencies
#include "UEDataBinding.hpp"
#include "Object.hpp"

#include "PTSExInternal.h"
#include "PTSExInvoke.h"
#include "PTSExSlate.h"
#include "PuerTSExtended.h"

#include "ContentBrowserItem.h"
#include "ToolMenuDelegates.h"
#include "ToolMenus.h"
#include "UObject/Class.h"          // Complete type for UClass*
#include "Blueprint/UserWidget.h"   // Complete type for UUserWidget*

#include "Framework/Commands/UIAction.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"

// ---------------------------------------------------------------------------
// TAttribute adapter for PuerTS (mirrors EasyEditor for Slate compatibility)
// ---------------------------------------------------------------------------
namespace puerts
{
template <typename T>
struct ScriptTypeName<TAttribute<T>>
{
	static constexpr auto value()
	{
		return ScriptTypeName<T>::value();
	}
};

namespace v8_impl
{
template <typename T>
struct Converter<TAttribute<T>>
{
	static v8::Local<v8::Value> toScript(v8::Local<v8::Context> context, TAttribute<T> value)
	{
		if (value.IsSet())
		{
			return Converter<T>::toScript(context, value.Get());
		}
		return v8::Undefined(context->GetIsolate());
	}

	static TAttribute<T> toCpp(v8::Local<v8::Context> context, const v8::Local<v8::Value>& value)
	{
		if (value.IsEmpty() || value->IsNullOrUndefined())
			return TAttribute<T>();
		return TAttribute<T>(Converter<T>::toCpp(context, value));
	}

	static bool accept(v8::Local<v8::Context> context, const v8::Local<v8::Value>& value)
	{
		return value.IsEmpty() || value->IsNullOrUndefined() || Converter<T>::accept(context, value);
	}
};
} // namespace v8_impl
} // namespace puerts

// ---------------------------------------------------------------------------
// USTRUCT / C++ types re-exposed to the env.
// ---------------------------------------------------------------------------
UsingUStruct(FToolMenuEntry);
UsingUStruct(FToolMenuContext);
UsingUStruct(FContentBrowserItem);
UsingCppType(FSlateIcon);

UsingUClass(UToolMenu);
UsingUClass(UUserWidget);
UsingUClass(UClass);
UsingUClass(UObject);

// ---------------------------------------------------------------------------
// cpp.PTS_Core — env lifecycle + misc
// ---------------------------------------------------------------------------
struct PTS_Core
{
	static void SetOnJsEnvPreReload(std::function<void()> InFunc)
	{
		PTSEx::PreReloadHook() = MoveTemp(InFunc);
	}

	static void SetOnJsEnvCleanup(std::function<void()> InFunc)
	{
		PTSEx::CleanupHook() = MoveTemp(InFunc);
	}

	static void SetEval(std::function<void(const FString&)> InFunc)
	{
		PTSEx::EvalHook() = MoveTemp(InFunc);
	}

	static void Reload()
	{
		FPuerTSExtendedModule::Get().RestartEnv();
	}

	static void Eval(const FString& InCode)
	{
		FPuerTSExtendedModule::Get().EvalScript(InCode);
	}

	static FString GetProjectDir()
	{
		return FPaths::ProjectDir();
	}

	static FString GetContentDir()
	{
		return FPaths::ProjectContentDir();
	}

	static FString GetEngineVersion()
	{
		return FEngineVersion::Current().ToString(EVersionComponent::Minor);
	}

	static void Notify(const FString& InTitle, const FString& InText, int32 InType)
	{
		PTSEx::Notify(InTitle, InText, InType);
	}

	static int32 MessageBox(const FString& InTitle, const FString& InText, int32 InButtons)
	{
		return PTSEx::MessageBox(InTitle, InText, InButtons);
	}

	static void OnMapChanged(std::function<void(const FString&, int32)> InFunc)
	{
		PTSEx::MapChangedHook() = MoveTemp(InFunc);
	}
};

// ---------------------------------------------------------------------------
// cpp.PTS_Menus — menu / toolbar / combo / context entries with TS callbacks
// ---------------------------------------------------------------------------
struct PTS_Menus
{
	static FToolMenuEntry InitMenuEntry(const FName InName, const FText& InLabel, const FText& InToolTip,
		std::function<void(const FToolMenuContext&)> InExecuteAction)
	{
		return FToolMenuEntry::InitMenuEntry(InName, InLabel, InToolTip, TAttribute<FSlateIcon>(),
			FToolUIActionChoice(FToolMenuExecuteAction::CreateLambda(
				[InExecuteAction](const FToolMenuContext& InContext)
				{
					if (InExecuteAction)
					{
						InExecuteAction(InContext);
					}
				})));
	}

	static FToolMenuEntry InitToolBarButton(const FName InName, const FText& InLabel,
		std::function<void(const FToolMenuContext&)> InExecuteAction)
	{
		return FToolMenuEntry::InitToolBarButton(InName,
			FToolUIActionChoice(FToolMenuExecuteAction::CreateLambda(
				[InExecuteAction](const FToolMenuContext& InContext)
				{
					if (InExecuteAction)
					{
						InExecuteAction(InContext);
					}
				})),
			InLabel);
	}

	static FToolMenuEntry InitComboButton(const FName InName,
		std::function<void(const FToolMenuContext&)> InExecuteAction,
		std::function<void(UToolMenu*)> InMenuContentGenerator,
		const TAttribute<FText>& InLabel = TAttribute<FText>(),
		const TAttribute<FText>& InToolTip = TAttribute<FText>(),
		const FSlateIcon& InIcon = FSlateIcon())
	{
		return FToolMenuEntry::InitComboButton(InName,
			FToolUIActionChoice(FToolMenuExecuteAction::CreateLambda(
				[InExecuteAction](const FToolMenuContext& InContext)
				{
					if (InExecuteAction)
					{
						InExecuteAction(InContext);
					}
				})),
			FNewToolMenuDelegate::CreateLambda(
				[InMenuContentGenerator](UToolMenu* InSubMenu)
				{
					if (InSubMenu && InMenuContentGenerator)
					{
						InMenuContentGenerator(InSubMenu);
					}
				}),
			InLabel, InToolTip, InIcon);
	}

	static void AddEntry(UToolMenu* InMenu, const FName InSection, const FToolMenuEntry& InEntry)
	{
		if (InMenu)
		{
			InMenu->AddMenuEntry(InSection, InEntry);
		}
	}

	static void AddFolderContextEntry(const FName InEntryName, const FText& InLabel, const FText& InToolTip,
		std::function<void(const FToolMenuContext&)> InAction)
	{
		UToolMenu* Menu = UToolMenus::Get()->FindMenu(TEXT("ContentBrowser.FolderContextMenu"));
		if (!Menu)
		{
			return;
		}

		FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(InEntryName, InLabel, InToolTip, TAttribute<FSlateIcon>(),
			FToolUIActionChoice(FToolMenuExecuteAction::CreateLambda(
				[InAction](const FToolMenuContext& InContext)
				{
					if (InAction)
					{
						InAction(InContext);
					}
				})));
		Menu->AddMenuEntry(TEXT("PTSEx"), Entry);
	}

	static void AddAssetContextEntry(const FName InEntryName, const FText& InLabel, const FText& InToolTip,
		std::function<void(const FToolMenuContext&)> InAction)
	{
		UToolMenu* Menu = UToolMenus::Get()->FindMenu(TEXT("ContentBrowser.AssetContextMenu"));
		if (!Menu)
		{
			return;
		}

		FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(InEntryName, InLabel, InToolTip, TAttribute<FSlateIcon>(),
			FToolUIActionChoice(FToolMenuExecuteAction::CreateLambda(
				[InAction](const FToolMenuContext& InContext)
				{
					if (InAction)
					{
						InAction(InContext);
					}
				})));
		Menu->AddMenuEntry(TEXT("PTSEx"), Entry);
	}
};

// ---------------------------------------------------------------------------
// cpp.PTS_Console — console commands with TS handlers
// ---------------------------------------------------------------------------
struct PTS_Console
{
	static int32 AddCommand(const FString& InName, const FString& InHelp,
		puerts::Function InCommand)
	{
		return FPuerTSExtendedModule::Get().RegisterTsConsoleCommand(InName, InHelp, MoveTemp(InCommand));
	}

	static void RemoveCommand(int32 InHandle)
	{
		FPuerTSExtendedModule::Get().UnregisterTsConsoleCommand(InHandle);
	}
};

// ---------------------------------------------------------------------------
// cpp.PTS_Windows — editor sub-windows hosting TS UMG
// ---------------------------------------------------------------------------
struct PTS_Windows
{
	static int32 Open(UUserWidget* InWidget, const FString& InTitle, int32 InWidth, int32 InHeight)
	{
		return PTSEx::OpenWindow(InWidget, InTitle, InWidth, InHeight);
	}

	static int32 OpenSlate(FPTSExSlateWidget InWidget, const FString& InTitle, int32 InWidth, int32 InHeight)
	{
		return PTSEx::OpenWindowSlate(InWidget, InTitle, InWidth, InHeight);
	}

	static void Close(int32 InId)
	{
		PTSEx::CloseWindow(InId);
	}

	static void CloseAll()
	{
		PTSEx::CloseAllWindows();
	}

	static bool IsOpen(int32 InId)
	{
		return PTSEx::IsWindowOpen(InId);
	}
};

// ---------------------------------------------------------------------------
// cpp.PTS_Tabs — nomad workspace tabs hosting TS UMG
// ---------------------------------------------------------------------------
struct PTS_Tabs
{
	static void Register(const FString& InTabId, const FString& InTitle, puerts::Function InSpawnCb,
		const FString& InIconStyle, const FString& InIconName)
	{
		PTSEx::RegisterTab(InTabId, InTitle, InSpawnCb, InIconStyle, InIconName);
	}

	static void RegisterSlate(const FString& InTabId, const FString& InTitle, puerts::Function InSpawnCb,
		const FString& InIconStyle, const FString& InIconName)
	{
		PTSEx::RegisterTabSlate(InTabId, InTitle, InSpawnCb, InIconStyle, InIconName);
	}

	static void Unregister(const FString& InTabId)
	{
		PTSEx::UnregisterTab(InTabId);
	}

	static void Open(const FString& InTabId)
	{
		PTSEx::OpenTab(InTabId);
	}
};

// ---------------------------------------------------------------------------
// cpp.PTS_Details — detail panel customizations from TS UMG rows
// ---------------------------------------------------------------------------
struct PTS_Details
{
	static void Add(UClass* InClass, puerts::Function InCb)
	{
		PTSEx::AddDetailCustomization(InClass, InCb);
	}

	static void AddSlate(UClass* InClass, puerts::Function InCb)
	{
		PTSEx::AddDetailCustomizationSlate(InClass, InCb);
	}

	static void Remove(UClass* InClass)
	{
		PTSEx::RemoveDetailCustomization(InClass);
	}
};

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------
using FSlateIconStyleName = const FName&;

UsingCppType(PTS_Core);
UsingCppType(PTS_Menus);
UsingCppType(PTS_Console);
UsingCppType(PTS_Windows);
UsingCppType(PTS_Tabs);
UsingCppType(PTS_Details);
UsingUStruct(FLinearColor);

struct FAutoRegisterForPTSEx
{
	FAutoRegisterForPTSEx()
	{
		puerts::DefineClass<FSlateIcon>()
			.Constructor(CombineConstructors(
				MakeConstructor(FSlateIcon),
				MakeConstructor(FSlateIcon, FSlateIconStyleName, FSlateIconStyleName),
				MakeConstructor(FSlateIcon, FSlateIconStyleName, FSlateIconStyleName, FSlateIconStyleName)))
			.Register();

		puerts::DefineClass<FToolMenuEntry>()
			.Function("InitMenuEntry", MakeFunction(&PTS_Menus::InitMenuEntry))
			.Function("InitToolBarButton", MakeFunction(&PTS_Menus::InitToolBarButton))
			.Function("InitComboButton", MakeFunction(&PTS_Menus::InitComboButton, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon()))
			.Register();

		puerts::DefineClass<FContentBrowserItem>()
			.Method("IsFolder", MakeFunction(&FContentBrowserItem::IsFolder))
			.Method("IsFile", MakeFunction(&FContentBrowserItem::IsFile))
			.Method("GetItemName", MakeFunction(&FContentBrowserItem::GetItemName))
			.Method("GetItemPhysicalPath", MakeFunction(&FContentBrowserItem::GetItemPhysicalPath))
			.Register();

		puerts::DefineClass<PTS_Core>()
			.Function("SetOnJsEnvPreReload", MakeFunction(&PTS_Core::SetOnJsEnvPreReload))
			.Function("SetOnJsEnvCleanup", MakeFunction(&PTS_Core::SetOnJsEnvCleanup))
			.Function("SetEval", MakeFunction(&PTS_Core::SetEval))
			.Function("Reload", MakeFunction(&PTS_Core::Reload))
			.Function("Eval", MakeFunction(&PTS_Core::Eval))
			.Function("GetProjectDir", MakeFunction(&PTS_Core::GetProjectDir))
			.Function("GetContentDir", MakeFunction(&PTS_Core::GetContentDir))
			.Function("GetEngineVersion", MakeFunction(&PTS_Core::GetEngineVersion))
			.Function("Notify", MakeFunction(&PTS_Core::Notify))
			.Function("MessageBox", MakeFunction(&PTS_Core::MessageBox))
			.Function("OnMapChanged", MakeFunction(&PTS_Core::OnMapChanged))
			.Register();

		puerts::DefineClass<PTS_Menus>()
			.Function("InitMenuEntry", MakeFunction(&PTS_Menus::InitMenuEntry))
			.Function("InitToolBarButton", MakeFunction(&PTS_Menus::InitToolBarButton))
			.Function("InitComboButton", MakeFunction(&PTS_Menus::InitComboButton, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon()))
			.Function("AddEntry", MakeFunction(&PTS_Menus::AddEntry))
			.Function("AddFolderContextEntry", MakeFunction(&PTS_Menus::AddFolderContextEntry))
			.Function("AddAssetContextEntry", MakeFunction(&PTS_Menus::AddAssetContextEntry))
			.Register();

		puerts::DefineClass<PTS_Console>()
			.Function("AddCommand", MakeFunction(&PTS_Console::AddCommand))
			.Function("RemoveCommand", MakeFunction(&PTS_Console::RemoveCommand))
			.Register();

		puerts::DefineClass<PTS_Windows>()
			.Function("Open", MakeFunction(&PTS_Windows::Open))
			.Function("OpenSlate", MakeFunction(&PTS_Windows::OpenSlate))
			.Function("Close", MakeFunction(&PTS_Windows::Close))
			.Function("CloseAll", MakeFunction(&PTS_Windows::CloseAll))
			.Function("IsOpen", MakeFunction(&PTS_Windows::IsOpen))
			.Register();

		puerts::DefineClass<PTS_Tabs>()
			.Function("Register", MakeFunction(&PTS_Tabs::Register))
			.Function("RegisterSlate", MakeFunction(&PTS_Tabs::RegisterSlate))
			.Function("Unregister", MakeFunction(&PTS_Tabs::Unregister))
			.Function("Open", MakeFunction(&PTS_Tabs::Open))
			.Register();

		puerts::DefineClass<PTS_Details>()
			.Function("Add", MakeFunction(&PTS_Details::Add))
			.Function("AddSlate", MakeFunction(&PTS_Details::AddSlate))
			.Function("Remove", MakeFunction(&PTS_Details::Remove))
			.Register();

		puerts::DefineClass<FPTSExSlateWidget>()
			.Function("VerticalBox", MakeFunction(&FPTSExSlateWidget::VerticalBox))
			.Function("HorizontalBox", MakeFunction(&FPTSExSlateWidget::HorizontalBox))
			.Function("ScrollBox", MakeFunction(&FPTSExSlateWidget::ScrollBox))
			.Function("Border", MakeFunction(&FPTSExSlateWidget::Border))
			.Function("Button", MakeFunction(&FPTSExSlateWidget::Button))
			.Function("CheckBox", MakeFunction(&FPTSExSlateWidget::CheckBox))
			.Function("TextBlock", MakeFunction(&FPTSExSlateWidget::TextBlock))
			.Function("EditableTextBox", MakeFunction(&FPTSExSlateWidget::EditableTextBox))
			.Function("Image", MakeFunction(&FPTSExSlateWidget::Image))
			.Function("Spacer", MakeFunction(&FPTSExSlateWidget::Spacer))
			.Function("Splitter", MakeFunction(&FPTSExSlateWidget::Splitter))
			.Function("Slider", MakeFunction(&FPTSExSlateWidget::Slider))
			.Function("ProgressBar", MakeFunction(&FPTSExSlateWidget::ProgressBar))
			.Function("ComboBox", MakeFunction(&FPTSExSlateWidget::ComboBox))
			.Function("Separator", MakeFunction(&FPTSExSlateWidget::Separator))
			.Function("ColorBlock", MakeFunction(&FPTSExSlateWidget::ColorBlock))
			.Method("Add", MakeFunction(&FPTSExSlateWidget::Add, 0.0f, 0.0f, 0, 0))
			.Method("SetText", MakeFunction(&FPTSExSlateWidget::SetText))
			.Method("SetFontSize", MakeFunction(&FPTSExSlateWidget::SetFontSize))
			.Method("SetColor", MakeFunction(&FPTSExSlateWidget::SetColor))
			.Method("SetPadding", MakeFunction(&FPTSExSlateWidget::SetPadding))
			.Method("SetVisibility", MakeFunction(&FPTSExSlateWidget::SetVisibility))
			.Method("SetValue", MakeFunction(&FPTSExSlateWidget::SetValue))
			.Method("Clear", MakeFunction(&FPTSExSlateWidget::Clear))
			.Register();
	}
};

static FAutoRegisterForPTSEx GAutoRegisterForPTSEx;
