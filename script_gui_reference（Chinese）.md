# HOI3 Scripted GUI 系统现状与 SGUI/SGFX 语法手册

本文档以当前仓库源码为准，覆盖：

- Scripted GUI 解释器的核心源码分类与职责；
- 当前核心能力的完成度和验收边界；
- `.sgui`、`.sgfx` 已实现的全部控件、资源和字段；
- 当前文件尚未使用、但解释器已经实现的语句；
- 已被解析但暂时没有运行时效果，以及当前配置中存在但会被忽略的语句。

## 1. 状态标记

| 标记 | 含义 |
|---|---|
| **现用** | 已在 `interface/*.sgui` 或 `interface/*.sgfx` 中实际使用，并有对应运行时实现。 |
| **已实现未用** | 当前 `.sgui/.sgfx` 未使用，但解析、布局、渲染或事件逻辑已经实现。 |
| **部分实现** | 仅对特定控件、特定平台或特定取值生效。 |
| **仅解析** | 字段会被读取并保存到 C++ 定义对象，但当前运行时不消费它。 |
| **忽略** | 通用语法解析器允许它存在，但解释器没有读取该字段，因此没有任何效果。 |

> 重要：解析器允许未知字段通过语法解析。一个字段“没有报错”不等于它“已经生效”。应以本文档和源码中的字段注册逻辑为准。

## 2. 核心源码分类

### 2.1 配置解析与定义模型

- `overlay/src/gui_interpreter.h/.cpp`：通用词法/语法解析器；注册 Sprite、进度条、索引地图资源；构建窗口和控件树；解析相对坐标、条件、事件、列表模板、Z 顺序和裁剪。
- `overlay/src/gui_plugin_manifest.h/.cpp`：解析 `interface/gui_plugins` 中的插件清单，将窗口、数据源、启动方式、可见条件和窗口层级注册为可启动插件。
- `overlay/src/gui_behavior.h/.cpp`：解析 `script_gui/*.txt` 行为文件，将 `.sgui` 中的动作名映射到 Lua 函数、触发阶段、条件、参数和离线回退操作。
- `overlay/src/gui_declarative_data.h/.cpp`：解析静态或离线数据文件，生成标量与列表数据。
- `overlay/src/gui_localization.h/.cpp`：读取本地化文本，并为 `localizationKey`、`localized` 和 Marker 提示提供翻译。

### 2.2 插件、应用与窗口生命周期

- `overlay/src/gui_plugin.h`、`gui_plugin_registry.h/.cpp`：定义插件接口、插件工厂和插件描述符。
- `overlay/src/gui_builtin_plugins.h/.cpp`：注册通用 `declarative_gui` 插件工厂。
- `overlay/src/declarative_gui_plugin.h/.cpp`：通用声明式插件；把任意窗口与通用数据提供器连接起来，不包含战争地图专用业务逻辑。
- `overlay/src/gui_inprocess_application.h/.cpp`：加载所有界面、资源、行为和插件清单；执行基础配置校验；隔离配置错误；创建有效插件实例。
- `overlay/src/gui_window_session.h/.cpp`：管理单个 GUI 会话的绑定、刷新、开关、列表实例、输入、动作、数据、临时状态和持久化状态。
- `overlay/src/gui_window_manager.h/.cpp`：管理多窗口打开状态、可见状态、窗口 Z 顺序和模态窗口。
- `overlay/src/gui_application_bus.h/.cpp`：执行跨窗口的打开、关闭、显隐和动作转发。
- `overlay/src/gui_tick.h/.cpp`：按插件配置的刷新周期调度数据更新。

### 2.3 布局、列表、输入与事件

- `overlay/src/gui_runtime.h/.cpp`：条件环境、通用事件路由、拖动参数、列表布局、滚动状态和命中测试。
- `overlay/src/gui_render_queue.h/.cpp`：将控件树转换为统一渲染命令，并按全局 Z 顺序稳定排序。
- `overlay/src/gui_custom_widget.h/.cpp`：为无法由内置控件表达的特殊控件提供 C++ 扩展注册点。
- `overlay/src/gui_list_model.h`：通用动态列表和列表项数据结构。

### 2.4 数据驱动、Lua 桥与发布者

- `overlay/src/gui_data.h/.cpp`：通用 `GuiDataRegistry`；保存布尔、整数、浮点、字符串和列表；解析数据路径、条件表达式和 `{变量}` 插值。
- `overlay/src/gui_data_provider.h/.cpp`：通用数据提供器接口和注册表。
- `overlay/src/gui_file_data_provider.h/.cpp`：从声明式数据文件加载快照。
- `overlay/src/gui_sequence_data_provider.h/.cpp`：按顺序播放离线快照，主要用于原型和回退测试。
- `overlay/src/gui_data_bridge.h/.cpp`：把外部发布者提供的数据快照导入统一数据注册表。
- `overlay/src/gui_lua_bridge.h/.cpp`：维护 Lua 数据频道、更新序号、动作队列、会话边界和游戏生命周期快照。
- `overlay/src/gui_lua_native_binding.h/.cpp`：在 HOI3 的 Lua 5.1 State 中注册原生数据发布与动作读取接口。
- `overlay/src/gui_lua51_hook.h/.cpp`：捕获游戏创建的 Lua State，安装和卸载原生绑定，不再主动调用危险的 HOI3 Lua 接口。
- `script/scripted_gui_runtime.lua`：通用 Lua 插件调度器、发布者所有权、刷新策略、错误冷却和动作泵。
- `script/scripted_gui_plugins.lua`：注册需要从游戏读取实时数据的 Lua 插件。
- `script/gui_data_bridge.lua`、`script/gui_action_bridge.lua`：Lua 侧数据发布与动作消费接口。

### 2.5 渲染、资源和特殊可视化

