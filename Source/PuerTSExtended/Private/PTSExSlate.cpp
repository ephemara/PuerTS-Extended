// Copyright Template_God. All Rights Reserved.

#include "PTSExSlate.h"

#include "PTSExInvoke.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h" // FDetailWidgetRow definition
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/SlateDelegates.h" // FOnClicked etc.
#include "IDetailCustomization.h"
#include "Misc/MessageDialog.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SNotificationList.h" // FNotificationInfo definition
#include "Widgets/SNullWidget.h"
#include "Widgets/SWindow.h"
#include "Input/Reply.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#include "Blueprint/UserWidget.h"

namespace PTSEx
{
namespace
{
// ---------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------
struct FWindowRecord
{
	TSharedPtr<SWindow> Window;
	UUserWidget* Widget = nullptr; // rooted until closed
};

TMap<int32, FWindowRecord> GWindows;
int32 GNextWindowId = 1;

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------
struct FTabRecord
{
	puerts::Function SpawnCb;
};

TMap<FName, FTabRecord> GTabs;
TSet<UUserWidget*> GPinnedTabWidgets;

// ---------------------------------------------------------------------------
// Details
// ---------------------------------------------------------------------------
class FPTSExDetailCustomization : public IDetailCustomization
{
public:
	explicit FPTSExDetailCustomization(const puerts::Function& InCb)
		: Cb(InCb)
	{
	}

	virtual ~FPTSExDetailCustomization() override
	{
		for (UUserWidget* Widget : PinnedWidgets)
		{
			if (IsValid(Widget))
			{
				Widget->RemoveFromRoot();
			}
		}
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		TArray<TWeakObjectPtr<UObject>> WeakObjects;
		DetailBuilder.GetObjectsBeingCustomized(WeakObjects);

		TArray<UObject*> Objects;
		for (const TWeakObjectPtr<UObject>& Weak : WeakObjects)
		{
			if (UObject* Strong = Weak.Get())
			{
				Objects.Add(Strong);
			}
		}

		if (!Cb.Isolate || Cb.GObject.IsEmpty())
		{
			return;
		}

		FTsCallScope Scope(Cb);
		v8::Isolate* Isolate = Scope.GetIsolate();
		v8::Local<v8::Context> Ctx = Scope.GetContext();
		v8::TryCatch TryCatch(Isolate);

		v8::Local<v8::Array> ObjArr = v8::Array::New(Isolate, Objects.Num());
		for (int32 i = 0; i < Objects.Num(); ++i)
		{
			ObjArr->Set(Ctx, i, puerts::v8_impl::Converter<UObject*>::toScript(Ctx, Objects[i]));
		}

		v8::Local<v8::Value> Args[] = { ObjArr };
		v8::Local<v8::Value> Result;
		if (!Scope.GetFunction()->Call(Ctx, v8::Undefined(Isolate), 1, Args).ToLocal(&Result))
		{
			LogCaughtException(Isolate, &TryCatch);
			return;
		}

		if (Result.IsEmpty() || !Result->IsArray())
		{
			return;
		}

		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("PuerTSExtended"));

		v8::Local<v8::Array> Rows = Result.As<v8::Array>();
		for (uint32 i = 0; i < Rows->Length(); ++i)
		{
			v8::Local<v8::Value> Row;
			if (!Rows->Get(Ctx, i).ToLocal(&Row) || Row.IsEmpty() || !Row->IsObject())
			{
				continue;
			}

			v8::Local<v8::Object> RowObj = Row.As<v8::Object>();
			v8::Local<v8::Value> LabelVal;
			v8::Local<v8::Value> WidgetVal;
			if (!RowObj->Get(Ctx, puerts::FV8Utils::ToV8String(Isolate, TEXT("Label"))).ToLocal(&LabelVal) ||
				!RowObj->Get(Ctx, puerts::FV8Utils::ToV8String(Isolate, TEXT("Widget"))).ToLocal(&WidgetVal))
			{
				continue;
			}

			FString Label = puerts::v8_impl::Converter<FString>::toCpp(Ctx, LabelVal);
			UUserWidget* Widget = puerts::v8_impl::Converter<UUserWidget*>::toCpp(Ctx, WidgetVal);
			if (!Widget)
			{
				continue;
			}

			// Pin the widget for as long as this adapter lives (until the panel rebuilds).
			Widget->AddToRoot();
			PinnedWidgets.Add(Widget);

			Category.AddCustomRow(FText::FromString(Label)).WholeRowContent()
			[
				Widget->TakeWidget()
			];
		}
	}

private:
	puerts::Function Cb;
	TArray<UUserWidget*> PinnedWidgets;
};

TSet<FName> GDetailClasses;
} // namespace

