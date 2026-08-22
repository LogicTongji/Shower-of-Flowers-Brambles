# HOI3 Scripted GUI 系统简介与 SGUI/SGFX 通用语法手册

**本文档以当前仓库源码为准，覆盖：**

- Scripted GUI 解释器的核心源码分类与职责；
- 当前核心能力的完成度和验收条件；
- `.sgui`、`.sgfx` 已实现的全部控件、资源和字段；
- 当前 `.sgui`、`.sgfx`尚未使用、但解释器已经实现的语句；
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

### 2.0 New Core 基础设施

- `new_core/src/core_module.h`、`core_module_registry.h/.cpp`：统一注册、排序、初始化、Tick、生命周期分发与逆序关闭所有注入模块；Script GUI 只是首个模块。
- `new_core/src/core_hook_registry.h/.cpp`：统一管理 Hook 的安装、状态、维护和逆序卸载，当前承载 D3D9 与 Lua 5.1 Hook。
- `new_core/src/core_lifecycle.h/.cpp`、`hoi3_lifecycle.h/.cpp`：统一发布主菜单、战局、玩家变化和存档读取边界。
- `new_core/src/core_runtime.h/.cpp`：拥有模块注册表、Hook 注册表和生命周期服务，是注入 DLL 的总运行时。
- `new_core/src/script_gui_core_module.h/.cpp`：将原 Script GUI 宿主封装为 `core::IModule`，保留现有 `ScriptedGui_*` 导出 ABI。
- `new_core/src/leader_capture_core_module.h/.cpp`：第二个核心模块；通过统一 Hook 注册表接管将领捕获补丁，并通过统一 Tick 与游戏生命周期控制业务启停和原生指针清理。
- `new_core/src/leader_capture_engine.h/.cpp`：将领捕获、胜方归属判定、将领池移除/转移、补丁安装回滚及诊断状态的核心引擎；不存在独立 DLL 入口或私有 Worker。

### 2.1 配置解析与定义模型

- `new_core/src/gui_interpreter.h/.cpp`：通用词法/语法解析器；注册 Sprite、进度条、内置效果和索引地图资源；构建窗口和控件树；解析相对坐标、条件、事件、列表模板、Z 顺序、裁剪与 2D 变换，并提供变换后命中和效果采样核心。
- `new_core/src/gui_plugin_manifest.h/.cpp`：解析 `interface/gui_plugins` 中的插件清单，将窗口、数据源、启动方式、可见条件和窗口层级注册为可启动插件。
- `new_core/src/gui_behavior.h/.cpp`：解析 `script_gui/*.txt` 行为文件，将 `.sgui` 中的动作名映射到 Lua 函数、触发阶段、条件、参数和离线回退操作。
- `new_core/src/gui_declarative_data.h/.cpp`：解析静态或离线数据文件，生成标量与列表数据。
- `new_core/src/gui_localization.h/.cpp`：读取本地化文本，并为 `localizationKey`、`localized` 和 Marker 提示提供翻译。

### 2.2 插件、应用与窗口生命周期

- `new_core/src/gui_plugin.h`、`gui_plugin_registry.h/.cpp`：定义插件接口、插件工厂和插件描述符。
- `new_core/src/gui_builtin_plugins.h/.cpp`：注册通用 `declarative_gui` 插件工厂。
- `new_core/src/declarative_gui_plugin.h/.cpp`：通用声明式插件；把任意窗口与通用数据提供器连接起来，不包含战争地图专用业务逻辑。
- `new_core/src/gui_inprocess_application.h/.cpp`：加载所有界面、资源、行为和插件清单；执行基础配置校验；隔离配置错误；创建有效插件实例。
- `new_core/src/gui_window_session.h/.cpp`：管理单个 GUI 会话的绑定、刷新、开关、列表实例、输入、动作、数据、临时状态和持久化状态。
- `new_core/src/gui_window_manager.h/.cpp`：管理多窗口打开状态、可见状态、窗口 Z 顺序和模态窗口。
- `new_core/src/gui_application_bus.h/.cpp`：执行跨窗口的打开、关闭、显隐和动作转发。
- `new_core/src/gui_tick.h/.cpp`：按插件配置的刷新周期调度数据更新。

### 2.3 布局、列表、输入与事件

- `new_core/src/gui_runtime.h/.cpp`：条件环境、通用事件路由、拖动参数、列表布局、滚动状态和命中测试。
- `new_core/src/gui_render_queue.h/.cpp`：将控件树转换为统一渲染命令，并按全局 Z 顺序稳定排序。
- `new_core/src/gui_custom_widget.h/.cpp`：为无法由内置控件表达的特殊控件提供 C++ 扩展注册点。
- `new_core/src/gui_list_model.h`：通用动态列表和列表项数据结构。

### 2.4 数据驱动、Lua 桥与统一发布者

- `new_core/src/gui_data.h/.cpp`：通用 `GuiDataRegistry`；保存布尔、整数、浮点、字符串和列表；解析数据路径、条件表达式和 `{变量}` 插值。
- `new_core/src/gui_data_provider.h/.cpp`：通用数据提供器接口和注册表。
- `new_core/src/gui_file_data_provider.h/.cpp`：从声明式数据文件加载快照。
- `new_core/src/gui_sequence_data_provider.h/.cpp`：按顺序播放离线快照，主要用于原型和回退测试。
- `new_core/src/gui_data_bridge.h/.cpp`：把外部发布者提供的数据快照导入统一数据注册表。
- `new_core/src/gui_lua_bridge.h/.cpp`：维护 Lua 数据频道、更新序号、动作队列、会话边界和游戏生命周期快照。
- `new_core/src/gui_lua_native_binding.h/.cpp`：在 HOI3 的 Lua 5.1 State 中注册原生数据发布与动作读取接口。
- `new_core/src/gui_lua51_hook.h/.cpp`：捕获游戏创建的 Lua State，安装和卸载原生绑定，不再主动调用危险的 HOI3 Lua 接口。
- `script/scripted_gui_runtime.lua`：通用 Lua 插件调度器、发布者所有权、刷新策略、错误冷却和动作泵。
- `script/scripted_gui_plugins.lua`：注册需要从游戏读取实时数据的 Lua 插件。
- `script/gui_data_bridge.lua`、`script/gui_action_bridge.lua`：Lua 侧数据发布与动作消费接口。

### 2.5 渲染、资源和特殊可视化

- `new_core/src/gui_host_d3d9.h/.cpp`：Windows 游戏内宿主；创建会话、缩放设计坐标、绘制命令、路由鼠标输入并控制底层游戏点击穿透。
- 每个会话的逻辑画布严格等于其根 `windowType` 的 `position + size` 矩形；子控件绘制与输入不能越过根边界。窗口拖动以游戏 D3D9 Viewport 为外边界，根画布触边后停止，不能继续拖出游戏客户区。
- `new_core/src/gui_d3d9_hook.h/.cpp`：挂接 Direct3D 9 Present/Reset 生命周期。
- `new_core/src/gui_texture_loader_d3d9.h/.cpp`：加载和缓存 D3D9 图片资源。
- `new_core/src/gui_text_renderer_d3d9.h/.cpp`：递归加载 `font` 目录中的 `.ttf/.otf`，使用 GDI+ 生成文字纹理并缓存。
- `new_core/src/gui_indexed_map_core.h/.cpp`：平台无关的 Region ID 图、着色、边界和命中核心。
- `new_core/src/gui_indexed_map_d3d9.h/.cpp`：Windows 索引地图渲染、悬停和点击 Region。
- `new_core/src/gui_marker_layer_d3d9.h/.cpp`：Windows 地图 Marker、头像、连线、堆叠、提示、拖动和附属动作。
- `new_core/src/gui_indexed_map_make.cpp`：根据地图源文件离线生成底图和 Region ID 二进制图。