- `overlay/src/gui_host_d3d9.h/.cpp`：Windows 游戏内宿主；创建会话、缩放设计坐标、绘制命令、路由鼠标输入并控制底层游戏点击穿透。
- `overlay/src/gui_d3d9_hook.h/.cpp`：挂接 Direct3D 9 Present/Reset 生命周期。
- `overlay/src/gui_texture_loader_d3d9.h/.cpp`：加载和缓存 D3D9 图片资源。
- `overlay/src/gui_text_renderer_d3d9.h/.cpp`：递归加载 `font` 目录中的 `.ttf/.otf`，使用 GDI+ 生成文字纹理并缓存。
- `overlay/src/gui_indexed_map_core.h/.cpp`：平台无关的 Region ID 图、着色、边界和命中核心。
- `overlay/src/gui_indexed_map_d3d9.h/.cpp`：Windows 索引地图渲染、悬停和点击 Region。
- `overlay/src/gui_marker_layer_d3d9.h/.cpp`：Windows 地图 Marker、头像、连线、堆叠、提示、拖动和附属动作。
- `overlay/src/gui_indexed_map_make.cpp`：根据地图源文件离线生成底图和 Region ID 二进制图。
- `overlay/src/gui_host_macos.*`、`gui_*_macos.*`、`scripted_gui_host_macos.cpp`：macOS 原型和离线验证宿主，不进入 Windows 注入 DLL。
- `overlay/src/gui_control_renderer.*`：SDL/macOS 原型使用的通用 2D 控件绘制辅助。

### 2.6 注入、游戏生命周期与持久化

- `overlay/src/scripted_gui_overlay_dll.cpp`、`scripted_gui_overlay.def`：注入 DLL 入口和导出接口。
- `overlay/src/scripted_gui_injector.cpp`：把 DLL 注入指定 HOI3 进程。
- `overlay/src/gui_hoi3_lifecycle.h/.cpp`：安全识别主菜单与战局状态，防止主菜单继续显示或操作游戏内 GUI。
- `overlay/src/gui_persistence.h/.cpp`：按插件、会话和存档边界保存/恢复 GUI 状态。
- `overlay/src/*_probe*.cpp`：离线、集成和回归测试程序，不是最终 DLL 的业务模块。

## 3. 当前完成度结论

### 3.1 已形成闭环的核心能力

- `.sgui/.sgfx` 解析、资源注册和插件清单加载；
- 窗口、图片、文字、按钮、色块、进度条、列表、滚动条；
- 父子相对坐标、显式 `parent`、统一 Z 顺序、父级裁剪和列表裁剪；
- 窗口拖动、控件拖动、悬停、按下、释放、点击和拖动事件；
- 通用列表模板实例化、网格布局、半圆/极坐标布局和滚动条自动绑定；
- `visibleWhen`、`enabledWhen`、数据路径插值和动态文字/图片/数值绑定；
- Lua 数据发布、Lua 动作回调、离线回退、刷新调度和发布者稳定性；
- 多插件、多窗口、窗口打开/关闭、游戏内/主菜单生命周期隔离；
- 持久化状态、会话切换和存档回滚恢复；
- D3D9 游戏内绘制、自适应缩放、输入区域收缩和底层游戏点击穿透；
- 通用索引地图和 Marker 图层。中国战争地图是第一个完整实例，议会半圆席位图是第二个纯声明式实例。

### 3.2 “核心功能是否完全补齐”的准确答案

- **若目标定义为 V1：支持由 Lua 数据驱动的通用 2D 游戏内 GUI，核心闭环已经补齐。**
- **若目标定义为完整复刻 HOI3/HOI4 全部 GUI 能力，尚未完全补齐。**

仍存在的主要缺口：

1. 尚无严格字段 Schema；拼错但语法合法的字段可能被静默忽略。
2. Windows D3D9 图片仍固定拉伸，`scaleMode` 只在 macOS 原型生效。
3. 通用 Tooltip 尚未实现；当前完整 Tooltip 只属于 `markerLayerType`。
4. `fullScreen`、`positionType`、`orientation` 等原版字段尚未接入当前布局系统。
5. Sprite 多帧、HOI3 Effect、九宫格、旋转、通用动画尚未实现。
6. 进度条贴图字段虽可解析，但当前 D3D9 进度条实际使用纯色绘制。
7. 通用 3D 模型控件尚未实现。
8. `customWidgetType` 仍要求 C++ 注册对应 Handler；它是扩展点，不是纯配置控件。

### 3.3 “只改 interface/font/gfx 就能添加新 GUI”的验收结论

严格按字面要求，**目前还没有完全达到**。

- 纯静态或文件数据驱动的 2D GUI：不需要新增 C++，但除 `.sgui/.sgfx` 和素材外，至少还要在 `interface/gui_plugins` 添加插件清单；通常还需要 `script_gui/data` 数据文件。
- 读取游戏实时状态、修改游戏状态或响应复杂动作的 GUI：不需要新增 C++ 的前提是现有 Lua/HOI3 接口足够，但仍需新增 Lua 数据模块、动作函数，并在 `script/scripted_gui_plugins.lua` 注册。
- 需要新控件语义、3D 模型、尚未暴露的游戏内存数据或新 Hook：仍需修改 C++。

因此当前已经达到的实际验收目标是：

> 对现有内置控件能够表达的 2D GUI，可以通过 `.sgui + .sgfx + 插件清单 + 数据/行为 Lua + 素材/字体` 完成，不再为每个 GUI 新写一个专用 C++ 程序。

## 4. 基础语法

```text
# 注释方式一
// 注释方式二

property = value
property = "带空格的字符串"
property = 123
property = -1.5
property = yes
property = { child = value }
```

- 文件扩展名：解释器会读取 `.gui`、`.gfx`、`.sgui`、`.sgfx`。
- 字段名和控件类型名：匹配时不区分大小写。
- 标识符可包含字母、数字、下划线、连字符和点；包含空格、反斜杠、斜杠或其他字符时应加双引号。
- 字符串内反斜杠是转义符；Windows 路径建议写成 `"gfx\\war_map\\image.png"`。
- 布尔真值：`yes`、`true`、`1`；布尔假值：`no`、`false`、`0`。
- `position`、`size` 等二维块必须使用 `x`、`y`，不支持 `{ 10 20 }` 形式。
- RGB/RGBA 颜色同时支持位置形式 `{ 1.0 0.5 0.2 1.0 }` 和命名形式 `{ r = 1.0 g = 0.5 b = 0.2 a = 1.0 }`。
- 未知字段不会自动报错；它们会被语法树保留，但不会产生运行时效果。

