// ============================================================================
// PuerTSExtended — Main entry script.
//
// Runs inside the editor-only PuerTS env owned by the PuerTSExtended plugin.
// Everything here is hot-reloadable: edit this file (or anything it requires)
// and the env reloads it live. Full restart: PuerTSEx.Reload console command.
//
// API surface (see Typings/PuerTSExtended.d.ts):
//   cpp.PTS_Core     lifecycle, eval, notify, messagebox, map hooks
//   cpp.PTS_Menus    menu / toolbar / combo / context entries
//   cpp.PTS_Console  console commands
//   cpp.PTS_Windows  editor windows hosting TS UMG
//   cpp.PTS_Tabs     workspace tabs hosting TS UMG
//   cpp.PTS_Details  detail panel customizations
// ============================================================================
'use strict';
const UE = require('ue');
const cpp = require('cpp');

const Core = cpp.PTS_Core;
const Menus = cpp.PTS_Menus;
const Console = cpp.PTS_Console;
const Windows = cpp.PTS_Windows;
const Tabs = cpp.PTS_Tabs;

// ---------------------------------------------------------------------------
// A TS UMG widget class. NOTE the PuerTS rules:
//   - PuerTS calls Constructor() (not the JS constructor) — all field init
//     goes there.
//   - NEVER create UObjects in Constructor() (runs inside native construction).
//   - The tree must be built BEFORE the widget enters any slate (TakeWidget
//     bakes the root) — call EnsureTreeBuilt() at the creation site.
//   - NO static class fields on classes extending UE types (module-scope
//     let/const instead).
// ---------------------------------------------------------------------------
class PTSEx_DemoPanel extends UE.UserWidget {
    Constructor() {
        this._built = false;
    }

    EnsureTreeBuilt() {
        if (this._built) return;
        this._built = true;

        if (!this.WidgetTree) {
            this.WidgetTree = new UE.WidgetTree(this);
        }

        const root = new UE.CanvasPanel(this);
        this.WidgetTree.RootWidget = root;

        const title = new UE.TextBlock(this);
        const font = new UE.SlateFontInfo();
        font.FontObject = UE.Object.Load('/Engine/EngineFonts/Roboto.Roboto');
        font.Size = 22;
        title.Font = font;
        title.SetText('PuerTSExtended demo panel');
        let slot = root.AddChildToCanvas(title);
        slot.SetAnchors(new UE.Anchors(new UE.Vector2D(0, 0), new UE.Vector2D(1, 0)));
        slot.SetOffsets(new UE.Margin(20, 20, 20, 20));

        const body = new UE.TextBlock(this);
        const bodyFont = new UE.SlateFontInfo();
        bodyFont.FontObject = UE.Object.Load('/Engine/EngineFonts/Roboto.Roboto');
        bodyFont.Size = 14;
        body.Font = bodyFont;
        body.SetText('Built from TypeScript. Edit Content/PuertsExtended/Main.js and it hot-reloads.');
        body.SetColorAndOpacity(new UE.LinearColor(0.8, 0.8, 0.8, 1.0));
        slot = root.AddChildToCanvas(body);
        slot.SetAnchors(new UE.Anchors(new UE.Vector2D(0, 0), new UE.Vector2D(1, 1)));
        slot.SetOffsets(new UE.Margin(20, 60, 20, 20));
    }

    Construct() {
        // Safety net — normally pre-built by the creation site.
        this.EnsureTreeBuilt();
    }
}

// ---------------------------------------------------------------------------
// Lifecycle hooks
// ---------------------------------------------------------------------------

// Enables the PuerTSEx.Eval console command + "TypeScript <code>" string commands.
Core.SetEval((code) => {
    try {
        // Indirect eval: runs in global scope.
        (0, eval)(code);
    } catch (e) {
        console.error('[PTSEx] eval error:', e);
    }
});