### 2.6 注入、游戏生命周期与持久化

- `new_core/src/scripted_gui_overlay_dll.cpp`、`scripted_gui_overlay.def`：兼容现有 ABI 的薄 DLL 入口和导出接口。
- `new_core/src/scripted_gui_injector.cpp`：把 New Core DLL 注入指定 HOI3 进程。
- `new_core/src/hoi3_lifecycle.h/.cpp`：安全识别主菜单与战局状态，并向全部核心模块发布统一生命周期。
- `new_core/src/gui_persistence.h/.cpp`：按插件、会话和存档边界保存/恢复 GUI 状态。
- `new_core/src/*_probe*.cpp`：离线、集成和回归测试程序，不是最终 DLL 的业务模块。

## 3. 系统完成度

### 3.1 已形成闭环的核心功能

- `.sgui/.sgfx` 解析、资源注册和插件清单加载；
- 窗口、图片、文字、按钮、色块、进度条、列表、滚动条、索引地图、MarkerLayer 和 Custom 扩展控件；
- 父子相对坐标、显式 `parent`、统一 Z 顺序、父级裁剪和列表裁剪；
- 窗口拖动、控件拖动、悬停、按下、释放、点击和拖动事件；
- 通用列表模板实例化、网格布局、半圆/极坐标布局和滚动条自动绑定；
- `visibleWhen`、`enabledWhen`、数据路径插值和动态文字/图片/数值绑定；
- Sprite 固定帧、数据帧、循环/往返/单次动画，以及可选进度条贴图；
- 图片、文字、按钮、色块、进度条和滚动条的统一旋转、缩放、翻转、枢轴与变换后输入命中；
- 声明式内置 `effectType`，支持静态调色、亮度脉冲、透明度脉冲和颜色脉冲，不依赖 HOI3 Effect 文件；
- Lua 数据发布、Lua 动作回调、离线回退、刷新调度和发布者稳定性；
- 多插件、多窗口、窗口打开/关闭、游戏内/主菜单生命周期隔离；
- 持久化状态、会话切换和存档回滚恢复；
- D3D9 游戏内绘制、自适应缩放、输入区域收缩和底层游戏点击穿透；
- `.sgui/.sgfx` 的未知字段、类型、范围、必填项、互斥项和控件适用范围诊断；原版 `.gui/.gfx` 可选择严格模式；
- `fullScreen`、`positionType`、`orientation` 原版兼容布局字段。

中国战争地图是第一个完整实例，议会半圆席位图是第二个纯声明式实例。

### 3.2 系统待补充的核心功能

- **若目标定义为支持由 Lua 数据驱动的通用 Windows D3D9 2D 游戏内 GUI，则核心能力已经补齐。**
- **若目标定义为完整复刻 HOI3/HOI4 全部 GUI 能力，尚未完全补齐。**

仍存在的能力边界：

1. 通用 3D 模型控件与对应资源生命周期尚未实现。
2. `customWidgetType` 仍要求 C++ 注册对应 Handler；它是扩展新控件语义的边界，不是纯配置控件。
3. 内置 `effectType` 是安全的颜色乘算与周期效果抽象，不加载原版 HOI3 `.fx`，也不执行任意 Shader。
4. 几何变换当前面向普通 2D 叶控件；索引地图、MarkerLayer、Custom、列表容器和根窗口不套用该通用变换，以避免其专用坐标系统与输入映射失配。

### 3.3 系统的初步验收结论

“只改 interface/font/gfx/lua 就能添加新 GUI”的验收标准，对内置 2D 控件范围内的界面**已经达到**；对任意新控件语义或 3D 界面则尚未达到。

- 纯静态或文件数据驱动的 2D GUI：不需要新增 C++，但除 `.sgui/.sgfx` 和素材外，至少还要在 `interface/gui_plugins` 添加插件清单；通常还需要 `script_gui/data` 数据文件。
- 读取游戏实时状态、修改游戏状态或响应复杂动作的 GUI：不需要新增 C++ 的前提是现有 Lua/HOI3 接口足够，但仍需新增 Lua 数据模块、动作函数，并在 `script/scripted_gui_plugins.lua` 注册。
- 需要新控件语义、3D 模型、尚未暴露的游戏内存数据或新 Hook：仍需修改 C++。

因此当前已经达到的实际验收目标是：

> 对现有内置控件能够表达的 2D GUI，可以通过 `.sgui + .sgfx + 插件清单 + 数据/行为 Lua + 素材/字体` 完成，不再为每个 GUI 新写一个专用 C++ 程序。

## 4. 系统基础语法

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
| 已实现（需 C++） | `customWidgetType = { ... }` | C++ 自定义控件扩展点；Windows Draw/Input 链路已经接通，但必须先由插件注册同名 Handler，通用声明式插件默认不提供 Handler。 |

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
| 已实现 | `positionType = { name = "anchor" position = { x = 0 y = 0 } }` | 注册原版风格的具名坐标。名称进入解释器的全局位置注册表，可由控件的字符串形式 `positionType` 引用。 |
| 已实现 | `positionType = "anchor"` / `position_type = "anchor"` | 引用具名坐标；具名坐标与控件自身 `position` 相加后，再交给 `orientation` 计算最终位置。找不到名称时仅使用控件自身 `position`。 |
| 已实现 | `fullScreen = yes/no` | 仅对作为会话根窗口运行的 `windowType` 生效。`yes` 时根矩形实时绑定当前 D3D9 客户区，窗口模式、全屏模式和分辨率变化都会更新根画布；此时宿主不再缩放或拖动整个根画布。 |
| 已实现 | `orientation = "CENTER/UPPER_LEFT/UPPER_RIGHT/LOWER_LEFT/LOWER_RIGHT"` | 以父控件矩形为锚点计算子控件位置。还支持 `CENTRE`、`CENTER_UP/TOP`、`CENTER_DOWN/BOTTOM`、`CENTER_LEFT`、`CENTER_RIGHT`。右侧和下侧锚点把正 `x/y` 解释为距对应边缘的内缩距离。 |

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
| 已实现未用 | `opacity = 0.0~1.0` | 控件透明度；与父级透明度相乘。最终透明度为零时不可见且不拦截输入。 |
| 已实现未用 | `alpha = 0.0~1.0` | `opacity` 的兼容别名。 |
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

### 6.5 可继承样式默认值

