# Declarative GUI plugin

A pure 2D window can use the `declarative_gui` factory without adding a
window-specific C++ plugin.

The plugin contract, registry, manifest loader, and declarative plugin are
platform independent. `IGuiPlugin` receives an opaque graphics context only
when a reusable custom control needs platform rendering. The macOS SDL host
and the Windows Direct3D host therefore share plugin discovery, data-provider
lifecycle, ticking, actions, and window metadata.

`GuiWindowSessionController` is the platform-independent runtime for one
plugin window. It binds the parsed window, owns its data snapshot, list models,
selection and scroll state, input router, behavior/action bridge, visibility
condition, and tick scheduler. A platform host supplies only a graphics
context plus callbacks for resource refresh and native-window visibility, then
renders the controller's resolved widgets. The SDL macOS host is already an
adapter around this controller; a Direct3D 9 host can reuse the same session
without duplicating plugin, list, condition, or event logic.

## Indexed maps

An indexed map is a built-in declarative control. Its renderer, color updates,
hover highlight, hit testing, and click item ID are owned by the GUI host rather
than a window-specific plugin.

Register the reusable assets and style in a `.gfx` file:

```text
indexedMapResourceType = {
	name = "GFX_example_map"
	texturefile = "gfx\\example\\map.bmp"
	indexfile = "gfx\\example\\map_ids.bin"
	sourceDefinitionFile = "map\\definition.csv"
	sourceProvinceFile = "map\\provinces.bmp"
	sourceGroupFile = "map\\groups.txt"
	cropPadding = 16
	flipVertical = yes
	sourceItem = { id = 1 name = "first_group" }
	sourceItem = { id = 2 name = "second_group" }
	boundaryColor = { 0.02 0.06 0.07 0.92 }
	hoverColor = { 0.90 0.94 1.0 0.47 }
	boundaryWidth = 1
	drawBoundaries = yes
	colorStop = { minimum = 0 color = { 0.31 0.51 0.31 0.63 } }
	colorStop = { minimum = 20 color = { 0.94 0.82 0.24 0.75 } }
	colorStop = { minimum = 60 color = { 0.90 0.39 0.39 0.78 } }
}
```

The same resource block is also the build specification. Generate its texture
and `IDX1` item-index file from the project root with:

```bash
cmake -S overlay -B overlay/build
cmake --build overlay/build --target gui_indexed_map_make
./overlay/build/gui_indexed_map_make . GFX_example_map
```

The generator contains no map-specific names. `sourceItem` assigns stable
nonzero IDs to groups found in `sourceGroupFile`; `texturefile` and `indexfile`
select the outputs consumed by the runtime. Any new indexed map therefore needs
only a `.gfx` resource block and its source map files.

Place the control in any `.gui` or injected-only `.sgui` window:

```text
indexedMapType = {
	name = "example_map"
	mapResource = "GFX_example_map"
	valueSource = "regions.{id}.value"
	onClick = "select_map_item"
	visibleWhen = "state.visible"
	position = { x = 100 y = 100 }
	size = { x = 800 y = 500 }
}
```

`{id}` is replaced by each nonzero ID stored in the index file. The resulting
keys are read from `GuiDataRegistry`; only IDs whose resulting color changes are
uploaded again. A click forwards the picked ID through the common selected-item
field, so behavior scripts can read it as `$list_item_id`.

```text
guiPlugin = {
	id = "example"
	displayName = "Example"
	factory = "declarative_gui"
	startup = no
	visibleWhen = "state.visible"
	options = {
		window = "example_window"
		title = "Example Window"
		data_provider = "file"
		data = "script_gui/data/example.txt"
		watch = yes
		tick_interval = 200
	}
}
```

`data_provider` selects a factory from the application-wide
`GuiDataProviderRegistry`. The built-in `file` provider resolves `data` or
`data_path` relative to the game root and watches its modification time.
`startup = yes` opens the window when the host starts; `startup = no` still
registers and initializes the plugin in a closed state so `open_window`,
`show_window`, or `toggle_window` can open it later.

The in-process host supports the same providers as the offline host. Any
option prefixed with `inprocess_` overrides its unprefixed option only inside
the injected DLL. This keeps an offline sequence and the live Lua source in
one manifest without tying data-provider choice to `startup`:

```text
options = {
	data_provider = "sequence"
	data = "snapshots/example"
	inprocess_data_provider = "bridge"
	inprocess_channel = "lua"
	inprocess_bridge_name = "example"
}
```