## 5. SGUI 控件类型

| 状态 | 控件语句 | 作用 |
|---|---|---|
| 现用 | `guiTypes = { ... }` | 习惯性的布局容器；解释器递归查找其中的窗口，本身没有运行时对象。 |
| 现用 | `windowType = { ... }` | 窗口或窗口内的子容器；支持边框、拖动、父子布局、条件、裁剪和 Z 顺序。 |
| 现用 | `iconType = { ... }` | 图片控件；可使用固定 Sprite 或数据动态选择 Sprite。 |
| 现用 | `textBoxType = { ... }` | 文字控件。 |
| 已实现未用 | `instantTextBoxType = { ... }` | `textBoxType` 的完全等价别名。 |
| 现用 | `guiButtonType = { ... }` | 按钮；支持普通/按下图片、文字、条件和事件。 |
| 现用 | `listBoxType = { ... }` | 动态列表；列表数据键默认为该控件的 `name`。 |
| 现用 | `scrollbarType = { ... }` | 与列表绑定的滚动条；指定轨道和滑块 Sprite。 |
| 现用 | `progressBarType = { ... }` | 进度条控件；引用 `.sgfx` 中的同名资源定义。 |
| 现用 | `colorBoxType = { ... }` | 纯色矩形。当前控件色只使用 RGB，透明度不由此字段控制。 |
| 现用 | `indexedMapType = { ... }` | Region ID 索引地图；支持数据着色、边界、悬停和点击 ID。 |
| 现用 | `markerLayerType = { ... }` | 附着到索引地图的动态 Marker 列表。 |
| 部分实现 | `customWidgetType = { ... }` | C++ 自定义控件扩展点；必须先由插件注册同名 Handler，通用声明式插件默认不提供 Handler。 |

所有上述控件均可嵌套在 `windowType` 或其他控件内部。子控件坐标默认相对其词法父控件；也可用 `parent` 改为相对同一窗口中的另一个具名控件。

## 6. SGUI 通用字段

### 6.1 身份、坐标、层级和裁剪

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `name = "id"` | 控件唯一名称；用于父子引用、模板、滚动条、地图、拖动轨道、事件上下文和配置校验。 |
| 已实现未用 | `parent = "other_widget"` | 将当前控件的坐标父级改为同一窗口内的具名控件；找不到时退回词法父级。 |
| 现用 | `position = { x = 10 y = 20 }` | 相对父控件的左上角坐标；根窗口为设计画布坐标。 |
| 现用 | `size = { x = 100 y = 40 }` | 控件宽度和高度。图片宽高小于等于零时，宿主可使用图片原始尺寸。 |
| 现用 | `zOrder = 10` | 相对父控件的 Z 偏移；最终 Z 为父级 Z 与子级 Z 之和。 |
| 已实现未用 | `z = 10` | `zOrder` 的别名。 |
| 已实现未用 | `layer = 10` | `zOrder` 的别名。 |
| 已实现未用 | `clipChildren = yes` | 将所有后代裁剪在当前控件矩形内。 |
| 已实现未用 | `clip_children = yes` | `clipChildren` 的别名。 |
| 已实现未用 | `clip = yes` | `clipChildren` 的别名。 |
| 仅解析 | `positionType = "..."` / `position_type = "..."` | 字符串会保存，但当前布局始终使用父级相对坐标；该字段没有布局效果。原版的 `positionType = { ... }` 块也不会生效。 |
| 忽略 | `fullScreen = yes/no` | 当前解释器未读取；窗口尺寸和宿主缩放由 `position`、`size` 与 D3D9 宿主管理。 |
| 仅解析 | `orientation = "..."` | 字段会保存，但当前布局和渲染器不消费它。 |

### 6.2 显示、启用和条件

| 状态 | 语句 | 作用 |
|---|---|---|
| 已实现未用 | `visible = yes/no` | 静态控制是否参与渲染和输入，默认 `yes`。 |
| 已实现未用 | `dontRender = yes/no` | `visible` 的反向兼容写法；`yes` 等价于 `visible = no`。 |
| 现用 | `visibleWhen = "condition"` | 每次刷新时求值；为假时控件及其后代不绘制、不接收输入。 |
| 已实现未用 | `visible_if = "condition"` | `visibleWhen` 的别名。 |
| 已实现未用 | `showIf = "condition"` | `visibleWhen` 的别名。 |
| 已实现未用 | `condition = "condition"` | 控件可见条件别名；注意行为文件中同名字段表示行为启用条件。 |
| 已实现未用 | `enabled = yes/no` | 静态控制输入是否启用，默认 `yes`。 |
| 已实现未用 | `disabled = yes/no` | `enabled` 的反向写法；`yes` 会禁用控件。 |
| 现用 | `enabledWhen = "condition"` | 动态控制输入是否启用；按钮禁用时会以灰色调绘制。 |
| 已实现未用 | `enabled_if = "condition"` | `enabledWhen` 的别名。 |

### 6.3 条件表达式

`visibleWhen`、`enabledWhen` 和行为文件中的 `enabledWhen` 共用同一求值器。

| 表达式 | 作用 |
|---|---|
| `state.active` | 读取布尔数据键。 |
| `!state.active` | 逻辑非。 |
| `a && b` | 逻辑与。 |
| `a || b` | 逻辑或。 |
| `state.tag == CHI` | 字符串或数值文本相等比较，不区分大小写。 |
| `state.tag != JAP` | 不等比较。 |
| `regions.{selectedregion.id}.active` | 先用另一个数据键替换 `{...}`，再读取最终路径；插值最大递归深度为 8。 |
| `(state.active)` | 整个子表达式外的一层括号可用。 |

限制：当前不支持 `<`、`>`、`<=`、`>=`、算术运算和可靠的任意嵌套括号。复杂条件应在 Lua 中预先计算为布尔数据键。