任意控件可声明 `styleDefaults = { ... }`。这些值只作用于其后代控件，按控件树逐层继承；更深层的 `styleDefaults` 会覆盖外层默认值，控件自身显式字段具有最高优先级。根 `windowType` 中声明它，可统一管理整个窗口的尺寸阈值和视觉样式。

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `styleDefaults = { ... }` | 为当前控件的后代建立继承样式环境，本身仍使用自己的显式字段。 |
| 现用 | `color = { r g b }` | 默认文字颜色。 |
| 现用 | `lineColor = { r g b a }` | 默认连线颜色。 |
| 现用 | `tooltipColor = { r g b a }` | 默认 Tooltip 背景色。 |
| 已实现未用 | `tooltipTextColor = { r g b }` | 默认通用 Tooltip 文字颜色。 |
| 已实现未用 | `tooltipSize = { x = 240 y = 96 }` | 默认通用 Tooltip 外框尺寸。 |
| 已实现未用 | `tooltipOffset = { x = 12 y = 16 }` | 默认通用 Tooltip 相对鼠标或目标控件的偏移。 |
| 已实现未用 | `tooltipSprite = "GFX_tooltip"` | 默认通用 Tooltip 背景 Sprite。 |
| 已实现未用 | `tooltipFont = "font_name"` | 默认通用 Tooltip 字体。 |
| 已实现未用 | `tooltipFontSize = 18` | 默认通用 Tooltip 字号。 |
| 已实现未用 | `tooltipLineSpacing = 3` | 默认通用 Tooltip 行距。 |
| 已实现未用 | `tooltipPlacement = "cursor"` | 默认通用 Tooltip 放置方式。 |
| 已实现未用 | `tooltipScaleMode = "stretch"` | 默认通用 Tooltip 背景缩放方式。 |
| 已实现未用 | `tooltipNineSlice = { left = 8 top = 8 right = 8 bottom = 8 }` | 默认通用 Tooltip 背景九宫格。 |
| 已实现未用 | `tooltipDelay = 250` | 默认通用 Tooltip 悬停延迟，单位毫秒。 |
| 已实现未用 | `tooltipWrap = yes` | 默认通用 Tooltip 是否换行。 |
| 已实现未用 | `localizeTooltip = yes` | 默认把通用或 Marker Tooltip 文本作为本地化键解析。 |
| 现用 | `frameZOrder = -1000` | 默认窗口框层级偏移。 |
| 已实现未用 | `fontSize = 20` | 默认文字字号。 |
| 已实现未用 | `lineSpacing = 4` | 默认文字行距。 |
| 现用 | `lineWidth = 3` | 默认连线宽度。 |
| 现用 | `tooltipPadding = 12` | 默认 Tooltip 内边距。 |
| 现用 | `tooltipSearchStep = 12` | Marker Tooltip 避让其他 Marker 时的垂直搜索步长。 |
| 现用 | `minimumThumbSize = 18` | 默认滚动条最小滑块尺寸。 |
| 现用 | `disabledBrightness = 0.588235` | 禁用按钮的 RGB 亮度乘数，范围 `0.0~1.0`。 |
| 现用 | `disabledOpacity = 0.588235` | 禁用按钮的透明度乘数，范围 `0.0~1.0`。 |

### 6.6 严格 Schema 与未知字段诊断

严格字段诊断默认对 `.sgui` 和 `.sgfx` 启用。原版 `.gui/.gfx` 默认仍采用宽松兼容模式；启动进程前设置环境变量 `SCRIPTED_GUI_STRICT_LEGACY=1`，即可对原版扩展名启用同一套可选严格模式。C++ 测试或工具也可在加载前调用 `GuiInterpreter::SetStrictLegacyFiles(true)`。

当前诊断覆盖：

1. SGUI/SGFX 根容器与对象类型。
2. 通用控件字段、`styleDefaults`、静态数据声明。
3. Sprite、进度条、内置效果、索引地图资源、`sourceItem` 和 `colorStop`。
4. `position`、`size`、`pivot`、`transformScale`、颜色、九宫格等常用子块的成员名。
5. 字段值类型：标量、块、布尔、整数、浮点数和枚举。
6. 数值范围：颜色、透明度、禁用亮度为 `0~1`，尺寸和内边距不得为负，帧数和资源 ID 等具有各自下限或上下限。
7. 必填项：控件名、非全屏窗口尺寸、列表模板、滚动条滑块与轨道、进度条资源、索引地图资源、自定义控件类型以及资源文件的关键字段。
8. 互斥项：别名重复、静态与动态文本源并存、正反布尔字段并存、静态值与动态值源并存，以及 `fullScreen=yes` 与 `moveable=yes`。
9. 控件适用范围：窗口、列表、进度条、滚动条、索引地图、MarkerLayer 和 Custom 专用字段不能误用于其他控件。
10. 文件路径、源码行号、对象类型与对象名称；拼写接近时附带建议。

示例：

```text
interface/example.sgui:42: unknown field 'visiblWhen' in guiButtonType 'open_button'; did you mean 'visiblewhen'?
```

所有 Schema 问题当前均为非致命加载诊断：错误字段会按解释器现有回退规则处理，诊断可从 `GuiInterpreter::LoadDiagnostics()` 和宿主诊断日志取得。`dataListType.item` 内的业务字段名称保持开放，不参与未知字段诊断。

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
| 已实现未用 | `scaleMode = "stretch/contain/preserve/preserveAspect/aspect/center/none"` | Windows D3D9 支持拉伸、保持比例或居中裁切；适用于窗口框、图片、按钮和滚动条贴图。 |
| 已实现未用 | `scale` / `fit` | `scaleMode` 的别名。 |
| 已实现未用 | `nineSlice = { left = 8 top = 8 right = 8 bottom = 8 }` | Windows D3D9 九宫格；边角保持尺寸，边缘和中心拉伸。适用于窗口框、图片、按钮和滚动条贴图。 |
| 已实现未用 | `nine_slice = { ... }` | `nineSlice` 的兼容别名。 |

### 7.2 Sprite 帧与动画

`frame` 与动画字段适用于 `iconType` 和 `guiButtonType`。帧编号与原版 HOI3 一致，从 **1** 开始；运行时优先级为 `frameSource > 动画 > frame`。

| 状态 | 语句 | 作用 |
|---|---|---|
| 已实现未用 | `frame = 1` | 固定选择 Sprite Sheet 的第几帧；超出范围时夹紧到 `1..noOfFrames`。 |
| 已实现未用 | `frameSource = "data.path"` | 从数据注册表动态读取帧号；列表模板中支持 `item.field` 和 `{id}`。非整数按最接近整数处理。 |
| 已实现未用 | `frameBinding` | `frameSource` 的别名。 |
| 已实现未用 | `animate = yes/no` | 显式开启或关闭自动动画；未写时继承 Sprite 资源的 `animation` 设置。 |
| 已实现未用 | `animationMode = "loop/pingpong/once"` | 覆盖资源动画模式：循环、往返或播放一次后停在末帧。 |
| 已实现未用 | `animationFrameTime = 100` | 每帧持续毫秒数。 |
| 已实现未用 | `animationFrameDuration = 100` | `animationFrameTime` 的别名。 |
| 已实现未用 | `animationFps = 12` | 用每秒帧数指定速度；与上面两个毫秒字段互斥。 |
| 已实现未用 | `animationStartFrame = 1` | 覆盖动画起始帧；`0` 表示继承资源。 |
| 已实现未用 | `animationEndFrame = 8` | 覆盖动画结束帧；`0` 表示继承资源或使用最后一帧。 |
| 已实现未用 | `animationOffset = 50` | 动画时间偏移，单位毫秒；可为负数。 |
| 已实现未用 | `animationTimeSource = "state.time_ms"` | 用数据值作为动画时钟，单位毫秒；适合由 Lua 同步、暂停或精确控制动画。未设置时使用窗口会话时钟。 |

```gui
iconType = {
    name = "animated_warning"
    spriteType = "GFX_animated_warning"
    animate = yes
    animationMode = "pingpong"
    animationFps = 10
    position = { x = 20 y = 20 }
    size = { x = 64 y = 64 }
}
```

多帧裁切与 `stretch`、`contain`、`center` 和 `nineSlice` 均兼容。按钮普通 Sprite 与按下 Sprite 使用同一个帧选择/动画配置，并分别按各自资源的 `noOfFrames` 夹紧。

### 7.3 通用 2D 几何变换

几何变换适用于 `iconType`、`textBoxType`/`instantTextBoxType`、`guiButtonType`、`colorBoxType`、`progressBarType` 和 `scrollbarType`。绘制与输入使用同一份最终变换：旋转或缩放后的按钮不会继续使用旧矩形命中。变换只作用于当前控件，不自动传递给子控件。