The generic `sequence` provider is the Mac offline fallback. It merges an
optional common file with one sorted frame file at a time:

```text
options = {
	data_provider = "sequence"
	data = "snapshots/example"
	base_data = "script_gui/data/example_common.txt"
	frame_interval = 1200
	loop = yes
}
```

It is not tied to a window or data schema. Replacing `sequence` with a live
provider therefore does not change the `.gui` layout, behaviors, sprites, or
fonts.

A live data source uses the bridge provider and selects a transport channel:

```text
options = {
	data_provider = "bridge"
	channel = "memory"
	max_updates_per_tick = 64
}
```

The Lua transport uses a named endpoint shared by the GUI provider and the
game-thread Lua binding:

```text
options = {
	data_provider = "bridge"
	channel = "lua"
	bridge_name = "example"
}
```

Lua publishes data and consumes actions through `script/gui_data_bridge.lua`:

```lua
local GuiDataBridge = require('gui_data_bridge')

GuiDataBridge.PublishSnapshot("example", {
	values = { ["state.visible"] = true, counter = 1 },
	lists = {}
})

GuiDataBridge.PublishDelta("example", {
	values = { counter = 2 }
})

GuiDataBridge.DispatchActions("example", 64)
```

The Windows Lua binding exposes
`ScriptedGuiNative.PublishUpdate(channelName, updateTable)` and
`ScriptedGuiNative.TryPopAction(channelName)`. The generic C++ endpoint,
revision validation, queues, data provider, action payload, and Lua callback
dispatch are platform independent. A Lua state reserves the channel before it
publishes. A higher-priority state may always preempt the current publisher; an
equal-priority fallback may do so after ten seconds of publisher silence, while
a lower-priority state cannot displace a stale higher-priority owner. Competing
states cannot publish or consume the owner's actions.

An `onClick`, `onPress`, or hover action may directly contain the registered
Lua action name. A separate behavior block is optional and is only needed for
phase restrictions, `enabledWhen`, fallback operations, or extra parameters.

Bridge channels publish a revisioned full snapshot followed by deltas. Each
delta declares its `baseRevision`; a gap is rejected atomically so the current
GUI registry remains valid until the producer sends a new full snapshot.
Actions travel in the opposite direction through the same channel. The
built-in bounded `memory` channel is intended for offline development and will
be replaced by a Lua/Hook channel on Windows.

Bridge update rules:

- A full snapshot uses `fullSnapshot = true`, `baseRevision = 0`, and a
  positive monotonically increasing `revision`; it replaces all prior data.
- A delta uses the currently applied revision as `baseRevision` and may set or
  remove scalar values and lists.
- Duplicate or stale revisions are ignored. Missing bases, invalid values, and
  duplicate list item IDs reject the entire polled batch without partial data.
- GUI actions preserve action/function names, phase, window/widget/list IDs,
  selected item ID, mouse coordinates, and arbitrary string parameters.

The transport wire format is little-endian and framed with a 16-byte header:
four-byte `GDBR` magic, protocol version, message type, payload length, and a
CRC32 of the payload. Revision and integer values remain exact 64-bit values.
Map fields are encoded in sorted order so identical messages produce identical
bytes. The decoder enforces frame, string, value, list, item, and action
parameter limits before allocating variable-size data.

Provider lifecycle:

- `Initialize` resolves external resources and publishes the first registry.
- `Registry` returns the current data view consumed by GUI layout.
- `Tick` reports `Changed`, `Unchanged`, or `Failed`; only `Changed` refreshes
  the window, while a failed reload preserves the previous registry.
- `HandleAction` receives the same behavior context used by file fallback
  actions and future Lua callbacks.
- `Shutdown` releases provider-owned resources before the window is destroyed.

The data file supports inferred scalar values, explicitly typed `value`
blocks, and `list` blocks.

```text
guiData = {
	state.visible = yes
	progress = 0.5
	title = "Example"
	value = { name = "code" type = "string" value = "007" }
	list = {
		name = "tasks"
		item = { id = 1 text = "Task One" }
	}
}
```

List items may publish arbitrary typed fields in addition to `id` and `text`.
An item template reads them with `item.<field>`, so portraits, role labels, and
other per-row resources remain data-driven:

```text
item = {
	id = 1
	textkey = "GUI_MILITARY_LEADER_NAME"
	portrait = "GFX_leader_portrait"
	role = "military"
	buttonsprite = "GFX_faction_leader_button"
	enabledwhen = "regions.{selectedregion.id}.militaryeligible"
}
```