### 6.4 通用事件绑定

字段值是行为名称。若 `script_gui` 中存在同名 `behavior`，按其函数、阶段、条件和参数执行；否则直接尝试调用同名 Lua 动作函数。

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `onClick = "action"` | 鼠标在同一控件按下并释放且未发生拖动时触发。 |
| 已实现未用 | `onclick` / `clickAction` / `action` / `callback` | `onClick` 的别名。 |
| 已实现未用 | `onPress = "action"` | 鼠标按下时触发。 |
| 已实现未用 | `onpress` / `pressAction` | `onPress` 的别名。 |
| 已实现未用 | `onRelease = "action"` | 已按下控件收到鼠标释放时触发。 |
| 已实现未用 | `onrelease` / `releaseAction` | `onRelease` 的别名。 |
| 已实现未用 | `onHoverEnter = "action"` | 鼠标首次进入控件时触发。 |
| 已实现未用 | `onhoverenter` / `onHover` / `onhover` / `onMouseEnter` / `onmouseenter` / `hoverEnterAction` | `onHoverEnter` 的别名。 |
| 已实现未用 | `onHoverLeave = "action"` | 鼠标离开控件时触发。 |
| 已实现未用 | `onhoverleave` / `onMouseLeave` / `onmouseleave` / `hoverLeaveAction` | `onHoverLeave` 的别名。 |
| 已实现未用 | `onDragStart = "action"` | 可拖动控件开始连续移动时触发一次。 |
| 已实现未用 | `ondragstart` | `onDragStart` 的别名。 |
| 现用 | `onDrag = "action"` | 按下和连续拖动期间触发，并附带标准拖动参数。 |
| 已实现未用 | `ondrag` | `onDrag` 的别名。 |
| 现用 | `onDragEnd = "action"` | 拖动结束时触发。 |
| 已实现未用 | `ondragend` | `onDragEnd` 的别名。 |

标准动作上下文包括窗口名、控件名、列表名、列表索引、列表项 ID、鼠标 X/Y。拖动事件还包括：`normalized`、`value`、`dragx`、`dragy`、`deltax`、`deltay`、`stepdeltax`、`stepdeltay`、`target`，配置 `dragSteps` 时还包括 `stepindex`。

## 7. SGUI 图片、按钮和窗口字段

### 7.1 Sprite 选择

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `spriteType = "GFX_name"` | 固定普通 Sprite。 |
| 现用 | `quadTextureSprite = "GFX_name"` | `spriteType` 的兼容别名，当前主要用于按钮。 |
| 现用 | `spriteSource = "data.path"` | 从数据注册表或列表项动态取得 Sprite 名。列表模板中可写 `item.field`。 |
| 已实现未用 | `spriteBinding` / `textureSource` | `spriteSource` 的别名。 |
| 现用 | `spriteValuePrefix = "GFX_prefix_"` | 只有动态 Source 成功取值时才添加此前缀。 |
| 已实现未用 | `spritePrefix` | `spriteValuePrefix` 的别名。 |
| 现用 | `pressedTextureSprite = "GFX_name"` | 按下状态的固定 Sprite。 |
| 已实现未用 | `pressedQuadTextureSprite = "GFX_name"` | `pressedTextureSprite` 的别名。 |
| 现用 | `pressedSpriteSource = "data.path"` | 按下状态的动态 Sprite。为空时回退普通 Sprite。 |
| 已实现未用 | `pressedTextureSource` | `pressedSpriteSource` 的别名。 |
| 现用 | `borderSprite = "GFX_name"` | 窗口框 Sprite；也作为 `frameSprite` 的首选别名。 |
| 现用 | `frameSprite = "GFX_name"` | `borderSprite` 的别名；Marker 中用于每个 Marker 的框。 |
| 已实现未用 | `windowFrame = "GFX_name"` | `borderSprite` 的别名。 |
| 部分实现 | `scaleMode = "stretch|contain|preserveaspect|aspect|center|none"` | macOS 图片支持拉伸、保持比例或居中；Windows D3D9 当前仍统一拉伸。 |
| 部分实现 | `scale` / `fit` | `scaleMode` 的别名，同样仅在 macOS 图片路径生效。 |

### 7.2 窗口与按钮

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `frameZOrder = -1000` | 窗口框相对窗口最终 Z 的额外偏移；负值可把背景框放到所有子控件后方。 |
| 现用 | `moveable = yes` | 允许拖动该窗口。 |
| 现用 | `dragHeight = 72` | 窗口顶部可拖动区域高度；必须与 `moveable = yes` 同时使用。 |
| 现用 | `font`、`fontSize`、`alignment`、`color` | 按钮没有文字子控件时，可直接绘制按钮文字；列表按钮默认使用列表项 `text`。 |

## 8. SGUI 文字字段

| 状态 | 语句 | 作用 |
|---|---|---|
| 已实现未用 | `text = "literal"` | 静态文字；优先级低于动态 `textSource` 和 `localizationKey`。 |
| 现用 | `textSource = "data.path"` | 动态文字数据路径；列表模板中支持 `item.field` 和 `{id}`。 |
| 已实现未用 | `textBinding` / `textValue` | `textSource` 的别名。 |
| 现用 | `localizationKey = "KEY"` | 直接把指定本地化键解析为文本，优先于 `textSource` 的显示结果。 |
| 已实现未用 | `localisationKey` / `textKey` | `localizationKey` 的别名。 |
| 现用 | `localized = yes` | 把 `text` 或 `textSource` 的结果再次当作本地化键解析。 |
| 已实现未用 | `localised = yes` | `localized` 的别名。 |
| 现用 | `font = "file_stem"` | 选择字体；值对应 `font` 目录下 `.ttf/.otf` 文件名去掉扩展名后的名称，不区分大小写。 |
| 现用 | `fontSize = 20` | 像素字号；未设置时默认取 `max(12, 控件高度 × 2/3)`。 |
| 已实现未用 | `textSize = 20` | `fontSize` 的别名。 |
| 现用 | `alignment = "left|center|centre|right"` | 水平对齐；未知值回退左对齐。 |
| 已实现未用 | `textAlignment` / `align` | `alignment` 的别名。 |
| 现用 | `color = { r g b }` | 文字 RGB，分量范围通常为 `0.0` 到 `1.0`。 |
| 现用 | `wrap = yes` | 在文字矩形内换行。 |
| 已实现未用 | `wordWrap = yes` | `wrap` 的别名。 |
| 现用 | `lineSpacing = 4` | 多行文字附加行距。 |
| 部分实现 | `renderMode = "custom"` | 当前只会阻止通用文字渲染器绘制该文字；不会自动调用自定义渲染器。 |
| 部分实现 | `drawMode = "custom"` | `renderMode` 的别名。 |