| 状态 | 语句 | 作用 |
|---|---|---|
| 已实现未用 | `rotation = 15` | 当前控件顺时针旋转角度，单位为度；可为任意实数。 |
| 已实现未用 | `rotationSource = "data.path"` | 从数据注册表动态读取旋转角；列表模板支持 `item.field` 和 `{id}`。 |
| 已实现未用 | `pivot = { x = 0.5 y = 0.5 }` | 旋转、缩放和翻转枢轴，使用控件矩形内的归一化坐标；`0,0` 为左上角，`1,1` 为右下角。 |
| 已实现未用 | `transformScale = { x = 1.0 y = 1.0 }` | X/Y 静态几何缩放，范围 `0.001~100`。它不同于图片适配字段 `scaleMode` 及其别名 `scale`。 |
| 已实现未用 | `scaleSource = "data.path"` | 动态统一缩放，同时覆盖 X/Y；`transformScaleSource` 是等价别名。 |
| 已实现未用 | `scaleXSource = "data.path"` | 动态 X 缩放；在统一动态缩放之后覆盖 X。`transformScaleXSource` 是等价别名。 |
| 已实现未用 | `scaleYSource = "data.path"` | 动态 Y 缩放；在统一动态缩放之后覆盖 Y。`transformScaleYSource` 是等价别名。 |
| 已实现未用 | `flipX = yes` | 围绕 `pivot` 水平翻转。 |
| 已实现未用 | `flipY = yes` | 围绕 `pivot` 垂直翻转。 |

```gui
guiButtonType = {
    name = "rotating_button"
    spriteType = "GFX_rotating_button"
    rotationSource = "state.button_angle"
    pivot = { x = 0.5 y = 0.5 }
    transformScale = { x = 1.1 y = 1.1 }
    onClick = "activate_button"
    position = { x = 100 y = 100 }
    size = { x = 160 y = 48 }
}
```

九宫格的九个切片会以完整控件矩形为共同枢轴整体变换，不会分别旋转。`contain` 和 `center` 产生的实际图片矩形也仍以原控件矩形的 `pivot` 为枢轴。变换后的图形继续受父级轴对齐裁剪矩形约束。

### 7.4 窗口与按钮

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `frameZOrder = -1000` | 窗口框相对窗口最终 Z 的额外偏移；负值可把背景框放到所有子控件后方。 |
| 现用 | `moveable = yes` | 允许拖动该窗口。 |
| 现用 | `dragHeight = 72` | 窗口顶部可拖动区域高度；必须与 `moveable = yes` 同时使用。 |
| 现用 | `font`、`fontSize`、`alignment`、`color` | 按钮没有文字子控件时，可直接绘制按钮文字；列表按钮默认使用列表项 `text`。 |
| 现用 | `disabledBrightness = 0.588235` | 按钮禁用时的 RGB 亮度乘数；可由 `styleDefaults` 继承。 |
| 现用 | `disabledOpacity = 0.588235` | 按钮禁用时的透明度乘数；可由 `styleDefaults` 继承。 |

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
| 现用 | `alignment = "left/center/centre/right"` | 水平对齐；未知值回退左对齐。 |
| 已实现未用 | `textAlignment` / `align` | `alignment` 的别名。 |
| 现用 | `color = { r g b }` | 文字 RGB，分量范围通常为 `0.0` 到 `1.0`。 |
| 现用 | `wrap = yes` | 在文字矩形内换行。 |
| 已实现未用 | `wordWrap = yes` | `wrap` 的别名。 |
| 现用 | `lineSpacing = 4` | 多行文字附加行距。 |
| 部分实现 | `renderMode = "custom"` | 当前只会阻止通用文字渲染器绘制该文字；不会自动调用自定义渲染器。 |
| 部分实现 | `drawMode = "custom"` | `renderMode` 的别名。 |

字体无需安装到操作系统。Windows 宿主会递归加载 `font` 目录；找不到指定字体时回退通用 Sans Serif。

### 8.1 通用 Tooltip

除 `markerLayerType` 外，所有可见且启用的普通控件都可声明通用 Tooltip。Tooltip 位于窗口最终绘制层，不参与命中测试，因此不会吞掉控件点击；其外框始终限制在根窗口画布内。列表模板中的 Tooltip 支持 `item.field` 和 `{id}`。

| 状态 | 语句 | 作用 |
|---|---|---|
| 已实现未用 | `tooltipText = "literal"` | 静态 Tooltip 文本。 |
| 已实现未用 | `tooltip = "literal"` | `tooltipText` 的别名。 |
| 已实现未用 | `delayedTooltipText = "literal"` | 静态文本兼容别名；是否延迟仍由 `tooltipDelay` 决定。 |
| 已实现未用 | `tooltipSource = "data.path"` | 从数据注册表读取文本；列表模板可写 `item.field`，路径可含 `{id}`。动态结果为空时回退静态文本。 |
| 已实现未用 | `tooltipBinding` / `tooltipValue` | `tooltipSource` 的别名。 |
| 已实现未用 | `tooltipLocalizationKey = "KEY"` | 直接解析本地化键，优先于静态文本和动态 Source。 |
| 已实现未用 | `tooltipTextKey` | `tooltipLocalizationKey` 的别名。 |
| 已实现未用 | `localizeTooltip = yes` | 把最终文本再次作为本地化键解析。 |
| 已实现未用 | `localiseTooltip` | `localizeTooltip` 的别名。 |
| 已实现未用 | `tooltipSize = { x = 240 y = 96 }` | Tooltip 外框尺寸；配置 Tooltip 时必须为正数。 |
| 已实现未用 | `tooltipOffset = { x = 12 y = 16 }` | 相对鼠标或目标控件锚点的 X/Y 偏移。 |
| 已实现未用 | `tooltipPlacement = "cursor/right/left/top/bottom"` | `cursor` 跟随鼠标；其他值相对目标控件对应边缘放置。未知值按 `cursor`。 |
| 已实现未用 | `tooltipSide` | `tooltipPlacement` 的别名。 |
| 已实现未用 | `tooltipDelay = 250` | 鼠标持续停留多少毫秒后显示；默认 `0`。 |
| 已实现未用 | `tooltipSprite = "GFX_tooltip"` | 背景 Sprite；未设置时使用 `tooltipColor` 绘制纯色矩形。 |
| 已实现未用 | `tooltipBackgroundSprite` | `tooltipSprite` 的别名。 |
| 已实现未用 | `tooltipColor = { r g b a }` | 背景颜色或 Sprite 调色，最终透明度还会乘以控件继承透明度。 |
| 已实现未用 | `tooltipScaleMode = "stretch/contain/center"` | 背景 Sprite 缩放；兼容 `preserve/preserveAspect/aspect/none`。 |
| 已实现未用 | `tooltipNineSlice = { left = 8 top = 8 right = 8 bottom = 8 }` | 背景 Sprite 九宫格；启用时优先于 `tooltipScaleMode`。 |
| 已实现未用 | `tooltip_nine_slice = { ... }` | `tooltipNineSlice` 的别名。 |
| 已实现未用 | `tooltipPadding = 10` | 文字相对外框的内边距。 |
| 已实现未用 | `tooltipFont = "font_name"` | Tooltip 专用字体；未设置时回退控件 `font`。 |
| 已实现未用 | `tooltipFontSize = 18` | Tooltip 专用字号；未设置时回退控件 `fontSize`，两者至少一个必须为正数。 |
| 已实现未用 | `tooltipTextColor = { r g b }` | Tooltip 文字 RGB。 |
| 已实现未用 | `tooltipLineSpacing = 3` | Tooltip 行距；为 `0` 时回退控件 `lineSpacing`。 |
| 已实现未用 | `tooltipWrap = yes` | 是否在 Tooltip 文字矩形内换行。 |

最小示例：

```gui
guiButtonType = {
    name = "example_button"
    tooltipSource = "items.{id}.description"
    tooltipSize = { x = 260 y = 90 }
    tooltipOffset = { x = 14 y = 18 }
    tooltipPlacement = "cursor"
    tooltipSprite = "GFX_common_tooltip"
    tooltipNineSlice = { left = 10 top = 10 right = 10 bottom = 10 }
    tooltipFont = "pixel_china"
    tooltipFontSize = 18
    tooltipTextColor = { 0.95 0.95 0.90 }
    tooltipPadding = 12
    tooltipDelay = 200
}
```

