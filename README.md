# PuerTSExtended

Scriptable Unreal editor tooling on [PuerTS](https://github.com/Tencent/puerts). Write editor
tools — menus, toolbar buttons, context menus, console commands, windows, workspace tabs,
detail-panel rows, notifications — in TypeScript/JavaScript, hot-reloaded live, **zero C++
iterations**. The whole C++ surface is ~4 small files; everything else is script.

Built to be **swappable**: drop the plugin into any PuerTS project, ship tools as scripts
inside the plugin or inside their own plugins, and they just work.

---

## What you get (the TS API)

| Namespace | What it does |
| --- | --- |
| `cpp.PTS_Core` | env lifecycle (`SetOnJsEnvPreReload`/`SetOnJsEnvCleanup`/`SetEval`/`Reload`), `Notify` toasts, `MessageBox`, `OnMapChanged`, path/version getters |
| `cpp.PTS_Menus` | `InitMenuEntry` / `InitToolBarButton` / `InitComboButton` with TS click handlers, `AddEntry`, content-browser folder + asset context menus |
| `cpp.PTS_Console` | `AddCommand` / `RemoveCommand` — console commands backed by TS functions |
| `cpp.PTS_Windows` | editor sub-windows hosting **TS-built UMG widgets** (reuses your runtime UMG skills) |
| `cpp.PTS_Tabs` | nomad workspace tabs hosting TS UMG widgets |
| `cpp.PTS_Details` | detail-panel customizations rendered from TS UMG rows |
| `cpp.FPTSExSlateWidget` | **native Slate DSL from TS** — `VerticalBox()`/`HorizontalBox()`/`ScrollBox()`/`Border()`/`Button()`/`CheckBox()`/`TextBlock()`/`EditableTextBox()`/`Image()`/`Spacer()`/`Splitter()` + chainable `.Add()`/`.SetText()`/`.SetFontSize()`/`.SetColor()`/`.SetPadding()`/`.SetVisibility()`/`.Clear()`. Host it via `PTS_Windows.OpenSlate()`, `PTS_Tabs.RegisterSlate()`, `PTS_Details.AddSlate()` |

Plus `UE.*` reflection — the entire engine/editor API is callable from the env (e.g.
`UE.ToolMenus.Get().FindMenu(...)`, asset APIs, editor subsystems).

## Quick start

1. Enable the plugin (it's in `Plugins/` — auto-loads in the editor).
2. Editor starts → plugin boots its own PuerTS env → loads
   `Content/PuertsExtended/Main.js` (bundled starter: menu, toolbar, combo,
   context menus, console command, window, tab).
3. Try `PTSEx.Hello a b c` in the editor console, or look for **PTSEx** in the
   main menu / toolbar / content-browser right-click.
4. Edit `Main.js` — it **hot-reloads on save**. Full restart: `PuerTSEx.Reload`.

## Script layout & swappability

Module search order for `require(...)`:

1. `plugin://PluginName/Sub/Path` — resolved against `<PluginName>/Content/`
   (prefers `<Plugin>/Content/PuertsExtended/Sub/Path`). **This is how reusable
   tool plugins work**: ship `Content/PuertsExtended/Main.js` inside a tool
   plugin, pull it in with `require('plugin://MyTool')` or
   `require('plugin://MyTool/SubModule')`.
2. Relative to the requiring module (default behavior).
3. `<Project>/Content/<ScriptRoot>/` (default `PuertsExtended`) — project tools win.
4. `<PuerTSExtended>/Content/<ScriptRoot>/` — bundled defaults, shadowed by (3).
5. `<Project>/Content/JavaScript/` — puerts runtime infra + shared modules.

Configure via **Project Settings > Plugins > PuerTSExtended** (or
`Config/DefaultEditor.ini` `[PuerTSExtended]`): `ScriptRoot`, `bHotReload`,
`DebugPort` (V8 inspector; 8080 is taken by the game env — use e.g. 8090),
`ExtraScriptRoots`.

Console commands:

| Command | Effect |
| --- | --- |
| `PuerTSEx.Reload` | full env restart (always works — no chicken-and-egg) |
| `PuerTSEx.Eval <code>` | eval a string in the env (needs `PTS_Core.SetEval`) |
| `PuerTSEx.Status` | env diagnostics |
| `PuerTSEx.DebugPort <n>` | set inspector port + reload |
| `PuerTSEx.ScriptRoot <dir>` | add an extra script root + reload |

## Writing editor UMG widgets (IMPORTANT)

Editor windows/tabs/details host `UUserWidget`s you build in TS. The same PuerTS
rules that apply to your runtime UI apply here:

- PuerTS calls `Constructor()` (not the JS constructor) — initialize fields there.
- **Never** create UObjects in `Constructor()` (runs inside native construction).
- The tree must be built **before** the widget enters any slate: `TakeWidget()`
  bakes the slate root at call time, so a tree built later renders as an empty
  `SSpacer`. Always call `widget.EnsureTreeBuilt()` at the creation site, right
  after `new MyWidget()` and before passing it to `PTS_Windows.Open` /
  returning it from a tab spawner.
- No static class fields on classes extending UE types (module-scope `let/const`).
- Generated BPs for TS widget classes in this env land in
  `Content/Blueprints/TypeScript/PuertsExtended/` — if you hit a stale-BP
  recursion on bind, delete the stale `.uasset` + `ts_file_versions_info.json`.

## Detail customizations

```js
cpp.PTS_Details.Add(UE.Actor.StaticClass(), (objects) => {
    const w = new MyRowWidget();
    w.EnsureTreeBuilt();
    w.SetActorName(objects[0].GetName());
    return [{ Label: 'My Row', Widget: w }];
});
```

The callback receives the selected objects and returns row descriptors; the
plugin renders them into the details panel (widgets pinned for the panel's
lifetime). `PTS_Details.Remove(classObj)` to unregister.

## Writing a reusable tool plugin

```js
// In your tool plugin's Content/PuertsExtended/Main.js:
const UE = require('ue');
const Core = cpp.PTS_Core;
const Menus = cpp.PTS_Menus;

Menus.AddAssetContextEntry('MyTool.Export', 'Export with MyTool', '...', (ctx) => { ... });
Core.Notify('MyTool', 'loaded', 1);
```

```js
// Project script pulls it in:
require('plugin://MyTool'); // runs the tool plugin's entry script
```

The tool plugin only needs `Puerts` + `PuerTSExtended` as plugin dependencies;
its scripts travel with it, so enabling/disabling the plugin swaps the tool in
and out.

## C++ for C++ devs

- `FPuerTSExtendedModule` (public header) — `RestartEnv()`, `EvalScript()`,
  `RegisterTsConsoleCommand()`, `IsEnvRunning()`.
- Binding registry: `Private/PTSExBindings.cpp` — `puerts::DefineClass` for each
  `cpp.PTS_*` namespace, `FToolMenuEntry`/`FSlateIcon`/`FContentBrowserItem` glue.
- Slate hosting (windows/tabs/details, widget pinning, teardown):
  `Private/PTSExSlate.cpp`.
- Env bootstrap, hot reload watcher, tick, console commands:
  `Private/PuerTSExtended.cpp`. Module loader (plugin:// scheme):
  `Private/PTSExModuleLoader.cpp`. Safe TS-function invocation (isolate/context
  scoping + exception logging): `Private/PTSExInvoke.cpp`.

The env is **separate** from the game env (`Content/JavaScript`): it does not
share module scope or generated classes, but does share the `ue`/`puerts`
builtins and the engine itself. Typescript projects can reference
`Typings/PuerTSExtended.d.ts` for intellisense.

---

## Performance & Memory Profile

- **Memory footprint:** A fresh V8 isolate uses roughly **5–15 MB of RAM**. In a heavy UE5 editor session, this is negligible.
- **CPU impact:** The env runs strictly on the Game Thread. Ticking is a simple call to V8's `IdleNotificationDeadline(0.0)`. When idle, this execution takes **less than 1 microsecond**.
- **Reactive design:** JS code only executes when clicked, called via a console command, or during details panel rebuilding. It spends 99.9% of its time completely asleep.
- **GC Isolation:** The garbage collection cycle of this env is independent of your game's runtime env. A large GC sweep in the game won't trigger sweeps or frame hitches in your editor tools.