字体无需安装到操作系统。Windows 宿主会递归加载 `font` 目录；找不到指定字体时回退通用 Sans Serif。

## 9. SGUI 拖动字段

这些字段可用于 `iconType`、`guiButtonType` 或其他可命中的控件。

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `draggable = yes` | 启用通用控件拖动。 |
| 现用 | `dragAxis = "horizontal|x|vertical|y"` | 拖动轴；除 `vertical/y` 外默认按水平处理。 |
| 已实现未用 | `dragOrientation` | `dragAxis` 的别名。 |
| 现用 | `dragTrack = "widget_name"` | 用具名控件矩形作为拖动范围。未设置时使用控件自身矩形。 |
| 已实现未用 | `dragBounds` / `trackWidget` | `dragTrack` 的别名。 |
| 现用 | `dragValueSource = "data.path"` | 将数据值映射为当前位置，并把目标路径作为动作参数 `target`。 |
| 已实现未用 | `dragBinding` | `dragValueSource` 的别名。 |
| 现用 | `dragMinimum = 0` | 拖动值最小值。 |
| 现用 | `dragMaximum = 1` | 拖动值最大值。 |
| 已实现未用 | `dragStep = 0.25` | 将拖动值吸附到固定数值步长。 |
| 现用 | `dragSteps = 9` | 把归一化位置转换为从 1 开始的离散 `stepindex`。 |
| 已实现未用 | `dragInverted = yes` | 反转拖动方向和归一化值。 |

## 10. SGUI 列表与滚动条字段

### 10.1 `listBoxType`

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `name = "list_name"` | 同时是控件名和默认列表数据键。当前通用列表不使用 `dataSource` 改名。 |
| 现用 | `itemTemplate = "template_widget"` | 指向同一窗口内的具名模板控件；模板根及全部子控件会为每个列表项实例化。 |
| 现用 | `scrollbarType = "scrollbar_name"` | 绑定具名 `scrollbarType` 控件。字段名不区分大小写。 |
| 现用 | `spacing = 6` | 相邻行之间的垂直间隔。 |
| 现用 | `columnSpacing = 8` | 相邻列之间的水平间隔。 |
| 现用 | `layout = "polar"` | 列表布局模式；默认是按控件宽度自动计算列数的网格。 |
| 已实现未用 | `layoutMode` / `itemLayout` | `layout` 的别名。 |
| 现用 | `disableItemsInList = "other_list"` | 若当前项 ID 已存在于另一列表，则禁用当前项。 |
| 已实现未用 | `disabledByList` | `disableItemsInList` 的别名。 |
| 现用 | `disableMatchingField = "field"` | 使用字段值而非只按 ID 匹配禁用项。 |
| 已实现未用 | `disabledMatchField` | `disableMatchingField` 的别名。 |
| 现用 | `disableFilterField = "field"` | 在另一列表中额外检查的过滤字段。 |
| 已实现未用 | `disabledFilterField` | `disableFilterField` 的别名。 |
| 现用 | `disableFilterValueSource = "data.path"` | 过滤字段必须等于该数据路径的当前值。 |
| 已实现未用 | `disabledFilterValueSource` | `disableFilterValueSource` 的别名。 |

列表项还可在数据中提供 `enabled` 或 `enabledwhen` 字段，对单个实例追加禁用条件。

### 10.2 网格布局

- 模板 `size.x` 和 `columnSpacing` 决定可容纳列数。
- 模板 `size.y` 和 `spacing` 决定行高。
- 超出 `listBoxType.size.y` 的内容通过绑定滚动条滚动。
- 列表自动裁剪实例和模板子控件，不要求额外设置 `clipChildren`。

### 10.3 极坐标/半圆布局

当 `layout` 为 `polar`、`radial` 或 `semicircle` 时，三者目前使用相同的极坐标算法。

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `polarCenter = { x = 225 y = 195 }` | 相对列表左上角的极坐标中心。 |
| 已实现未用 | `polarCenterX = 225` | 单独设置中心 X；会被 `polarCenter.x` 覆盖。 |
| 已实现未用 | `polarCenterY = 195` | 单独设置中心 Y；会被 `polarCenter.y` 覆盖。 |
| 已实现未用 | `polarRingCount = 6` | 未提供 `polarRingItemCounts` 时，自动平均分配的环数。 |
| 现用 | `polarInnerRadius = 58` | 第一圈半径。 |
| 现用 | `polarOuterRadius = 133` | 最大半径；小于等于零时按列表尺寸自动计算。 |
| 现用 | `polarRingSpacing = 15` | 环间距；未设置且多于一圈时自动计算。 |
| 现用 | `polarRingItemCounts = { 12 16 19 }` | 每一圈的项目数；不足时剩余项追加到最后一圈。 |
| 现用 | `polarStartAngle = 180` | 起始角度，单位为度。 |
| 现用 | `polarEndAngle = 360` | 结束角度，单位为度。 |

### 10.4 `scrollbarType`

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `slider = "GFX_thumb"` | 滚动条滑块 Sprite。 |
| 现用 | `track = "GFX_track"` | 滚动条轨道 Sprite。 |
| 现用 | `position`、`size` | 滚动条轨道区域；滑块高度和位置按内容长度自动计算。 |

只有列表内容超出视口时，Windows 宿主才绘制滚动条。

## 11. SGUI 进度条与色块字段