## 9. SGUI 拖动字段

这些字段可用于 `iconType`、`guiButtonType` 或其他可命中的控件。

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `draggable = yes` | 启用通用控件拖动。 |
| 现用 | `dragAxis = "horizontal/x & vertical/y"` | 拖动轴；除 `vertical/y` 外默认按水平处理。 |
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
| 现用 | `itemFilterField = "tag"` | 读取每个列表项的指定字段，只实例化符合过滤值的项目；被过滤项目不占据布局和滚动高度。 |
| 现用 | `itemFilterValueSource = "state.viewertag"` | 从统一数据注册表读取当前过滤值。必须和 `itemFilterField` 同时使用。 |
| 已实现未用 | `filterField` / `filterValueSource` | 上述两个字段的简写别名。 |

列表项还可在数据中提供 `visible`、`visiblewhen`、`enabled` 或 `enabledwhen` 字段。前两者决定是否实例化，后两者只决定是否允许输入。

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
| 现用 | `minimumThumbSize = 18` | 自动计算后允许的最小滑块尺寸；可由 `styleDefaults` 继承。设置为 0 表示不额外限制。 |

只有列表内容超出视口时，系统才绘制滚动条。

## 11. SGUI 进度条与色块字段

### 11.1 `progressBarType`

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `progressBar = "resource_name"` | 引用 `.sgfx` 中的 `progressBarType` 资源。 |
| 已实现未用 | `progressbar` / `progressResource` / `progressType` | `progressBar` 的别名。 |
| 现用 | `valueSource = "data.path"` | 读取 `0.0` 到 `1.0` 的动态进度；运行时会夹紧到该范围。 |
| 已实现未用 | `valueBinding` / `progressSource` | `valueSource` 的别名。 |
| 已实现未用 | `value = 0.5` | 不使用 `valueSource` 时的静态进度。 |
| 现用 | `progressColor = 0/1` | `0` 使用资源 `color/textureFile1`，`1` 使用 `colortwo/textureFile2`。贴图不存在或加载失败时回退对应纯色。 |
| 已实现未用 | `colorIndex = 0/1` | `progressColor` 的别名。 |
| 现用 | `fillFromEnd = yes` | 从右侧或底部反向填充。 |
| 已实现未用 | `reverse = yes` | `fillFromEnd` 的别名。 |
| 现用 | `drawBackground = yes/no` | 字段已解析并保留；Windows D3D9 当前不生成独立的进度条背景层，背景图片应由独立 `iconType` 提供。 |

### 11.2 `colorBoxType`

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `color = { r g b }` | 纯色矩形 RGB；最终 Alpha 使用控件继承后的 `opacity`。 |
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
| 现用 | `lineColor = { r g b a }` | 锚点到 Marker 的连线颜色。 |
| 现用 | `lineWidth = 3` | 连线宽度，最小为 1。 |

### 13.3 堆叠

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `stackSource = "item.group"` | 同值 Marker 归入同一堆叠组；为空时每项独立。 |
| 已实现未用 | `markerStackSource` / `stackGroupSource` | `stackSource` 的别名。 |
| 现用 | `stackOrderSource = "item.order"` | 同组内排序值。 |
| 已实现未用 | `markerStackOrderSource` | `stackOrderSource` 的别名。 |
| 现用 | `stackDirection = "vertical/horizontal"` | `horizontal` 横向堆叠，其他值按纵向处理。 |
| 已实现未用 | `markerStackDirection` | `stackDirection` 的别名。 |
| 现用 | `stackSpacing = 4` | 相邻 Marker 额外间距。 |
| 已实现未用 | `markerStackSpacing` | `stackSpacing` 的别名。 |

### 13.4 Tooltip

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `nameSource = "item.namekey"` | Tooltip 标题数据字段。 |
| 已实现未用 | `titleSource` | `nameSource` 的别名。 |
| 现用 | `descriptionSource = "item.descriptionkey"` | Tooltip 正文数据字段。 |
| 已实现未用 | `markerDescriptionSource` | `descriptionSource` 的别名；`tooltipSource` 已保留给普通控件的通用 Tooltip。 |
| 现用 | `localizeTooltip = yes` | 将标题和正文作为本地化键解析。 |
| 已实现未用 | `localiseTooltip` | `localizeTooltip` 的别名。 |
| 现用 | `tooltipSize = { x = 300 y = 150 }` | Tooltip 尺寸；配置标题或正文 Source 时必须为正数。 |
| 现用 | `tooltipPlacement = "right"` | `right` 放在 Marker 右侧，其他值放在左侧。 |
| 已实现未用 | `tooltipSide` | `tooltipPlacement` 的别名。 |
| 现用 | `avoidTooltipOverlap = yes` | 尝试调整 Tooltip Y，避免覆盖 Marker。 |
| 已实现未用 | `tooltipAvoidMarkers` | `avoidTooltipOverlap` 的别名。 |
| 现用 | `tooltipColor = { r g b a }` | Tooltip 背景 RGBA。 |
| 现用 | `tooltipPadding = 12` | Tooltip 内边距。 |
| 现用 | `font`、`fontSize`、`lineSpacing`、`color` | Tooltip 文字字体、字号、行距和 RGB。 |

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
| 已实现（需 C++） | `customWidgetType = { ... }` | 创建自定义控件节点，并接入 Windows 的统一绘制队列、裁剪、Z 顺序和输入分发。 |
| 已实现（需 C++） | `customType = "handler_name"` | 选择 C++ `GuiCustomWidgetRegistry` 中注册的 Handler；名称区分大小写。 |
| 已实现（需 C++） | `type = "handler_name"` | `customType` 的别名。 |

**注意：如果没有注册匹配 Handler，自定义控件既不会绘制，也不会处理输入。`customWidgetType` 是 C++ 扩展点，不是纯配置控件；只有使用第 4—7 章已有内置控件时，才能仅靠 `.sgui/.sgfx` 添加界面。**

### 14.1 什么时候应创建自定义控件

适合使用 `customWidgetType` 的功能：

1. 动态曲线图、节点图、复杂战场标记层等不能由普通图片和文字组合表达的画布。
2. 需要特殊 D3D9 绘制、着色器或独立纹理更新策略的控件。
3. 需要自行解释鼠标拖拽、滚轮、键盘、文本输入或焦点的交互控件。
4. 后续的 3D 模型预览等拥有独立渲染生命周期的控件。

普通窗口、图片、文字、按钮、列表、滚动条和进度条应优先使用内置控件，不要为它们重复编写 Custom Handler。

### 14.2 第一步：在 `.sgui` 声明控件

```text
customWidgetType = {
    name = "strategic_chart_widget"
    customType = "strategic_chart"

    position = { x = 120 y = 90 }
    size = { x = 640 y = 320 }
    zOrder = 30
    opacity = 1.0

    visibleWhen = "state.active"
    enabledWhen = "state.chart_enabled"
}
```

字段说明：

1. `name` 是这个控件实例的唯一名称，供布局、诊断和事件上下文识别。
2. `customType` 是 Handler 注册键；上例必须注册名为 `strategic_chart` 的 Handler。
3. 若省略 `customType`，解释器会使用 `name` 查找 Handler。
4. `position`、`size`、`zOrder`、`opacity`、父子相对坐标、`clipChildren`、`visibleWhen` 和 `enabledWhen` 都继续由通用解释器处理。
5. Custom Handler 接收的是已经完成布局、缩放、条件计算和裁剪计算的 `GuiResolvedWidget`，不应再次解析 `.sgui`。
6. `onClick`、`onHover` 等通用动作字段可以与 Custom Handler 并存；控件特有的复杂输入由 Handler 的统一事件回调处理。