// Called before the env is torn down — close windows, unregister tabs, etc.
Core.SetOnJsEnvPreReload(() => {
    console.log('[PTSEx] pre-reload cleanup');
    Windows.CloseAll();
});

// Called after the env was torn down.
Core.SetOnJsEnvCleanup(() => {
    console.log('[PTSEx] env cleaned up');
});

// ---------------------------------------------------------------------------
// Console command
// ---------------------------------------------------------------------------
const helloHandle = Console.AddCommand('PTSEx.Hello', 'PTSEx demo command', (...args) => {
    console.log('[PTSEx] Hello from TypeScript! args:', args.join(','));
});
console.log('[PTSEx] registered PTSEx.Hello (handle', helloHandle + ')');

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------
const mainMenu = UE.ToolMenus.Get().FindMenu('LevelEditor.MainMenu');
const subMenu = mainMenu.AddSubMenuScript('PTSEx', 'PTSEx', 'PTSEx', 'PTSEx', 'PTSEx Demo Tools');

const openWindowEntry = Menus.InitMenuEntry('PTSExOpenWindow', 'Open Demo Window', 'Opens a TS UMG window', () => {
    const w = new PTSEx_DemoPanel();
    w.EnsureTreeBuilt(); // MANDATORY before the widget enters slate
    const id = Windows.Open(w, 'PTSEx Demo Window', 520, 360);
    console.log('[PTSEx] window opened, id', id);
});
subMenu.AddMenuEntry('Scripts', openWindowEntry);

const notifyEntry = Menus.InitMenuEntry('PTSExNotify', 'Show Notification', 'Editor toast', () => {
    Core.Notify('PTSEx', 'Hello from TypeScript', 0); // 0=Info 1=Success 2=Warning 3=Error
});
subMenu.AddMenuEntry('Scripts', notifyEntry);

const msgEntry = Menus.InitMenuEntry('PTSExMessageBox', 'Message Box', 'Modal dialog', () => {
    const result = Core.MessageBox('PTSEx', 'Is this plugin a treasure find?', 1); // 1 = YesNo
    console.log('[PTSEx] MessageBox result:', result); // 0=No 1=Yes 4=Cancel 5=Ok
});
subMenu.AddMenuEntry('Scripts', msgEntry);

const tabEntry = Menus.InitMenuEntry('PTSExOpenTab', 'Open Demo Tab', 'Opens the nomad workspace tab', () => {
    Tabs.Open('PTSExDemoTab');
});
subMenu.AddMenuEntry('Scripts', tabEntry);

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------
const toolbar = UE.ToolMenus.Get().FindMenu('LevelEditor.LevelEditorToolBar');

const tbEntry = Menus.InitToolBarButton('PTSExToolbar', 'PTSEx', () => {
    Core.Notify('PTSEx', 'Toolbar button clicked', 1);
});
toolbar.AddMenuEntry('PTSEx', tbEntry);

const comboIcon = new cpp.FSlateIcon('EditorStyle', 'LevelEditor.WorldProperties', 'LevelEditor.WorldProperties.Small');
const comboEntry = Menus.InitComboButton('PTSExCombo', undefined, (subMenu) => {
    if (!subMenu) return;
    const item = Menus.InitMenuEntry('PTSExComboItem', 'Combo Item', 'Nested entry', () => {
        console.log('[PTSEx] combo item clicked');
    });
    subMenu.AddMenuEntry('PTSExCombo', item);
}, 'PTSEx', 'PTSEx combo', comboIcon);
toolbar.AddMenuEntry('PTSEx', comboEntry);

// ---------------------------------------------------------------------------
// Content browser context menus
// ---------------------------------------------------------------------------
Menus.AddFolderContextEntry('PTSExFolderInfo', 'PTSEx: Log Folders', 'Logs the selected folders', (context) => {
    const ctx = UE.ToolMenus.FindContext(context, UE.ContentBrowserDataMenuContext_FolderMenu.StaticClass());
    if (!ctx) return;
    const items = ctx.SelectedItems;
    console.log('[PTSEx] selected folders:', items.Num());
    for (let i = 0; i < items.Num(); i++) {
        const item = items.Get(i);
        let path = $ref();
        item.GetItemPhysicalPath(path);
        console.log('[PTSEx]   -', item.GetItemName(), '|', path);
    }
});