`textBoxType` supports `localizationKey`, while dynamic localized keys can be
provided through `textSource`. Localization CSV files are loaded from the
project `localisation` directory. Set `wrap = yes` and `lineSpacing` for
multi-line descriptions.

A list template can use `spriteSource = "item.buttonsprite"` and
`pressedSpriteSource = "item.buttonsprite"` to select row art from data.
Set `disableItemsInList` on the list box to another list name to disable and
gray rows whose stable item IDs already occur in that list. The generic input
router excludes disabled rows, and fallback actions also evaluate each item's
`enabled` and `enabledwhen` fields before copying it.

Lists with typed assignment slots can additionally set
`disableMatchingField`, `disableFilterField`, and
`disableFilterValueSource`. For example, matching `leadertype` while filtering
`regionid` by `selectedregion.id` disables every military candidate after that
Region's military slot is occupied, while leaving its administrative slot
available. A `copy_list_item` fallback can enforce the same invariant with
`reject_matching_fields = "regionid,leadertype"`.

## Map markers

`markerLayerType` renders an arbitrary data list over an `indexedMapType`.
The target Region anchor comes from the indexed-map ID resource, while marker
position is stored as normalized map coordinates. No plugin-specific C++ is
required.

```text
markerLayerType = {
	name = "leader_markers"
	dataSource = "assigned_leaders"
	mapWidget = "region_map"
	frameSprite = "GFX_leader_frame"
	portraitSource = "item.portrait"
	regionSource = "item.regionid"
	xSource = "item.x"
	ySource = "item.y"
	nameSource = "item.namekey"
	descriptionSource = "item.descriptionkey"
	stackSource = "item.regionid"
	stackOrderSource = "item.assignmentorder"
	stackDirection = "vertical"
	stackSpacing = 4
	markerSize = { x = 68 y = 84 }
	portraitPosition = { x = 2 y = 2 }
	portraitSize = { x = 64 y = 80 }
	tooltipSize = { x = 300 y = 150 }
	tooltipPlacement = "right"
	avoidTooltipOverlap = yes
	tooltipColor = { 0 0 0 0.76 }
	lineColor = { 0.18 0.60 1 0.92 }
	draggable = yes
	localizeTooltip = yes
	onDragEnd = "move_leader"
	markerActionSprite = "GFX_step_down"
	markerActionPosition = { x = -80 y = 0 }
	markerActionSize = { x = 80 y = 15 }
	markerActionLocalizationKey = "GUI_STEP_DOWN"
	onMarkerAction = "step_down_leader"
}
```

Hovering a marker displays its tooltip; clicking pins or unpins it. The
tooltip defaults to the marker's left edge; `tooltipPlacement = "right"`
places it directly against the right edge instead. A selected marker may expose
one declarative action button through the `markerAction*` fields. Drag actions carry
the source item fields plus `normalizedx`, `normalizedy`, `markerx`, and
`markery`, allowing Lua to persist the marker position. Tooltip textures and
marker state are released when their source list items disappear.

Markers sharing the same `stackSource` value are ordered by
`stackOrderSource`. Vertical stacks place later items below earlier items;
horizontal stacks place them to the right. Dragging any stacked marker moves
the group's shared base position. `avoidTooltipOverlap = yes` searches along
the preferred tooltip side before falling back to the opposite side, preventing
the tooltip mask from covering another marker where map space permits.

Data paths may contain bindings to other registry values. For example,
`regions.{selectedregion.id}.name` first resolves `selectedregion.id`, then
reads the resulting Region key. This lets one information panel display any
item selected from a list or indexed map without window-specific C++.

Behavior fallback operations supported by the declarative store are
`set_value`, `set_text`, `toggle_value`, `add_value`, and `select_item`.
Parameters such as `target`, `value`, `type`, and `amount` are forwarded through
the common action bridge. `select_item` writes the clicked list or indexed-map
ID to `target`; `persist = yes` keeps that value while a sequence provider
loads its next frame. Event placeholders include `$list_item_id`, `$list_index`,
`$mouse_x`, `$mouse_y`, `$widget`, and `$list`.

Application-level fallback operations can target another plugin or window with
the `window` parameter. Supported operations are `open_window`, `show_window`,
`hide_window`, `toggle_window`, `reset_window_visibility`, `close_window`,
`send_action`, `set_window_value`, `toggle_window_value`, `add_window_value`,
and `reload_window_data`.

