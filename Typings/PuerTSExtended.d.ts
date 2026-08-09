// PuerTSExtended — TypeScript typings for the editor-tooling env.
//
// Reference this from a TS project to get intellisense for the cpp.PTS_*
// API surface. The runtime objects are registered by the plugin's C++ module
// (PTSExBindings.cpp) — these declarations mirror them exactly.

declare namespace cpp {
    // ------------------------------------------------------------------
    // Lifecycle + misc
    // ------------------------------------------------------------------
    namespace PTS_Core {
        /** Called before the env is torn down (hot reload / PuerTSEx.Reload). Clean up windows/tabs here. */
        function SetOnJsEnvPreReload(cb: () => void): void;
        /** Called after the env was torn down. */
        function SetOnJsEnvCleanup(cb: () => void): void;
        /** Install the eval hook (enables PuerTSEx.Eval console command). */
        function SetEval(cb: (code: string) => void): void;
        /** Full env restart (same as PuerTSEx.Reload). */
        function Reload(): void;
        /** Evaluate a string in this env (requires SetEval). */
        function Eval(code: string): void;
        function GetProjectDir(): string;
        function GetContentDir(): string;
        function GetEngineVersion(): string;
        /** Editor toast. type: 0=Info 1=Success 2=Warning 3=Error. */
        function Notify(title: string, text: string, type: number): void;
        /**
         * Modal message box. buttons is an EAppMsgType (0=Ok, 1=YesNo, 2=OkCancel,
         * 3=YesNoCancel, 4=CancelRetryContinue, 5=YesNoYesAllNoAll). Returns an
         * EAppReturnType (0=No, 1=Yes, 2=YesAll, 3=NoAll, 4=Cancel, 5=Ok, 6=Retry, 7=Continue).
         */
        function MessageBox(title: string, text: string, buttons: number): number;
        /** (worldName, EMapChangeType) — changeType 0 = TearDownWorld. */
        function OnMapChanged(cb: (worldName: string, changeType: number) => void): void;
    }

    // ------------------------------------------------------------------
    // Menus / toolbar / context entries
    // ------------------------------------------------------------------
    interface FToolMenuEntry {
        InitMenuEntry(name: string, label: string, toolTip: string, onClick: (context?: UE.ToolMenuContext) => void): FToolMenuEntry;
        InitToolBarButton(name: string, label: string, onClick: (context?: UE.ToolMenuContext) => void): FToolMenuEntry;
        InitComboButton(
            name: string,
            onClick: ((context?: UE.ToolMenuContext) => void) | undefined,
            menuBuilder: (menu: UE.ToolMenu) => void,
            label?: string,
            toolTip?: string,
            icon?: cpp.FSlateIcon
        ): FToolMenuEntry;
    }
    interface FSlateIcon {
        new (): FSlateIcon;
        new (styleSetName: string, iconName: string): FSlateIcon;
        new (styleSetName: string, iconName: string, smallIconName: string): FSlateIcon;
    }
    interface FContentBrowserItem {
        IsFolder(): boolean;
        IsFile(): boolean;
        GetItemName(): string;
        GetItemPhysicalPath(outPath: $Ref<string>): boolean;
    }

    namespace PTS_Menus {
        /** Build a menu entry with a TS click handler. */
        function InitMenuEntry(name: string, label: string, toolTip: string, onClick: (context?: UE.ToolMenuContext) => void): FToolMenuEntry;
        /** Build a toolbar button with a TS click handler. */
        function InitToolBarButton(name: string, label: string, onClick: (context?: UE.ToolMenuContext) => void): FToolMenuEntry;
        /** Build a combo button whose dropdown is built by a TS callback. */
        function InitComboButton(
            name: string,
            onClick: ((context?: UE.ToolMenuContext) => void) | undefined,
            menuBuilder: (menu: UE.ToolMenu) => void,
            label?: string,
            toolTip?: string,
            icon?: cpp.FSlateIcon
        ): FToolMenuEntry;
        /** Add a pre-built entry to a menu (UToolMenu::AddMenuEntry is not reflected, use this). */
        function AddEntry(menu: UE.ToolMenu, section: string, entry: FToolMenuEntry): void;
        /** Register an entry on the content browser folder context menu. */
        function AddFolderContextEntry(name: string, label: string, toolTip: string, onClick: (context: UE.ToolMenuContext) => void): void;
        /** Register an entry on the content browser asset context menu. */
        function AddAssetContextEntry(name: string, label: string, toolTip: string, onClick: (context: UE.ToolMenuContext) => void): void;
    }

    // ------------------------------------------------------------------
    // Console commands
    // ------------------------------------------------------------------
    namespace PTS_Console {
        /** Register a console command backed by a TS handler. Returns a handle. */
        function AddCommand(name: string, help: string, cb: (...args: string[]) => void): number;
        /** Remove a command by handle. */
        function RemoveCommand(handle: number): void;
    }