### 11.1 `progressBarType`

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `progressBar = "resource_name"` | 引用 `.sgfx` 中的 `progressBarType` 资源。 |
| 已实现未用 | `progressbar` / `progressResource` / `progressType` | `progressBar` 的别名。 |
| 现用 | `valueSource = "data.path"` | 读取 `0.0` 到 `1.0` 的动态进度；运行时会夹紧到该范围。 |
| 已实现未用 | `valueBinding` / `progressSource` | `valueSource` 的别名。 |
| 已实现未用 | `value = 0.5` | 不使用 `valueSource` 时的静态进度。 |
| 现用 | `progressColor = 0|1` | `0` 使用资源 `color`，`1` 使用 `colortwo`。 |
| 已实现未用 | `colorIndex = 0|1` | `progressColor` 的别名。 |
| 现用 | `fillFromEnd = yes` | 从右侧或底部反向填充。 |
| 已实现未用 | `reverse = yes` | `fillFromEnd` 的别名。 |
| 现用 | `drawBackground = yes/no` | 字段已实现；macOS 原型会控制背景绘制。当前 Windows D3D9 路径只绘制彩色填充，因此背景图片通常由独立 `iconType` 提供。 |

### 11.2 `colorBoxType`

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `color = { r g b }` | 纯色矩形 RGB。当前 Windows 绘制为完全不透明。 |
| 现用 | `position`、`size`、`zOrder`、条件字段 | 控制色块范围、层级和显隐。 |

## 12. SGUI 索引地图字段

### 12.1 `indexedMapType`

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `mapResource = "GFX_map"` | 引用 `.sgfx` 中的 `indexedMapResourceType`。 |
| 已实现未用 | `indexedMap` / `indexedMapResource` / `resource` | `mapResource` 的别名。 |
| 现用 | `valueSource = "regions.{id}.value"` | 对每个非零 Region ID 替换 `{id}`，读取数值并按 `colorStop` 着色。 |
| 现用 | `onClick = "action"` | 点击有效 Region 时触发，事件会附带 Region ID 作为项目 ID。 |
| 现用 | `position`、`size` | 地图目标矩形；底图、覆盖层、边界层和悬停层使用同一矩形。 |

## 13. SGUI Marker 图层字段

### 13.1 数据与地图锚点

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `dataSource = "list_name"` | Marker 数据列表。 |
| 已实现未用 | `listSource` / `itemsSource` | `dataSource` 的别名。 |
| 现用 | `mapWidget = "indexed_map_name"` | 指定作为锚点的 `indexedMapType`；为空时使用找到的第一个索引地图。 |
| 已实现未用 | `targetMap` / `indexedMapWidget` | `mapWidget` 的别名。 |
| 现用 | `regionSource = "item.regionid"` | 从列表项读取 Region ID，并取得该 Region 在索引地图中的中心锚点。 |
| 已实现未用 | `regionIdSource` / `anchorItemSource` | `regionSource` 的别名。 |
| 现用 | `xSource = "item.x"` | 读取 Marker 的归一化 X；有效范围为 `0.0` 到 `1.0`。无效时回退 Region 锚点。 |
| 已实现未用 | `markerXSource` | `xSource` 的别名。 |
| 现用 | `ySource = "item.y"` | 读取 Marker 的归一化 Y。 |
| 已实现未用 | `markerYSource` | `ySource` 的别名。 |

### 13.2 外观、头像和连线

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `frameSprite = "GFX_frame"` | 每个 Marker 的框 Sprite。 |
| 现用 | `portraitSource = "item.portrait"` | 每个 Marker 的头像 Sprite 数据字段。 |
| 已实现未用 | `imageSource` / `itemSpriteSource` | `portraitSource` 的别名。 |
| 现用 | `markerSize = { x = 68 y = 84 }` | Marker 总尺寸；默认 `68×84`。 |
| 现用 | `portraitPosition = { x = 2 y = 2 }` | 头像在 Marker 内的相对位置。 |
| 现用 | `portraitSize = { x = 64 y = 80 }` | 头像尺寸；未设置时回退 Marker 尺寸。 |
| 现用 | `lineColor = { r g b a }` | Region 锚点到 Marker 的连线颜色。 |
| 现用 | `lineWidth = 3` | 连线宽度，最小为 1。 |

### 13.3 堆叠

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `stackSource = "item.group"` | 同值 Marker 归入同一堆叠组；为空时每项独立。 |
| 已实现未用 | `markerStackSource` / `stackGroupSource` | `stackSource` 的别名。 |
| 现用 | `stackOrderSource = "item.order"` | 同组内排序值。 |
| 已实现未用 | `markerStackOrderSource` | `stackOrderSource` 的别名。 |
| 现用 | `stackDirection = "vertical|horizontal"` | `horizontal` 横向堆叠，其他值按纵向处理。 |
| 已实现未用 | `markerStackDirection` | `stackDirection` 的别名。 |
| 现用 | `stackSpacing = 4` | 相邻 Marker 额外间距。 |
| 已实现未用 | `markerStackSpacing` | `stackSpacing` 的别名。 |

### 13.4 Tooltip

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `nameSource = "item.namekey"` | Tooltip 标题数据字段。 |
| 已实现未用 | `titleSource` | `nameSource` 的别名。 |
| 现用 | `descriptionSource = "item.descriptionkey"` | Tooltip 正文数据字段。 |
| 已实现未用 | `tooltipSource` | `descriptionSource` 的别名。 |
| 现用 | `localizeTooltip = yes` | 将标题和正文作为本地化键解析。 |
| 已实现未用 | `localiseTooltip` | `localizeTooltip` 的别名。 |
| 现用 | `tooltipSize = { x = 300 y = 150 }` | Tooltip 尺寸；未设置时使用默认值。 |
| 现用 | `tooltipPlacement = "right"` | `right` 放在 Marker 右侧，其他值放在左侧。 |
| 已实现未用 | `tooltipSide` | `tooltipPlacement` 的别名。 |
| 现用 | `avoidTooltipOverlap = yes` | 尝试调整 Tooltip Y，避免覆盖 Marker。 |
| 已实现未用 | `tooltipAvoidMarkers` | `avoidTooltipOverlap` 的别名。 |
| 现用 | `tooltipColor = { r g b a }` | Tooltip 背景 RGBA。 |
| 现用 | `tooltipPadding = 12` | Tooltip 内边距。 |
| 现用 | `font`、`fontSize`、`lineSpacing`、`color` | Tooltip 文字字体、字号、行距和 RGB。 |
| 仅解析 | `tooltipText` / `tooltip` / `delayedTooltipText` | 通用字段会保存到定义对象，但当前 Marker 和普通控件都不直接显示它。 |

