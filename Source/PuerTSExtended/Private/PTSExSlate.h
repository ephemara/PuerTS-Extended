// Copyright Template_God. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Binding.hpp"       // MUST be included first to resolve circular dependencies
#include "UEDataBinding.hpp" // For Converter templates
#include "Object.hpp"        // puerts::Function

class UUserWidget;
class SWidget;

enum class EPTSExSlateType
{
	VerticalBox,
	HorizontalBox,
	ScrollBox,
	Border,
	Button,
	CheckBox,
	TextBlock,
	EditableTextBox,
	Image,
	Spacer,
	Splitter,
	Leaf
};

/**
 * A C++ wrapper around Slate SWidget.
 * Allows declarative Slate layout creation directly from TypeScript/JavaScript.
 * GLOBAL namespace on purpose: the PuerTS typings generator does not support
 * namespaced C++ types (it would emit `PTSEx::FPTSExSlateWidget` = invalid TS).
 */
class FPTSExSlateWidget
{
public:
	TSharedPtr<SWidget> Widget;
	EPTSExSlateType Type = EPTSExSlateType::Leaf;

	FPTSExSlateWidget() = default;
	FPTSExSlateWidget(TSharedPtr<SWidget> InWidget, EPTSExSlateType InType)
		: Widget(InWidget), Type(InType) {}

	// --- Factories ---
	static FPTSExSlateWidget VerticalBox();
	static FPTSExSlateWidget HorizontalBox();
	static FPTSExSlateWidget ScrollBox();
	static FPTSExSlateWidget Border();
	static FPTSExSlateWidget Button(const FString& InLabel, puerts::Function InClickCb);
	static FPTSExSlateWidget CheckBox(bool bChecked, puerts::Function InCheckStateChangedCb);
	static FPTSExSlateWidget TextBlock(const FString& InText);
	static FPTSExSlateWidget EditableTextBox(const FString& InText, puerts::Function InOnTextChangedCb);
	static FPTSExSlateWidget Image(const FName& InStyleSet, const FName& InStyleName);
	static FPTSExSlateWidget Spacer(float InWidth, float InHeight);
	static FPTSExSlateWidget Splitter(bool bVertical);

	// --- Builder methods (chainable) ---
	FPTSExSlateWidget Add(const FPTSExSlateWidget& Child, float Padding = 0.0f, float Fill = 0.0f, int32 HAlign = 0, int32 VAlign = 0);
	FPTSExSlateWidget SetText(const FString& InText);
	FPTSExSlateWidget SetFontSize(int32 InSize);
	FPTSExSlateWidget SetColor(const FLinearColor& InColor);
	FPTSExSlateWidget SetPadding(float InPadding);
	FPTSExSlateWidget SetVisibility(int32 InVisibility);
	FPTSExSlateWidget Clear();
};

// PuerTS bindings: ScriptTypeName + pointer + by-value converters (needed by
// every TU that converts FPTSExSlateWidget to/from JS — factories return by
// value and builder methods take const refs). MUST be at global scope.
UsingCppType(FPTSExSlateWidget);
__DefCDataConverter(FPTSExSlateWidget);

namespace PTSEx
{
// ---- Windows (editor sub-windows hosting TS-built UMG widgets) ----
/** Open a window hosting a TS-built UUserWidget. Returns a window id (0 = failure). */
int32 OpenWindow(UUserWidget* InWidget, const FString& InTitle, int32 InWidth, int32 InHeight);

/** Open a window hosting a Slate widget. */
int32 OpenWindowSlate(const FPTSExSlateWidget& InWidget, const FString& InTitle, int32 InWidth, int32 InHeight);

/** Close a window by id. */
void CloseWindow(int32 InId);

/** Close every open PTSEx window (call from the pre-reload hook). */
void CloseAllWindows();

/** True if a window with this id is still open. */
bool IsWindowOpen(int32 InId);

// ---- Nomad tabs ----
/** Register a workspace tab whose content is spawned by a TS callback returning a UUserWidget. */
void RegisterTab(const FString& InTabId, const FString& InTitle, const puerts::Function& InSpawnCb,
	const FString& InIconStyle, const FString& InIconName);

/** Register a workspace tab whose content is spawned by a TS callback returning a Slate widget. */
void RegisterTabSlate(const FString& InTabId, const FString& InTitle, const puerts::Function& InSpawnCb,
	const FString& InIconStyle, const FString& InIconName);

/** Remove a previously registered tab. */
void UnregisterTab(const FString& InTabId);

/** Invoke an existing nomad tab (opens/focuses it). */
void OpenTab(const FString& InTabId);

// ---- Detail customizations ----
/**
 * Register a detail-panel customization for a class. The TS callback receives
 * the selected objects and returns an array of { Label: string, Widget: UUserWidget }
 * rows rendered into the panel. Widgets must have their tree built already.
 */
void AddDetailCustomization(UClass* InClass, const puerts::Function& InCb);

/** Register a detail customization that returns Slate widgets. */
void AddDetailCustomizationSlate(UClass* InClass, const puerts::Function& InCb);

/** Remove a previously registered detail customization. */
void RemoveDetailCustomization(UClass* InClass);

// ---- Notifications / dialogs ----
/** Editor toast. Type: 0=Info 1=Success 2=Warning 3=Error. */
void Notify(const FString& InTitle, const FString& InText, int32 InType);

/**
 * Modal message box. InButtons is an EAppMsgType value (0=Ok, 1=YesNo, 2=OkCancel,
 * 3=YesNoCancel, ...). Returns an EAppReturnType value (0=No, 1=Yes, 2=YesAll,
 * 3=NoAll, 4=Cancel, 5=Ok, 6=Retry, 7=Continue).
 */
int32 MessageBox(const FString& InTitle, const FString& InText, int32 InButtons);

/** Tear down every editor-facing registration (windows, tabs, details). Called before env reload. */
void ClearAllState();
}