// ---------------------------------------------------------------------------
// Slate widget wrapper (declarative Slate from TS)
// ---------------------------------------------------------------------------
FPTSExSlateWidget FPTSExSlateWidget::VerticalBox()
{
	return FPTSExSlateWidget(SNew(SVerticalBox), EPTSExSlateType::VerticalBox);
}

FPTSExSlateWidget FPTSExSlateWidget::HorizontalBox()
{
	return FPTSExSlateWidget(SNew(SHorizontalBox), EPTSExSlateType::HorizontalBox);
}

FPTSExSlateWidget FPTSExSlateWidget::ScrollBox()
{
	return FPTSExSlateWidget(SNew(SScrollBox), EPTSExSlateType::ScrollBox);
}

FPTSExSlateWidget FPTSExSlateWidget::Border()
{
	return FPTSExSlateWidget(SNew(SBorder), EPTSExSlateType::Border);
}

FPTSExSlateWidget FPTSExSlateWidget::Button(const FString& InLabel, puerts::Function InClickCb)
{
	puerts::Function ClickCb = InClickCb;
	FOnClicked Delegate = FOnClicked::CreateLambda([ClickCb]()
	{
		if (ClickCb.Isolate && !ClickCb.GObject.IsEmpty())
		{
			PTSEx::Invoke(ClickCb);
		}
		return FReply::Handled();
	});

	return FPTSExSlateWidget(
		SNew(SButton).OnClicked(Delegate)[SNew(STextBlock).Text(FText::FromString(InLabel))],
		EPTSExSlateType::Button
	);
}

FPTSExSlateWidget FPTSExSlateWidget::CheckBox(bool bChecked, puerts::Function InCheckStateChangedCb)
{
	puerts::Function CheckCb = InCheckStateChangedCb;
	FOnCheckStateChanged Delegate = FOnCheckStateChanged::CreateLambda([CheckCb](ECheckBoxState NewState)
	{
		if (CheckCb.Isolate && !CheckCb.GObject.IsEmpty())
		{
			PTSEx::InvokeWithInt(CheckCb, (int32)NewState);
		}
	});

	return FPTSExSlateWidget(
		SNew(SCheckBox).IsChecked(bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked).OnCheckStateChanged(Delegate),
		EPTSExSlateType::CheckBox
	);
}

FPTSExSlateWidget FPTSExSlateWidget::TextBlock(const FString& InText)
{
	return FPTSExSlateWidget(SNew(STextBlock).Text(FText::FromString(InText)), EPTSExSlateType::TextBlock);
}

FPTSExSlateWidget FPTSExSlateWidget::EditableTextBox(const FString& InText, puerts::Function InOnTextChangedCb)
{
	puerts::Function TextChangedCb = InOnTextChangedCb;
	FOnTextChanged Delegate = FOnTextChanged::CreateLambda([TextChangedCb](const FText& NewText)
	{
		if (TextChangedCb.Isolate && !TextChangedCb.GObject.IsEmpty())
		{
			PTSEx::InvokeWithString(TextChangedCb, NewText.ToString());
		}
	});

	return FPTSExSlateWidget(
		SNew(SEditableTextBox).Text(FText::FromString(InText)).OnTextChanged(Delegate),
		EPTSExSlateType::EditableTextBox
	);
}

FPTSExSlateWidget FPTSExSlateWidget::Image(const FName& InStyleSet, const FName& InStyleName)
{
	return FPTSExSlateWidget(SNew(SImage).Image(FSlateIcon(InStyleSet, InStyleName).GetIcon()), EPTSExSlateType::Leaf);
}

FPTSExSlateWidget FPTSExSlateWidget::Spacer(float InWidth, float InHeight)
{
	return FPTSExSlateWidget(SNew(SSpacer).Size(FVector2D(InWidth, InHeight)), EPTSExSlateType::Leaf);
}

FPTSExSlateWidget FPTSExSlateWidget::Splitter(bool bVertical)
{
	return FPTSExSlateWidget(
		SNew(SSplitter).Orientation(bVertical ? Orient_Vertical : Orient_Horizontal),
		EPTSExSlateType::Splitter
	);
}