## Global Z order

Every drawable control is converted to a render command. List templates are
instantiated as ordinary button, image, and text nodes instead of being drawn
through a list-specific bypass. Commands from indexed maps, marker layers,
custom widgets, images, buttons, scrollbars, color boxes, progress bars, text,
and the window frame are sorted together by inherited `zOrder`. Lexical
declaration order breaks ties. Child `zOrder` values are added to the parent
value. `frameZOrder` controls the frame offset and defaults to `1000000` to
preserve the usual topmost border. Set `clipChildren = yes` on a container to
clip both rendering and hit testing to its rectangle; list items are always
clipped to their list viewport.

```text
windowType = {
	name = "example_window"
	zOrder = 0
	frameZOrder = 1000
	iconType = { name = "background" zOrder = 0 }
	textBoxType = { name = "title" zOrder = 20 }
}
```

Control `zOrder` remains local to one declarative window. Plugin manifests
provide the application-wide window band with `windowZOrder`; pointer focus
reorders windows only inside the same band. `modal = yes` renders that window
above non-modal windows and restricts Scripted GUI input to the topmost active
modal window.

```text
guiPlugin = {
	id = "example"
	startup = yes
	windowZOrder = 20
	modal = no
}
```

The global window manager keeps rendering and input order in one registry.
Clicking or dragging a visible window focuses it, hidden or closed windows are
removed from hit testing, and a closed window continues polling its provider so
an application action or a new game session can reopen it.

## Lua scheduling and ownership

`script/scripted_gui_runtime.lua` is the only AI callback entry point for Lua
GUI plugins. `script/scripted_gui_plugins.lua` registers modules and channels.
The runtime reserves each native channel by priority, pumps actions separately
from data refreshes, isolates plugin failures with a cooldown, and calls these
optional module callbacks:

- `ShouldTick(context)` and `PumpActions(context, budget)`
- `ShouldRefresh(context)` and `BuildUpdate(context)`
- `PublishUpdate(update, context)` and `OnUpdatePublished(update, context)`
- `OnPublisherAcquired(context)` and `OnPublisherLost(context)`
- `RestoreState(context)`, `PersistState(context)`, and `Shutdown()`

The player-country Lua state uses the higher channel priority. A fallback AI
state may publish while it is unavailable, but native ownership prevents two
states from scanning and publishing the same GUI simultaneously. Publisher
takeover resets local revisions while the native bridge preserves one global,
monotonic channel revision.

## Session and save persistence

A live snapshot may publish two independent identities:

- `state.sessionid` identifies one logical gameplay UI session. It remains
  stable across equivalent Lua publisher handoffs and changes when the Lua
  adapter detects that a different or older save state has been loaded. A
  change clears hover, press, list, visibility override, and other transient
  window state.
- `state.persistencekey` identifies the game/save profile. Values changed by a
  behavior with `persist = yes`, plus list selection and scroll state, are
  restored only for that profile.

The C++ sidecar uses a versioned binary format, bounded collection sizes,
profile verification, corruption isolation, and temporary-file replacement.
On Windows it defaults to
`%LOCALAPPDATA%\HOI3 Scripted GUI\state`; set
`SCRIPTED_GUI_STATE_ROOT` to override it for tests or portable installs.

Gameplay state must still live in the HOI3 save. The generic
`script/scripted_gui_persistence.lua` adapter reads `CVariables` and posts
`CSetVariableCommand` updates. The China war-map adapter uses it for its stable
profile token, selected Region, panel state, leader assignments, assignment
order, normalized marker positions, and a commit revision written after the
payload. Every publisher targets the player country's variables, including a
fallback AI Lua state. A profile or revision rollback therefore detects an
in-process save load even when HOI3 keeps the same Lua state alive. The native
sidecar stores presentation state only and never replaces save-game variables.

Run the Mac host with `--resource-stats` to print approximate per-window host
texture bytes, plugin texture bytes, CPU buffer bytes, and the size of the
application-wide shared font cache.

## War-map preview modes

The default China war-map manifest reads the single persistent preview frame
`snapshots/china_war_gui/01_static.txt`. It contains the agreed Region control
percentages and never advances to the hidden peace state:

```bash
./overlay/build/scripted_gui_host_macos --root .
```

The original multi-frame communication regression remains available through a
runtime data override:

```bash
./overlay/build/scripted_gui_host_macos \
	--root . \
	--data snapshots/china_war_gui
```