### 14.3 第二步：实现 Windows Handler

建议为每一种新控件建立独立的 `.h/.cpp`，并提供一个创建 Handler 的工厂函数：

```cpp
#include <d3d9.h>

#include "gui_custom_widget.h"

struct StrategicChartState
{
    int selectedNode = -1;
    bool dragging = false;
};

gui::GuiCustomWidgetHandler CreateStrategicChartHandler()
{
    gui::GuiCustomWidgetHandler handler;

    handler.draw = [](
        const gui::GuiResolvedWidget& widget,
        const gui::GuiCustomWidgetContext& context
    ) {
        auto* device = static_cast<IDirect3DDevice9*>(
            context.graphicsContext
        );
        auto* state = static_cast<StrategicChartState*>(
            context.hostContext
        );
        if (!device || !state)
        {
            return;
        }

        // 由新控件实现者编写；使用 widget.rect、widget.opacity
        // 和 widget.clipRect 绘制，不要重新计算布局。
        DrawStrategicChart(*device, widget, *state);
    };

    handler.event = [](
        const gui::GuiResolvedWidget& widget,
        const gui::GuiCustomWidgetContext& context,
        const gui::GuiCustomInputEvent& event
    ) -> bool {
        auto* state = static_cast<StrategicChartState*>(
            context.hostContext
        );
        if (!state)
        {
            return false;
        }

        switch (event.type)
        {
        case gui::GuiCustomInputEventType::PointerDown:
            if (event.button != gui::GuiCustomPointerButton::Left)
            {
                return false;
            }
            state->dragging = true;
            state->selectedNode = HitTestStrategicChart(
                widget,
                event.mouseX,
                event.mouseY
            );
            return true;

        case gui::GuiCustomInputEventType::PointerMove:
            if (!state->dragging)
            {
                return false;
            }
            UpdateStrategicChartDrag(
                widget,
                event.mouseX,
                event.mouseY,
                *state
            );
            return true;

        case gui::GuiCustomInputEventType::PointerUp:
            if (event.button != gui::GuiCustomPointerButton::Left)
            {
                return false;
            }
            state->dragging = false;
            return true;

        case gui::GuiCustomInputEventType::PointerWheel:
            ZoomStrategicChart(event.wheelDelta, *state);
            return true;

        case gui::GuiCustomInputEventType::FocusLost:
        case gui::GuiCustomInputEventType::Cancel:
            state->dragging = false;
            return false;

        default:
            return false;
        }
    };

    return handler;
}
```

`DrawStrategicChart`、`HitTestStrategicChart`、`UpdateStrategicChartDrag` 和 `ZoomStrategicChart` 是示例控件自己的函数，不是解释器内置 API。Handler 的关键规则如下：

1. `handler.draw` 是必填项；缺少它时 `GuiCustomWidgetRegistry::Register` 会返回 `false`，即使该控件只想处理输入也必须提供空绘制回调。
2. `context.graphicsContext` 在 Windows D3D9 宿主中是借用的 `IDirect3DDevice9*`。
3. `context.hostContext` 由当前插件的 `CustomWidgetContext()` 提供，适合传入控件状态、纹理缓存或服务对象。
4. `event.mouseX/mouseY` 是 GUI 设计画布坐标，可直接与 `widget.rect` 比较。
5. 事件回调返回 `true` 表示事件已消费，Windows 宿主会阻止该事件继续穿透到底层 HOI3。
6. `PointerDown` 返回 `true` 会建立鼠标捕获；之后拖出控件仍会收到移动和释放事件。
7. `PointerUp`、`PointerLeave`、`FocusLost` 和 `Cancel` 属于清理事件，即使控件刚刚变为隐藏或禁用，也可能收到它们；必须在这些分支中清除拖拽、按下等临时状态。

### 14.4 第三步：由插件注册 Handler 和状态

自定义控件必须在插件初始化流程中注册：

```cpp
class StrategicGuiPlugin final : public IGuiPlugin
{
public:
    void RegisterCustomWidgets(
        gui::GuiCustomWidgetRegistry& registry
    ) override
    {
        registry.Register(
            "strategic_chart",
            CreateStrategicChartHandler()
        );
    }

    void* CustomWidgetContext() override
    {
        return &chartState_;
    }

    // 其余 IGuiPlugin 接口按插件需求实现。

private:
    StrategicChartState chartState_;
};
```

注册字符串必须与 `.sgui` 的 `customType` 完全一致。当前注册表采用同名覆盖：再次注册相同名称时，后注册的 Handler 会替换旧 Handler。

当前 `DeclarativeGuiPlugin::RegisterCustomWidgets()` 默认不注册任何 Handler。因此，新增真正的自定义控件时，需要建立实现 `IGuiPlugin` 的专用插件，或者以后为声明式插件增加可配置的 Handler 提供器；仅把 `customWidgetType` 写进使用 `factory = "declarative_gui"` 的清单不会自动得到新语义。

### 14.5 第四步：注册插件工厂并在清单中使用

在 `gui_builtin_plugins.cpp` 注册专用插件工厂：

```cpp
bool RegisterBuiltinGuiPluginFactories(
    GuiPluginRegistry& registry
)
{
    const bool declarativeRegistered = registry.RegisterFactory(
        "declarative_gui",
        CreateDeclarativeGuiPlugin
    );
    const bool strategicRegistered = registry.RegisterFactory(
        "strategic_gui",
        CreateStrategicGuiPlugin
    );
    return declarativeRegistered && strategicRegistered;
}
```

随后在 `interface/gui_plugins/<gui_name>.txt` 选择该工厂：

```text
guiPlugins = {
    guiPlugin = {
        id = "strategic_example"
        displayName = "Strategic Example"
        factory = "strategic_gui"
        startup = yes
        windowZOrder = 20
        modal = no
        maxViewportWidthRatio = 0.92
        maxViewportHeightRatio = 0.90
        cascadeOffsetX = 24
        cascadeOffsetY = 24

        options = {
            window = "strategic_example_window"
            title = "Strategic Example"
        }
    }
}
```

`maxViewportWidthRatio` 和 `maxViewportHeightRatio` 控制设计画布相对游戏客户区的最大占比；`cascadeOffsetX/Y` 控制多个同类启动窗口的初始级联偏移。它们均属于插件清单配置，不再由 Windows 宿主写死。

插件工厂只负责创建插件；控件的外观、位置、尺寸、条件和普通动作仍应放在 `.sgui/.sgfx` 中，避免重新把布局硬编码进 C++。

### 14.6 统一事件模型

`GuiCustomInputEvent::type` 可收到以下事件：

| 事件 | 含义 |
|---|---|
| `PointerMove` | 指针在控件内移动，或控件捕获鼠标后的移动。 |
| `PointerEnter` | 指针进入控件。 |
| `PointerLeave` | 指针离开控件。 |
| `PointerDown` | 鼠标按键按下；按键见 `event.button`。 |
| `PointerUp` | 鼠标按键释放。 |
| `PointerWheel` | 垂直或水平滚轮；增量见 `wheelDelta`，方向见 `horizontalWheel`。 |
| `KeyDown` | 获得焦点的控件收到按键按下；键值见 `keyCode`。 |
| `KeyUp` | 获得焦点的控件收到按键释放。 |
| `TextInput` | 文本字符输入；UTF-32 字符值见 `character`。 |
| `FocusGained` | 控件获得键盘焦点。 |
| `FocusLost` | 控件失去键盘焦点。 |
| `Cancel` | 宿主取消当前交互，控件应立即清除捕获相关状态。 |