FPTSExSlateWidget FPTSExSlateWidget::Add(const FPTSExSlateWidget& Child, float Padding, float Fill, int32 HAlign, int32 VAlign)
{
	if (!Widget.IsValid() || !Child.Widget.IsValid())
	{
		return *this;
	}

	const FMargin Margin(Padding);
	const EHorizontalAlignment HAlignEnum = (EHorizontalAlignment)HAlign;
	const EVerticalAlignment VAlignEnum = (EVerticalAlignment)VAlign;

	if (Type == EPTSExSlateType::VerticalBox)
	{
		auto Slot = static_cast<SVerticalBox*>(Widget.Get())->AddSlot();
		Slot.Padding(Margin).HAlign(HAlignEnum).VAlign(VAlignEnum);
		if (Fill > 0.0f) Slot.FillHeight(Fill); else Slot.AutoHeight();
		Slot[Child.Widget.ToSharedRef()];
	}
	else if (Type == EPTSExSlateType::HorizontalBox)
	{
		auto Slot = static_cast<SHorizontalBox*>(Widget.Get())->AddSlot();
		Slot.Padding(Margin).HAlign(HAlignEnum).VAlign(VAlignEnum);
		if (Fill > 0.0f) Slot.FillWidth(Fill); else Slot.AutoWidth();
		Slot[Child.Widget.ToSharedRef()];
	}
	else if (Type == EPTSExSlateType::ScrollBox)
	{
		auto Slot = static_cast<SScrollBox*>(Widget.Get())->AddSlot();
		Slot.Padding(Margin).HAlign(HAlignEnum).VAlign(VAlignEnum);
		Slot[Child.Widget.ToSharedRef()];
	}
	else if (Type == EPTSExSlateType::Splitter)
	{
		auto Slot = static_cast<SSplitter*>(Widget.Get())->AddSlot();
		if (Fill > 0.0f) Slot.Value(Fill);
		Slot[Child.Widget.ToSharedRef()];
	}
	else if (Type == EPTSExSlateType::Border || Type == EPTSExSlateType::Button)
	{
		static_cast<SBorder*>(Widget.Get())->SetContent(Child.Widget.ToSharedRef());
	}
	else if (Type == EPTSExSlateType::CheckBox)
	{
		static_cast<SCheckBox*>(Widget.Get())->SetContent(Child.Widget.ToSharedRef());
	}

	return *this;
}

FPTSExSlateWidget FPTSExSlateWidget::SetText(const FString& InText)
{
	if (Type == EPTSExSlateType::TextBlock)
	{
		static_cast<STextBlock*>(Widget.Get())->SetText(FText::FromString(InText));
	}
	else if (Type == EPTSExSlateType::EditableTextBox)
	{
		static_cast<SEditableTextBox*>(Widget.Get())->SetText(FText::FromString(InText));
	}
	return *this;
}

FPTSExSlateWidget FPTSExSlateWidget::SetFontSize(int32 InSize)
{
	FSlateFontInfo Font = FAppStyle::Get().GetFontStyle("NormalFont");
	Font.Size = InSize;

	if (Type == EPTSExSlateType::TextBlock)
	{
		static_cast<STextBlock*>(Widget.Get())->SetFont(Font);
	}
	else if (Type == EPTSExSlateType::EditableTextBox)
	{
		static_cast<SEditableTextBox*>(Widget.Get())->SetFont(Font);
	}
	return *this;
}

FPTSExSlateWidget FPTSExSlateWidget::SetColor(const FLinearColor& InColor)
{
	if (Type == EPTSExSlateType::TextBlock)
	{
		static_cast<STextBlock*>(Widget.Get())->SetColorAndOpacity(InColor);
	}
	else if (Type == EPTSExSlateType::Border)
	{
		static_cast<SBorder*>(Widget.Get())->SetBorderBackgroundColor(InColor);
	}
	return *this;
}

FPTSExSlateWidget FPTSExSlateWidget::SetPadding(float InPadding)
{
	if (Type == EPTSExSlateType::Border)
	{
		static_cast<SBorder*>(Widget.Get())->SetPadding(FMargin(InPadding));
	}
	else if (Type == EPTSExSlateType::Button)
	{
		static_cast<SButton*>(Widget.Get())->SetContentPadding(FMargin(InPadding));
	}
	return *this;
}