### 13.5 Marker 附属动作

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `markerActionSprite = "GFX_action"` | Marker 旁附属按钮 Sprite。 |
| 已实现未用 | `selectedActionSprite` | `markerActionSprite` 的别名。 |
| 现用 | `markerActionPosition = { x = -80 y = 0 }` | 附属按钮相对 Marker 的位置。 |
| 现用 | `markerActionSize = { x = 80 y = 15 }` | 附属按钮尺寸。 |
| 现用 | `onMarkerAction = "action"` | 点击附属按钮时触发的动作。 |
| 已实现未用 | `markerAction` / `selectedAction` | `onMarkerAction` 的别名。 |
| 现用 | `markerActionLocalizationKey = "KEY"` | 附属按钮文字本地化键。 |
| 已实现未用 | `markerActionTextKey` | `markerActionLocalizationKey` 的别名。 |
| 现用 | `markerActionFontSize = 11` | 附属按钮文字字号。 |
| 现用 | `draggable = yes`、`onDragEnd = "action"` | 允许玩家拖动 Marker；事件附带归一化坐标 `normalizedx`、`normalizedy` 等 Marker 专用参数。 |

## 14. SGUI 自定义控件字段

| 状态 | 语句 | 作用 |
|---|---|---|
| 部分实现 | `customWidgetType = { ... }` | 创建自定义控件节点。 |
| 部分实现 | `customType = "handler_name"` | 选择 C++ `GuiCustomWidgetRegistry` 中的 Handler。 |
| 部分实现 | `type = "handler_name"` | `customType` 的别名。 |

如果没有注册匹配 Handler，自定义控件既不会绘制，也不会处理输入。因此它不能满足“只写 `.sgui` 就添加全新控件语义”的目标。

## 15. SGFX 资源类型

| 状态 | 资源语句 | 作用 |
|---|---|---|
| 现用 | `spriteTypes = { ... }` | 习惯性的资源容器；解释器会递归查找其中资源，本身没有运行时对象。 |
| 现用 | `spriteType = { ... }` | 普通图片资源。 |
| 现用 | `progressBarType = { ... }` | 进度条样式资源；类型名不区分大小写，因此 `progressbartype` 等价。 |
| 现用 | `indexedMapResourceType = { ... }` | 索引地图资源及其离线生成参数、运行时着色参数。 |

当前没有实现 `.sgfx` 的 `fontType`。字体由宿主直接扫描 `font` 目录，不需要也不能通过 `fontType` 注册。

## 16. SGFX `spriteType`

```text
spriteType = {
    name = "GFX_example"
    texturefile = "gfx\\example\\image.png"
    noOfFrames = 1
    loadType = "INGAME"
}
```

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `name = "GFX_name"` | Sprite 唯一资源名。 |
| 现用 | `texturefile = "path"` | 图片路径；以 `gfx/` 开头时相对项目根，否则相对项目的 `gfx` 目录。反斜杠会统一为斜杠。 |
| 已实现未用 | `textureFile = "path"` | `texturefile` 的大小写兼容形式；字段匹配本身不区分大小写。 |
| 仅解析 | `effectFile = "gfx\\FX\\...fx"` | 会保存，但当前自定义 D3D9 渲染器不加载 HOI3 Effect。 |
| 现用但仅解析 | `loadType = "INGAME"` | 当前文件已写入，字段会保存，但不会改变加载时机或资源生命周期。 |
| 现用但仅解析 | `noOfFrames = 1` | 会保存帧数且最小为 1，但当前渲染器没有帧索引和动画逻辑。 |
| 已实现未用但仅解析 | `norefcount = yes` | 会保存，但当前纹理缓存不使用该标志。 |

支持的实际图片格式由平台纹理加载器决定。当前 Windows 路径通过 GDI+/D3D9 处理项目已使用的 PNG、BMP、DDS 等资源。

## 17. SGFX `progressBarType`

```text
progressBarType = {
    name = "example_progress"
    size = { x = 270 y = 22 }
    horizontal = yes
    color = { 0.27 0.71 0.37 }
    colortwo = { 0.73 0.27 0.27 }
}
```

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `name = "resource_name"` | 进度条资源名。 |
| 已实现未用 | `size = { x = 270 y = 22 }` | 解析为资源建议尺寸；当前宿主实际使用 `.sgui` 控件的 `size`。 |
| 现用 | `horizontal = yes/no` | `yes` 横向填充，`no` 纵向填充。 |
| 现用 | `color = { r g b }` | `progressColor = 0` 使用的第一颜色。 |
| 现用 | `colortwo = { r g b }` | `progressColor = 1` 使用的第二颜色。 |
| 已实现未用但仅解析 | `textureFile1 = "path"` | 会保存，但当前 Windows/macOS 通用进度条不加载该贴图。 |
| 已实现未用但仅解析 | `textureFile2 = "path"` | 会保存，但当前 Windows/macOS 通用进度条不加载该贴图。 |
| 已实现未用但仅解析 | `effectFile = "path"` | 会保存，但当前通用进度条不执行 HOI3 Effect。 |
| 忽略 | `width = 270` | 当前 `china_anti_jap.sgfx` 中存在，但解析器不读取；应使用 `size = { x = 270 y = 22 }`。 |
| 忽略 | `height = 22` | 当前解析器不读取；应使用 `size`。 |

## 18. SGFX `indexedMapResourceType`