    // ------------------------------------------------------------------
    // Editor windows hosting TS UMG
    // ------------------------------------------------------------------
    namespace PTS_Windows {
        /**
         * Open an editor sub-window hosting a TS-built UUserWidget.
         * CRITICAL: call widget.EnsureTreeBuilt() BEFORE passing it — TakeWidget
         * bakes the slate root and a tree built later renders as an empty spacer.
         * Returns a window id (0 = failure).
         */
        function Open(widget: UE.UserWidget, title: string, width: number, height: number): number;
        function Close(id: number): void;
        function CloseAll(): void;
        function IsOpen(id: number): boolean;
    }

    // ------------------------------------------------------------------
    // Nomad workspace tabs hosting TS UMG
    // ------------------------------------------------------------------
    namespace PTS_Tabs {
        /**
         * Register a workspace tab. spawnCb must create + EnsureTreeBuilt + return
         * a UUserWidget. iconStyle/iconName are FAppStyle brush names ('' = default).
         */
        function Register(tabId: string, title: string, spawnCb: () => UE.UserWidget, iconStyle?: string, iconName?: string): void;
        function Unregister(tabId: string): void;
        /** Open/focus the tab. */
        function Open(tabId: string): void;
    }

    // ------------------------------------------------------------------
    // Detail panel customizations
    // ------------------------------------------------------------------
    namespace PTS_Details {
        /**
         * Customize the details panel of a class. cb receives the selected objects
         * and must return an array of { Label: string, Widget: UE.UserWidget } rows
         * (widgets must already have their tree built).
         */
        function Add(classObj: UE.Class, cb: (objects: UE.Object[]) => Array<{ Label: string; Widget: UE.UserWidget }>): void;
        /** Same as Add but rows host native Slate widgets instead of UMG. */
        function AddSlate(classObj: UE.Class, cb: (objects: UE.Object[]) => Array<{ Label: string; Widget: PTSEx_SlateWidget }>): void;
        function Remove(classObj: UE.Class): void;
    }

    // ------------------------------------------------------------------
    // Native Slate widget builder (declarative Slate from TS, no UMG)
    // ------------------------------------------------------------------
    interface PTSEx_SlateWidget {
        // Factories
        VerticalBox(): PTSEx_SlateWidget;
        HorizontalBox(): PTSEx_SlateWidget;
        ScrollBox(): PTSEx_SlateWidget;
        Border(): PTSEx_SlateWidget;
        Button(label: string, onClick: () => void): PTSEx_SlateWidget;
        CheckBox(checked: boolean, onCheckStateChanged: (state: number) => void): PTSEx_SlateWidget;
        TextBlock(text: string): PTSEx_SlateWidget;
        EditableTextBox(text: string, onTextChanged: (text: string) => void): PTSEx_SlateWidget;
        Image(styleSet: string, styleName: string): PTSEx_SlateWidget;
        Spacer(width: number, height: number): PTSEx_SlateWidget;
        Splitter(vertical: boolean): PTSEx_SlateWidget;

        // Builder methods (chainable)
        /** Add a child. Padding = margin, Fill = fill size (0 = auto), HAlign/VAlign = 0 Fill,1 Left,2 Center,3 Right / 0 Fill,1 Top,2 Center,3 Bottom */
        Add(child: PTSEx_SlateWidget, padding?: number, fill?: number, hAlign?: number, vAlign?: number): PTSEx_SlateWidget;
        SetText(text: string): PTSEx_SlateWidget;
        SetFontSize(size: number): PTSEx_SlateWidget;
        SetColor(color: UE.LinearColor): PTSEx_SlateWidget;
        SetPadding(padding: number): PTSEx_SlateWidget;
        /** 0=Visible 1=Collapsed 2=Hidden 3=HitTestInvisible 4=SelfHitTestInvisible */
        SetVisibility(visibility: number): PTSEx_SlateWidget;
        Clear(): PTSEx_SlateWidget;
    }

    namespace PTS_Windows {
        /** Open a window hosting a TS-built UUserWidget. Returns a window id (0 = failure). */
        function Open(widget: UE.UserWidget, title: string, width: number, height: number): number;
        /** Open a window hosting a native Slate widget. Returns a window id (0 = failure). */
        function OpenSlate(widget: PTSEx_SlateWidget, title: string, width: number, height: number): number;
        function Close(id: number): void;
        function CloseAll(): void;
        function IsOpen(id: number): boolean;
    }

    namespace PTS_Tabs {
        /** Register a workspace tab hosting TS UMG. */
        function Register(tabId: string, title: string, spawnCb: () => UE.UserWidget, iconStyle?: string, iconName?: string): void;
        /** Register a workspace tab hosting native Slate. */
        function RegisterSlate(tabId: string, title: string, spawnCb: () => PTSEx_SlateWidget, iconStyle?: string, iconName?: string): void;
        function Unregister(tabId: string): void;
        /** Open/focus the tab. */
        function Open(tabId: string): void;
    }
}