FPTSExSlateWidget FPTSExSlateWidget::SetVisibility(int32 InVisibility)
{
	if (!Widget.IsValid())
	{
		return *this;
	}

	EVisibility NewVisibility = EVisibility::Visible;
	switch (InVisibility)
	{
	case 1: NewVisibility = EVisibility::Collapsed; break;
	case 2: NewVisibility = EVisibility::Hidden; break;
	case 3: NewVisibility = EVisibility::HitTestInvisible; break;
	case 4: NewVisibility = EVisibility::SelfHitTestInvisible; break;
	default: break; // 0 = Visible
	}

	Widget->SetVisibility(NewVisibility);
	return *this;
}

FPTSExSlateWidget FPTSExSlateWidget::Clear()
{
	if (!Widget.IsValid())
	{
		return *this;
	}

	if (Type == EPTSExSlateType::VerticalBox)
	{
		static_cast<SVerticalBox*>(Widget.Get())->ClearChildren();
	}
	else if (Type == EPTSExSlateType::HorizontalBox)
	{
		static_cast<SHorizontalBox*>(Widget.Get())->ClearChildren();
	}
	else if (Type == EPTSExSlateType::ScrollBox)
	{
		static_cast<SScrollBox*>(Widget.Get())->ClearChildren();
	}
	else if (Type == EPTSExSlateType::Border || Type == EPTSExSlateType::Button)
	{
		static_cast<SBorder*>(Widget.Get())->ClearContent();
	}

	return *this;
}

// ---------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------
int32 OpenWindow(UUserWidget* InWidget, const FString& InTitle, int32 InWidth, int32 InHeight)
{
	if (!InWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("PTSEx: OpenWindow called with null widget"));
		return 0;
	}

	// Pin the UObject — the slate layer references it but GC must not collect it.
	InWidget->AddToRoot();

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(InTitle))
		.ClientSize(FVector2D(FMath::Max(InWidth, 200), FMath::Max(InHeight, 120)))
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.CreateTitleBar(true);

	Window->SetContent(InWidget->TakeWidget());

	const int32 Id = GNextWindowId++;
	GWindows.Add(Id, FWindowRecord{ Window, InWidget });

	Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([Id](const TSharedRef<SWindow>&)
	{
		if (FWindowRecord* Record = GWindows.Find(Id))
		{
			if (IsValid(Record->Widget))
			{
				Record->Widget->RemoveFromRoot();
			}
			GWindows.Remove(Id);
		}
	}));

	FSlateApplication::Get().AddWindow(Window);
	return Id;
}

int32 OpenWindowSlate(const FPTSExSlateWidget& InWidget, const FString& InTitle, int32 InWidth, int32 InHeight)
{
	if (!InWidget.Widget.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PTSEx: OpenWindowSlate called with null widget"));
		return 0;
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(InTitle))
		.ClientSize(FVector2D(FMath::Max(InWidth, 200), FMath::Max(InHeight, 120)))
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		.CreateTitleBar(true);

	Window->SetContent(InWidget.Widget.ToSharedRef());

	const int32 Id = GNextWindowId++;
	GWindows.Add(Id, FWindowRecord{ Window, nullptr });

	Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([Id](const TSharedRef<SWindow>&)
	{
		GWindows.Remove(Id);
	}));

	FSlateApplication::Get().AddWindow(Window);
	return Id;
}

void CloseWindow(int32 InId)
{
	if (FWindowRecord* Record = GWindows.Find(InId))
	{
		Record->Window->RequestDestroyWindow();
	}
}

void CloseAllWindows()
{
	TArray<int32> Ids;
	GWindows.GetKeys(Ids);
	for (int32 Id : Ids)
	{
		CloseWindow(Id);
	}
	GWindows.Reset();
}

bool IsWindowOpen(int32 InId)
{
	return GWindows.Contains(InId);
}