鼠标按键类型为 `None`、`Left`、`Right`、`Middle`、`X1` 和 `X2`。`modifiers` 是位标志，可检测 Shift、Ctrl、Alt 及当前按下的各鼠标键；`repeatCount` 和 `repeated` 用于键盘重复，`horizontalWheel` 用于区分横向滚轮。

### 14.7 旧输入回调兼容

旧代码仍可填写 `handler.input`：

```cpp
handler.input = [](
    const gui::GuiResolvedWidget& widget,
    const gui::GuiCustomWidgetContext& context,
    gui::GuiCustomInputPhase phase,
    int mouseX,
    int mouseY
) -> bool {
    // 只兼容 Move、左键 Press、左键 Release。
    return false;
};
```

新控件应使用 `handler.event`。如果 `event` 和 `input` 同时存在，注册表优先调用 `event`，不会再调用旧 `input`。

### 14.8 D3D9 绘制安全规则

1. 不要对 `context.graphicsContext` 调用 `Release()`；设备所有权属于宿主。
2. 不要在 Handler 内调用 `BeginScene()`、`EndScene()`、`Present()` 或 `Reset()`。
3. 不要永久改变 Render Target、Depth Stencil、Viewport、Scissor、纹理或渲染状态；若必须改变，应完整保存并恢复。
4. 使用 `widget.rect` 作为最终绘制矩形，使用 `widget.opacity` 作为继承父级后的最终透明度。
5. `widget.hasClipRect` 为真时必须遵守 `widget.clipRect`；宿主会设置通用裁剪，但自建离屏表面或特殊绘制仍需自行保证不越界。
6. 不要在每帧 `draw` 中重复创建大型纹理、字体或顶点缓冲；把资源放入插件状态，在 `Initialize/Shutdown` 中管理生命周期。
7. `draw` 只负责绘制；游戏状态修改、Lua 动作和持久化应通过插件动作桥或数据提供器完成，不要在绘制回调中修改业务状态。

## 15. SGFX 资源类型

| 状态 | 资源语句 | 作用 |
|---|---|---|
| 现用 | `spriteTypes = { ... }` | 习惯性的资源容器；解释器会递归查找其中资源，本身没有运行时对象。 |
| 现用 | `spriteType = { ... }` | 普通图片资源。 |
| 现用 | `progressBarType = { ... }` | 进度条样式资源；类型名不区分大小写，因此 `progressbartype` 等价。 |
| 已实现未用 | `effectType = { ... }` | 安全内置 2D 效果资源；提供调色和周期脉冲，不加载任意 Shader。 |
| 现用 | `indexedMapResourceType = { ... }` | 索引地图资源及其离线生成参数、运行时着色参数。 |

**注意，当前本系统没有实现 `.sgfx` 的 `fontType`。字体由宿主直接扫描 `font` 目录，不需要也不能通过 `fontType` 注册。**

## 16. SGFX `spriteType`