Menus.AddAssetContextEntry('PTSExAssetInfo', 'PTSEx: Log Assets', 'Logs the selected assets', (context) => {
    const ctx = UE.ToolMenus.FindContext(context, UE.ContentBrowserAssetContextMenuContext.StaticClass());
    if (!ctx) return;
    const objects = ctx.SelectedObjects;
    for (let i = 0; i < objects.Num(); i++) {
        const obj = objects.Get(i);
        console.log('[PTSEx] asset:', obj.GetClass().GetName(), '/', obj.GetName());
    }
});

// ---------------------------------------------------------------------------
// Nomad tab
// ---------------------------------------------------------------------------
Tabs.Register('PTSExDemoTab', 'PTSEx Demo', () => {
    const w = new PTSEx_DemoPanel();
    w.EnsureTreeBuilt();
    return w;
}, 'EditorStyle', 'LevelEditor.WorldProperties');

// ---------------------------------------------------------------------------
// Native Slate (no UMG) demos — cpp.PTSEx_SlateWidget wrappers
// ---------------------------------------------------------------------------
const SW = cpp.PTSEx_SlateWidget;

function buildSlatePanel() {
    // A vertical box with a header, a button, a checkbox and an editable text box.
    let header = SW.TextBlock('PTSEx Native Slate Panel').SetFontSize(18);

    let counter = 0;
    let btn = SW.Button('Click Me', () => {
        counter++;
        console.log('[PTSEx] slate button clicked', counter);
        Core.Notify('PTSEx', 'Slate button clicked ' + counter, 0);
    });

    let check = SW.CheckBox(false, (state) => {
        console.log('[PTSEx] checkbox state', state); // 0=Unchecked 1=Checked 2=Undetermined
    });

    let edit = SW.EditableTextBox('type here', (text) => {
        console.log('[PTSEx] text changed:', text);
    });

    return SW.VerticalBox()
        .Add(header, 8, 0, 0, 1)
        .Add(SW.Border().Add(btn, 4).SetColor(new UE.LinearColor(0.15, 0.15, 0.15, 1)), 8, 0, 0, 1)
        .Add(check, 8, 0, 0, 1)
        .Add(edit, 8, 0, 0, 1)
        .Add(SW.Spacer(0, 20), 0, 1, 0, 0); // fill the rest
}

// Native Slate window
Menus.AddEntry(subMenu, 'Scripts', Menus.InitMenuEntry('PTSExSlateWindow', 'Open Native Slate Window', 'Uses raw Slate widgets from TS', () => {
    const id = Windows.OpenSlate(buildSlatePanel(), 'PTSEx Native Slate Window', 480, 360);
    console.log('[PTSEx] slate window opened, id', id);
}));

// Native Slate tab
Tabs.RegisterSlate('PTSExSlateTab', 'PTSEx Slate Tab', () => buildSlatePanel(), 'EditorStyle', 'LevelEditor.WorldProperties');
Menus.AddEntry(subMenu, 'Scripts', Menus.InitMenuEntry('PTSExSlateTabOpen', 'Open Native Slate Tab', 'Opens the raw Slate nomad tab', () => {
    Tabs.Open('PTSExSlateTab');
}));

// ---------------------------------------------------------------------------
// World hooks
// ---------------------------------------------------------------------------
Core.OnMapChanged((worldName, changeType) => {
    if (changeType === 0) { // EMapChangeType::TearDownWorld
        console.log('[PTSEx] world torn down:', worldName);
    }
});

console.log('[PTSEx] Main.js loaded OK');