// ---------------------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------------------
void RegisterTab(const FString& InTabId, const FString& InTitle, const puerts::Function& InSpawnCb,
	const FString& InIconStyle, const FString& InIconName)
{
	if (InTabId.IsEmpty() || !InSpawnCb.Isolate || InSpawnCb.GObject.IsEmpty())
	{
		return;
	}

	const FName TabId(*InTabId);
	UnregisterTab(InTabId);

	FSlateIcon Icon;
	if (!InIconName.IsEmpty())
	{
		const FName StyleSet = InIconStyle.IsEmpty() ? FAppStyle::GetAppStyleSetName() : FName(*InIconStyle);
		Icon = FSlateIcon(StyleSet, FName(*InIconName));
	}

	FOnSpawnTab SpawnTab = FOnSpawnTab::CreateLambda([InSpawnCb](const FSpawnTabArgs&)
	{
		UUserWidget* Widget = InvokeSpawnWidget(InSpawnCb);
		if (!Widget)
		{
			return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNullWidget::NullWidget];
		}

		// Pin until the tab is closed (slate holds it, GC must not collect it).
		Widget->AddToRoot();
		GPinnedTabWidgets.Add(Widget);

		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			.OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([Widget](TSharedRef<SDockTab>)
			{
				if (IsValid(Widget))
				{
					Widget->RemoveFromRoot();
					GPinnedTabWidgets.Remove(Widget);
				}
			}))[Widget->TakeWidget()];
	});

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId, SpawnTab)
		.SetDisplayName(FText::FromString(InTitle))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(Icon);

	GTabs.Add(TabId, FTabRecord{ InSpawnCb });
}

void RegisterTabSlate(const FString& InTabId, const FString& InTitle, const puerts::Function& InSpawnCb,
	const FString& InIconStyle, const FString& InIconName)
{
	if (InTabId.IsEmpty() || !InSpawnCb.Isolate || InSpawnCb.GObject.IsEmpty())
	{
		return;
	}

	const FName TabId(*InTabId);
	UnregisterTab(InTabId);

	FSlateIcon Icon;
	if (!InIconName.IsEmpty())
	{
		const FName StyleSet = InIconStyle.IsEmpty() ? FAppStyle::GetAppStyleSetName() : FName(*InIconStyle);
		Icon = FSlateIcon(StyleSet, FName(*InIconName));
	}

	FOnSpawnTab SpawnTab = FOnSpawnTab::CreateLambda([InSpawnCb](const FSpawnTabArgs&)
	{
		FPTSExSlateWidget SlateWidget = InvokeSpawnSlateWidget(InSpawnCb);
		if (!SlateWidget.Widget.IsValid())
		{
			return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNullWidget::NullWidget];
		}

		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)[SlateWidget.Widget.ToSharedRef()];
	});

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId, SpawnTab)
		.SetDisplayName(FText::FromString(InTitle))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(Icon);

	GTabs.Add(TabId, FTabRecord{ InSpawnCb });
}

void UnregisterTab(const FString& InTabId)
{
	const FName TabId(*InTabId);
	if (GTabs.Contains(TabId))
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
		GTabs.Remove(TabId);
	}
}

void OpenTab(const FString& InTabId)
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName(*InTabId));
}

// ---------------------------------------------------------------------------
// Details
// ---------------------------------------------------------------------------
void AddDetailCustomization(UClass* InClass, const puerts::Function& InCb)
{
	if (!InClass || !InCb.Isolate || InCb.GObject.IsEmpty())
	{
		return;
	}

	RemoveDetailCustomization(InClass);

	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	PropertyEditorModule.RegisterCustomClassLayout(InClass->GetFName(),
		FOnGetDetailCustomizationInstance::CreateLambda([InCb]()
		{
			return MakeShared<FPTSExDetailCustomization>(InCb);
		}));

	GDetailClasses.Add(InClass->GetFName());
}

class FPTSExDetailCustomizationSlate : public IDetailCustomization
{
public:
	explicit FPTSExDetailCustomizationSlate(const puerts::Function& InCb)
		: Cb(InCb)
	{
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		TArray<TWeakObjectPtr<UObject>> WeakObjects;
		DetailBuilder.GetObjectsBeingCustomized(WeakObjects);

		TArray<UObject*> Objects;
		for (const TWeakObjectPtr<UObject>& Weak : WeakObjects)
		{
			if (UObject* Strong = Weak.Get())
			{
				Objects.Add(Strong);
			}
		}

		if (!Cb.Isolate || Cb.GObject.IsEmpty())
		{
			return;
		}

		FTsCallScope Scope(Cb);
		v8::Isolate* Isolate = Scope.GetIsolate();
		v8::Local<v8::Context> Ctx = Scope.GetContext();
		v8::TryCatch TryCatch(Isolate);

		v8::Local<v8::Array> ObjArr = v8::Array::New(Isolate, Objects.Num());
		for (int32 i = 0; i < Objects.Num(); ++i)
		{
			ObjArr->Set(Ctx, i, puerts::v8_impl::Converter<UObject*>::toScript(Ctx, Objects[i]));
		}

		v8::Local<v8::Value> Args[] = { ObjArr };
		v8::Local<v8::Value> Result;
		if (!Scope.GetFunction()->Call(Ctx, v8::Undefined(Isolate), 1, Args).ToLocal(&Result))
		{
			LogCaughtException(Isolate, &TryCatch);
			return;
		}

		if (Result.IsEmpty() || !Result->IsArray())
		{
			return;
		}

		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("PuerTSExtended"));