### 18.1 运行时资源与样式

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `name = "GFX_map"` | 索引地图资源名。 |
| 现用 | `texturefile = "base.bmp"` | 地图底图。 |
| 现用 | `indexfile = "ids.bin"` | 每像素 `uint16` Region ID 图。ID 0 表示无 Region。 |
| 已实现未用 | `textureFile` / `indexFile` | 对应字段的大小写兼容形式。 |
| 现用 | `boundaryColor = { r g b a }` | 运行时 Region 边界 RGBA。 |
| 现用 | `hoverColor = { r g b a }` | 鼠标悬停 Region 的覆盖色。 |
| 现用 | `boundaryWidth = 1` | 边界扩张半径；最小可为 0。数值越大边界越粗。 |
| 现用 | `drawBoundaries = yes/no` | 是否生成并绘制边界层。 |
| 现用 | `colorStop = { ... }` | 添加一档数据颜色；可重复定义，加载后按阈值升序稳定排序。 |
| 现用 | `minimum = 20` | 该颜色档生效的最小值；运行时选取不大于当前值的最后一个 Color Stop。 |
| 已实现未用 | `threshold = 20` | `minimum` 的别名。 |
| 现用 | `color = { r g b a }` | 当前 Color Stop 的 RGBA。 |

### 18.2 离线地图生成字段

下列字段由 `gui_indexed_map_make` 使用，不是每帧运行时参数。

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `sourceDefinitionFile = "map\\definition.csv"` | 省份 ID 与地图颜色定义文件。 |
| 已实现未用 | `definitionFile` | `sourceDefinitionFile` 的别名。 |
| 现用 | `sourceProvinceFile = "map\\provinces.bmp"` | 原始 Province 颜色图。 |
| 已实现未用 | `provinceFile` | `sourceProvinceFile` 的别名。 |
| 现用 | `sourceGroupFile = "map\\region.txt"` | Province 到 Region 的分组定义。 |
| 已实现未用 | `groupFile` | `sourceGroupFile` 的别名。 |
| 现用 | `sourceItem = { id = 1 name = "region_name" }` | 把分组名映射到输出 ID；可重复。ID 必须为 `1..65535`，0 保留为空白。 |
| 现用 | `id = 1` | `sourceItem` 的输出 Region ID。 |
| 现用 | `name = "region_name"` | `sourceItem` 在分组文件中的名称。 |
| 现用 | `cropPadding = 16` | 对有效地图包围盒裁剪时保留的像素边距。 |
| 现用 | `flipVertical = yes/no` | 生成底图和 ID 图时同时进行垂直翻转。 |
| 现用 | `sourceFillColor = { r g b a }` | 生成底图时 Region 内部的默认颜色。 |
| 现用 | `sourceBoundaryColor = { r g b a }` | 生成底图时 Region 边界颜色。 |

## 19. 数据路径与列表模板约定

| 写法 | 作用 |
|---|---|
| `state.active` | 普通标量路径。数据键不区分大小写。 |
| `regions.{selectedregion.id}.name` | 使用另一个标量动态拼接路径。 |
| `regions.{id}.controlledPercentage` | 在索引地图中，`{id}` 代表当前 Region ID。 |
| `items.{id}.label` | 在列表实例中，`{id}` 代表列表项稳定 ID，而不是可变索引。 |
| `item.textkey` | 在列表模板或 Marker 中直接读取当前列表项字段。 |
| `spriteValuePrefix + 动态值` | 将紧凑数据值转换成完整 Sprite 名，例如 `1` 变成 `GFX_party_1`。 |

## 20. 新增 GUI 的最小文件集合

### 20.1 纯 2D 静态或文件数据 GUI

1. `interface/<gui_name>.sgui`：窗口、控件、坐标、条件和动作名。
2. `interface/<gui_name>.sgfx`：图片、进度条或索引地图资源。
3. `interface/gui_plugins/<gui_name>.txt`：注册插件、窗口名、启动方式、数据提供器和窗口层级。
4. `gfx/<gui_name>/...`：图片资源。
5. `font/<font_name>.ttf` 或 `.otf`：可选；`.sgui` 中使用文件名 stem。
6. `script_gui/data/<gui_name>.txt`：需要文件数据时添加。
7. `script_gui/<gui_name>.txt`：需要行为条件、Lua 回调映射或离线回退时添加。

### 20.2 实时游戏数据 GUI

在上面基础上还需要：

1. `script/<gui_module>.lua`：从 HOI3 接口建立快照并处理动作。
2. 在 `script/scripted_gui_plugins.lua` 注册插件 ID、频道、模块、刷新模式和发布优先级。

只要功能可由本文档中的内置控件和现有 Lua/HOI3 接口表达，就无需新增 C++。

## 21. 当前最容易误用的字段

1. `fullScreen`：当前完全忽略。
2. `width`、`height`（SGFX 进度条资源）：当前完全忽略，改用 `size = { x = ... y = ... }`。
3. `positionType`、`orientation`：只解析或保存，不影响当前布局。
4. `scaleMode`：macOS 原型有效，Windows D3D9 暂未实现。
5. `tooltipText`、`tooltip`、`delayedTooltipText`：当前不会生成通用 Tooltip。
6. `effectFile`、`noOfFrames`、`loadType`、`norefcount`：Sprite 中仅保存元数据，当前自定义渲染器不执行相应 HOI3 语义。
7. `textureFile1/2`（进度条）：当前不绘制这些贴图。
8. `renderMode = "custom"`：只抑制通用文字绘制，不会自动获得自定义绘制。
9. `customWidgetType`：必须有 C++ Handler，不能仅靠配置创造全新控件语义。

## 22. 推荐的后续核心补足顺序

1. 为 `.sgui/.sgfx` 建立严格 Schema、字段类型校验和“未知字段”诊断。
2. 补齐 Windows D3D9 的 `scaleMode`、通用透明度和九宫格。
3. 实现通用 Tooltip，而不是只依赖 Marker Tooltip。
4. 明确实现或正式废弃 `fullScreen`、`positionType`、`orientation` 等兼容字段。
5. 增加 Sprite 帧选择、动画与可选进度条贴图。
6. 最后再设计独立的 3D Model 控件与渲染生命周期。