```text
spriteType = {
    name = "GFX_example"
    texturefile = "gfx\\example\\image.png"
    noOfFrames = 8
    frameLayout = "horizontal"
    animation = {
        enabled = yes
        mode = "loop"
        fps = 12
        startFrame = 1
        endFrame = 8
    }
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
| 现用 | `noOfFrames = 1` | Sprite Sheet 帧数，最小为 1；各帧等宽或等高切分。 |
| 已实现未用 | `frameLayout = "horizontal/vertical"` | 帧表排列方向；默认 `horizontal`，与原版常见横向 Sprite Sheet 一致。 |
| 已实现未用 | `animation = yes/no` | 简写：启用或关闭资源默认动画；启用时使用全部帧、`loop` 和默认每帧 100 ms。 |
| 已实现未用 | `animation = { ... }` | 声明资源默认动画；块一旦存在，`enabled` 默认 `yes`。 |
| 已实现未用 | `enabled = yes/no` | `animation` 块内是否默认自动播放。 |
| 已实现未用 | `mode = "loop/pingpong/once"` | `animation` 块内的播放模式。 |
| 已实现未用 | `frameTime = 100` / `frameDuration = 100` | 每帧毫秒数；二者互为别名。 |
| 已实现未用 | `fps = 12` | 用每秒帧数指定速度；与 `frameTime/frameDuration` 互斥。 |
| 已实现未用 | `startFrame = 1` / `endFrame = 8` | 资源默认动画范围；`endFrame = 0` 表示最后一帧。 |
| 已实现未用 | `offset = 50` | 资源动画时间偏移，单位毫秒；可为负数。 |
| 已实现未用但仅解析 | `norefcount = yes` | 会保存，但当前纹理缓存不使用该标志。 |

支持的实际图片格式由平台纹理加载器决定。当前 Windows 路径通过 GDI+/D3D9 处理项目已使用的 PNG、BMP、DDS 等资源。

## 17. SGFX 进度条与内置效果

### 17.1 `progressBarType`

```text
progressBarType = {
    name = "example_progress"
    size = { x = 270 y = 22 }
    horizontal = yes
    color = { 0.27 0.71 0.37 }
    colortwo = { 0.73 0.27 0.27 }
    textureFile1 = "gfx\\example\\progress_green.dds"
    textureFile2 = "gfx\\example\\progress_red.dds"
}
```

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `name = "resource_name"` | 进度条资源名。 |
| 已实现未用 | `size = { x = 270 y = 22 }` | 解析为资源建议尺寸；当前宿主实际使用 `.sgui` 控件的 `size`。 |
| 现用 | `horizontal = yes/no` | `yes` 横向填充，`no` 纵向填充。 |
| 现用 | `color = { r g b }` | `progressColor = 0` 使用的第一颜色。 |
| 现用 | `colortwo = { r g b }` | `progressColor = 1` 使用的第二颜色。 |
| 已实现未用 | `textureFile1 = "path"` | Windows D3D9 中作为 `progressColor = 0` 的填充贴图；按当前进度同步裁切。为空或加载失败时回退 `color`。 |
| 已实现未用 | `textureFile2 = "path"` | Windows D3D9 中作为 `progressColor = 1` 的填充贴图；为空或加载失败时回退 `colortwo`。 |
| 已实现未用但仅解析 | `effectFile = "path"` | 会保存，但当前通用进度条不执行 HOI3 Effect。 |
| 忽略 | `width = 270` | 当前 `china_anti_jap.sgfx` 中存在，但解析器不读取；应使用 `size = { x = 270 y = 22 }`。 |
| 忽略 | `height = 22` | 当前解析器不读取；应使用 `size`。 |

### 17.2 `effectType`

`effectType` 是本系统自己的安全效果抽象。它只计算 RGBA 乘数并交给统一 D3D9 2D 绘制链路，不读取 Sprite 的 `effectFile`，不加载原版 HOI3 `.fx`，也不会执行配置文件指定的任意 Shader。

```text
effectType = {
    name = "GFX_warning_pulse"
    effect = "opacity_pulse"
    color = { 1.0 0.8 0.8 1.0 }
    minimum = 0.35
    maximum = 1.0
    speed = 1.5
    phase = 0
    enabled = yes
}
```

| 状态 | 语句 | 作用 |
|---|---|---|
| 已实现未用 | `name = "GFX_effect"` | 效果资源唯一名称。 |
| 已实现未用 | `effect = "tint"` | 固定乘以 `color` 的 RGBA。 |
| 已实现未用 | `effect = "pulse"` / `"brightness_pulse"` | 在 `minimum..maximum` 间以正弦波改变 RGB 亮度，并乘以 `color`；两种名称等价。 |
| 已实现未用 | `effect = "opacity_pulse"` | 保持 `color.rgb` 调色，在 `minimum..maximum` 间周期改变 Alpha。 |
| 已实现未用 | `effect = "color_pulse"` | 在白色与 `color.rgb` 之间周期插值。 |
| 已实现未用 | `color = { r g b a }` | 效果颜色乘数，四个分量范围均为 `0~1`；默认全白。 |
| 已实现未用 | `minimum = 0.5` | 周期效果最小值，范围 `0~1`，不得大于 `maximum`。 |
| 已实现未用 | `maximum = 1.0` | 周期效果最大值，范围 `0~1`。 |
| 已实现未用 | `speed = 1.0` | 每秒周期数，范围 `0~100`；`0` 表示固定在由 `phase` 决定的位置。 |
| 已实现未用 | `phase = 0` | 初始相位，单位为度。 |
| 已实现未用 | `enabled = yes/no` | 关闭时返回全白乘数，相当于不应用效果。 |

控件通过以下字段使用效果：

| 状态 | 语句 | 作用 |
|---|---|---|
| 已实现未用 | `effectType = "GFX_effect"` | 固定引用效果资源；`effectResource` 是等价别名。 |
| 已实现未用 | `effectSource = "data.path"` | 动态读取效果资源名；列表模板支持 `item.field` 和 `{id}`，空值回退固定 `effectType`。 |
| 已实现未用 | `effectTimeSource = "state.time_ms"` | 使用数据值作为效果时钟，单位毫秒；未设置时使用窗口会话时钟。 |

效果可用于窗口框、图片、文字、按钮、色块、进度条、滚动条和索引地图。Custom 与 MarkerLayer 拥有自己的专用绘制器，不隐式套用内置效果。

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

## 20. 使用本系统新增 GUI 的最小文件集合

### 20.1 纯 2D 静态或文件数据 GUI

1. `interface/<gui_name>.sgui`：窗口、控件、坐标、条件和动作名。
2. `interface/<gui_name>.sgfx`：图片、进度条\其他索引资源或索引地图资源。
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

## 21. 易误用字段

1. `fullScreen`：只绑定作为会话根窗口运行的 `windowType`；同一声明作为其他窗口的子控件时不会把父窗口替换为客户区。
2. `width`、`height`（SGFX 进度条资源）：当前完全忽略，改用 `size = { x = ... y = ... }`。
3. `positionType` 字符串引用的名称区分大小写；未找到资源时回退直接 `position`。`orientation` 未知值会按 `UPPER_LEFT` 布局并产生严格 Schema 诊断。
4. `scaleMode`：未知值会回退为 `stretch` 并写入加载诊断；九宫格启用时优先于 `scaleMode`。
5. `delayedTooltipText`：只是静态文本别名，不会自行产生延迟；必须另设 `tooltipDelay`。
6. `frame`：使用原版的 1 基编号，不是 0 基；`frameSource` 存在时会覆盖自动动画和静态 `frame`。
7. `animationTimeSource`：值的单位是毫秒，不是秒；资源与控件的 `offset` 会叠加。
8. `effectFile`、`loadType`、`norefcount`：Sprite 中仍只保存元数据，当前自定义渲染器不执行相应 HOI3 语义。
9. `textureFile1/2`（进度条）：是两套可选填充外观，不是背景与前景；背景仍应由独立 `iconType` 提供。
10. `renderMode = "custom"`：只抑制通用文字绘制，不会自动获得自定义绘制。
11. `customWidgetType`：必须有 C++ Handler，不能仅靠配置创造全新控件语义。
12. `scale` 是图片适配模式 `scaleMode` 的别名，不是几何缩放；几何缩放必须使用 `transformScale` 或动态 `scaleSource`。
13. 通用几何变换只作用于当前普通 2D 叶控件，不会连带变换其子控件；根窗口、列表、索引地图、MarkerLayer 和 Custom 不使用该变换。
14. `effectType` 是安全内置颜色效果资源；它与原版 Sprite 的 `effectFile` 无关，不能填写 `.fx` 路径或自定义 Shader 名。

## 22. SGUI 窗口静态数据

窗口可以直接声明只属于该界面的少量静态标量和静态列表。它们在每次数据刷新后由会话重新合并，因此不会被 Lua 的完整快照意外清除。可复用或便于扩展的业务目录更适合写入 `script_gui/data`，通过插件的 `base_data` 合并；动态游戏状态仍由数据提供器或 Lua 发布。

```gui
windowType = {
    name = "example_window"

    dataValueType = {
        name = "buttonsprites.CHI"
        value = "GFX_button_chi"
    }

    dataListType = {
        name = "officer_catalog"
        revision = 1

        item = {
            id = 1
            text = "OFFICER_LIST_ITEM_1"
            role = "military"
            portrait = "GFX_officer_1"
            namekey = "OFFICER_NAME_1"
            descriptionkey = "OFFICER_DESC_1"
            enabledwhen = "state.active"
        }
    }
}
```

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `dataValueType = { name = "path" value = "..." }` | 在当前窗口声明一个静态数据路径。布尔、整数、浮点和字符串会自动推断。 |
| 已实现未用 | `type = "string/bool/int/number"` | 强制指定 `dataValueType` 的值类型；类型和值不匹配时产生加载诊断。 |
| 现用 | `dataListType = { name = "list" ... }` | 在当前窗口声明静态列表。列表名进入统一 `GuiDataRegistry`。 |
| 现用 | `revision = 1` | 静态列表修订号，必须是非负整数。 |
| 现用 | `item = { id = 1 ... }` | 声明列表项；`id` 必须唯一且大于零，省略时按出现顺序自动分配。 |
| 现用 | `text = "KEY"` | 设置列表项主文本，同时保留为 `item.text` 字段。 |
| 现用 | 任意标量字段 | 以小写键保存为列表项数据，可通过 `item.field` 读取，也会进入动作参数。 |

### 22.1 Marker 静态目录补全

动态 Marker 列表只需要发布运行时字段；头像、姓名和说明等静态字段可从另一个列表按相同 `item.id` 补全：

```gui
markerLayerType = {
    name = "officer_markers"
    dataSource = "assigned_officers"
    catalogSource = "officer_catalog"
    regionSource = "item.regionid"
    portraitSource = "item.portrait"
    nameSource = "item.namekey"
    descriptionSource = "item.descriptionkey"
}
```

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `catalogSource = "list_name"` | 按 Marker 动态项的 `id` 查找静态目录项；动态项缺少字段时使用目录字段。 |
| 已实现未用 | `itemCatalog` / `fallbackDataSource` | `catalogSource` 的别名。 |

字段优先级为“动态项 > 静态目录项”。Marker 事件会先附加目录字段，再由动态字段覆盖，因此 Lua 动作既能收到静态身份参数，也能收到最新的 Region、坐标和顺序。

### 22.2 Bridge 基础数据合并

实时 Lua Bridge 可以在每个完整快照下方保留一份声明式基础数据。基础数据先加载，Lua 快照随后覆盖同名键；Lua 没有发布的静态列表不会因完整快照刷新而消失。

```gui
options = {
    inprocess_data_provider = "bridge"
    inprocess_channel = "lua"
    inprocess_bridge_name = "example_gui"
    inprocess_base_data = "script_gui/data/example_common.txt"
}
```

| 状态 | 语句 | 作用 |
|---|---|---|
| 现用 | `inprocess_base_data = "path"` | 在 Windows 进程内 Bridge 初始化时加载基础数据文件，并在每次 Lua 完整快照前重新作为底层数据。 |
| 已实现未用 | `base_data` / `base_path` | 直接创建 Bridge 数据提供器时使用的同义选项；插件清单的进程内选项需要添加 `inprocess_` 前缀。 |

战争地图主官目录当前保存在 `script_gui/data/china_anti_jap_common.txt`。每项通过 `tag = "CHI"` 等字段声明所属国家，`leader_candidate_list` 使用 `itemFilterField = "tag"` 和 `itemFilterValueSource = "state.viewertag"` 只显示当前玩家国家可用的主官。