		v8::Local<v8::Array> Rows = Result.As<v8::Array>();
		for (uint32 i = 0; i < Rows->Length(); ++i)
		{
			v8::Local<v8::Value> Row;
			if (!Rows->Get(Ctx, i).ToLocal(&Row) || Row.IsEmpty() || !Row->IsObject())
			{
				continue;
			}

			v8::Local<v8::Object> RowObj = Row.As<v8::Object>();
			v8::Local<v8::Value> LabelVal;
			v8::Local<v8::Value> WidgetVal;
			if (!RowObj->Get(Ctx, puerts::FV8Utils::ToV8String(Isolate, TEXT("Label"))).ToLocal(&LabelVal) ||
				!RowObj->Get(Ctx, puerts::FV8Utils::ToV8String(Isolate, TEXT("Widget"))).ToLocal(&WidgetVal))
			{
				continue;
			}

			FString Label = puerts::v8_impl::Converter<FString>::toCpp(Ctx, LabelVal);
			FPTSExSlateWidget Widget = puerts::v8_impl::Converter<FPTSExSlateWidget>::toCpp(Ctx, WidgetVal);
			if (!Widget.Widget.IsValid())
			{
				continue;
			}

			Category.AddCustomRow(FText::FromString(Label)).WholeRowContent()
			[
				Widget.Widget.ToSharedRef()
			];
		}
	}

private:
	puerts::Function Cb;
};

void AddDetailCustomizationSlate(UClass* InClass, const puerts::Function& InCb)
{
	if (!InClass || !InCb.Isolate || InCb.GObject.IsEmpty())
	{
		return;
	}

	RemoveDetailCustomization(InClass);

	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	PropertyEditorModule.RegisterCustomClassLayout(InClass->GetFName(),
		FOnGetDetailCustomizationInstance::CreateLambda([InCb]()
		{
			return MakeShared<FPTSExDetailCustomizationSlate>(InCb);
		}));

	GDetailClasses.Add(InClass->GetFName());
}

void RemoveDetailCustomization(UClass* InClass)
{
	if (!InClass)
	{
		return;
	}

	if (GDetailClasses.Contains(InClass->GetFName()))
	{
		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditorModule.UnregisterCustomClassLayout(InClass->GetFName());
		GDetailClasses.Remove(InClass->GetFName());
	}
}

// ---------------------------------------------------------------------------
// Notifications / dialogs
// ---------------------------------------------------------------------------
void Notify(const FString& InTitle, const FString& InText, int32 InType)
{
	FNotificationInfo Info(FText::FromString(InText));
	Info.SubText = FText::FromString(InTitle);
	Info.bUseThrobber = false;
	Info.bUseSuccessFailIcons = false;
	Info.ExpireDuration = 4.0f;

	switch (InType)
	{
	case 1: Info.Image = FAppStyle::GetBrush(TEXT("Icons.Success")); break;
	case 2: Info.Image = FAppStyle::GetBrush(TEXT("Icons.Warning")); break;
	case 3: Info.Image = FAppStyle::GetBrush(TEXT("Icons.Error")); break;
	default: Info.Image = FAppStyle::GetBrush(TEXT("Icons.Info")); break;
	}

	FSlateNotificationManager::Get().AddNotification(Info);
}

int32 MessageBox(const FString& InTitle, const FString& InText, int32 InButtons)
{
	return (int32)FMessageDialog::Open(
		(EAppMsgType::Type)InButtons, FText::FromString(InText), FText::FromString(InTitle));
}

// ---------------------------------------------------------------------------
// Teardown
// ---------------------------------------------------------------------------
void ClearAllState()
{
	CloseAllWindows();

	for (const auto& Pair : GTabs)
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(Pair.Key);
	}
	GTabs.Reset();

	for (UUserWidget* Widget : GPinnedTabWidgets)
	{
		if (IsValid(Widget))
		{
			Widget->RemoveFromRoot();
		}
	}
	GPinnedTabWidgets.Reset();

	for (const FName& ClassName : GDetailClasses)
	{
		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditorModule.UnregisterCustomClassLayout(ClassName);
	}
	GDetailClasses.Reset();
}
}
