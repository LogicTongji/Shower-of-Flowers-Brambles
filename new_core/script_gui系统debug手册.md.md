# Scripted GUI（HOI3 / Windows D3D9）测试与 Debug 终端命令手册

> 适用于当前工程：
>
> - MOD / 工程根目录：`D:\80th_special_version\Shower-of-Flowers-Brambles`
> - Overlay：`D:\80th_special_version\Shower-of-Flowers-Brambles\overlay`
> - HOI3：`D:\80th_special_version\hoi3\hoi3_tfh.exe`
> - 构建架构：Win32 / x86
> - 默认测试配置：Debug
>
> 以下命令均以 **PowerShell** 为准。

---

## 0. 建议先定义这些路径变量

以后大多数命令都可以直接复制使用。

```powershell
$ProjectRoot = "D:\80th_special_version\Shower-of-Flowers-Brambles"
$OverlayRoot = "$ProjectRoot\overlay"
$Hoi3Root    = "D:\80th_special_version\hoi3"
$Hoi3Exe     = "$Hoi3Root\hoi3_tfh.exe"

$BuildRoot   = "$OverlayRoot\build-win"
$DebugDir    = "$BuildRoot\Debug"

$Injector    = "$DebugDir\scripted_gui_injector.exe"
$OverlayDll  = "$DebugDir\hoi3_new_core.dll"

$DevModSrc   = "$BuildRoot\hoi3_scripted_gui_dev.mod"
$DevModDst   = "$Hoi3Root\mod\hoi3_scripted_gui_dev.mod"
```

检查这些路径：

```powershell
$ProjectRoot
$OverlayRoot
$Hoi3Exe
$Injector
$OverlayDll
```

检查关键文件是否存在：

```powershell
Test-Path $Hoi3Exe
Test-Path $Injector
Test-Path $OverlayDll
```

正常应返回：

```text
True
True
True
```

---

# 1. 处理 PowerShell / MSBuild 中文乱码

如果终端出现：

```text
宸插畬鎴愮敓鎴愰」鐩
```

之类乱码，可先执行：

```powershell
chcp 65001
```

然后：

```powershell
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8
```

建议每次打开新的 Debug PowerShell 后先执行一次。

---

# 2. 进入工程目录

进入工程根目录：

```powershell
cd "D:\80th_special_version\Shower-of-Flowers-Brambles"
```

进入 Overlay：

```powershell
cd "D:\80th_special_version\Shower-of-Flowers-Brambles\overlay"
```

查看当前目录：

```powershell
Get-Location
```

查看当前目录文件：

```powershell
Get-ChildItem
```

---

# 3. 正常 Debug 编译

## 3.1 只编译，不运行 CTest

在工程根目录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File ".\overlay\build_windows.ps1" `
    -Configuration Debug `
    -SkipTests
```

在 `overlay` 目录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File ".\build_windows.ps1" `
    -Configuration Debug `
    -SkipTests
```

这是开发阶段最常用的编译命令。

---

## 3.2 编译并运行全部测试

工程根目录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File ".\overlay\build_windows.ps1" `
    -Configuration Debug
```

Overlay 目录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File ".\build_windows.ps1" `
    -Configuration Debug
```

正常情况下应看到：

```text
100% tests passed
```

当前工程应有约 19 个 probe 测试。

---

# 4. 找到 Visual Studio 自带的 CMake / CTest

如果直接执行：

```powershell
cmake
```

出现：

```text
无法将“cmake”项识别为 cmdlet
```

说明 `cmake.exe` 没在系统 PATH 中。

使用以下代码自动寻找 Visual Studio 2022 的 CMake：

```powershell
$CMake = @(
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

$CMake
```

CTest 与 CMake 在同一目录：

```powershell
$CTest = Join-Path (Split-Path $CMake) "ctest.exe"
$CTest
```

确认版本：

```powershell
& $CMake --version
& $CTest --version
```

---

# 5. C++ 编译失败时的定位方法

如果最后只看到：

```text
CMake build failed with exit code 1.
```

这只是汇总错误。

真正错误通常在前面，例如：

```text
error C2039
error C2065
error C2660
error C2664
error C2146
LNK2019
```

---

## 5.1 直接进行详细构建并保存日志

先进入 Overlay：

```powershell
cd $OverlayRoot
```

然后：

```powershell
& $CMake --build --preset windows-x86-debug --verbose *> ".\build_full.log"
```

查看所有真实 C++ / Linker 错误：

```powershell
Get-Content ".\build_full.log" |
    Select-String `
        -Pattern "error C[0-9]{4}|fatal error|LNK[0-9]{4}|MSB[0-9]{4}"
```

只看第一条错误及上下文：

```powershell
Get-Content ".\build_full.log" |
    Select-String `
        -Pattern "error C[0-9]{4}|fatal error|LNK[0-9]{4}|MSB[0-9]{4}" `
        -Context 5,10 |
    Select-Object -First 1
```

---

## 5.2 用工程脚本编译，同时保存输出

工程根目录：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File ".\overlay\build_windows.ps1" `
    -Configuration Debug `
    -SkipTests 2>&1 |
    Tee-Object ".\build_error.txt"
```

筛选错误：

```powershell
Get-Content ".\build_error.txt" |
    Select-String `
        -Pattern "error C|fatal error|LNK|MSB" `
        -Context 5,5
```

---

# 6. CTest 测试失败定位

如果看到：

```text
CTest failed with exit code 8.
```

说明：

> 编译已经成功，但至少有一个测试失败。

---

## 6.1 显示所有测试以及失败输出

```powershell
cd $OverlayRoot

& $CTest `
    --test-dir ".\build-win" `
    -C Debug `
    --output-on-failure
```

正常输出类似：

```text
1/19 Test #1 ... Passed
...
19/19 Test #19 ... Passed
```

---

## 6.2 只重新运行失败测试

```powershell
& $CTest `
    --test-dir ".\build-win" `
    -C Debug `
    --rerun-failed `
    --output-on-failure `
    -V
```

`-V` 会输出详细过程。

---

## 6.3 查看 CTest 上一次完整日志

```powershell
Get-Content ".\build-win\Testing\Temporary\LastTest.log"
```

筛选失败：

```powershell
Select-String `
    -Path ".\build-win\Testing\Temporary\LastTest.log" `
    -Pattern "Test Failed|FAILED|Failed|Error|exit code" `
    -Context 8,20
```

---

## 6.4 直接运行单个 Probe

例如：

```powershell
.\build-win\Debug\gui_render_queue_probe.exe
```

```powershell
.\build-win\Debug\gui_interpreter_probe.exe
```

```powershell
.\build-win\Debug\gui_window_session_probe.exe
```

```powershell
.\build-win\Debug\gui_host_d3d9_probe.exe
```

如果不知道有哪些 Probe：

```powershell
Get-ChildItem ".\build-win\Debug\*_probe.exe"
```

---

# 7. 清理后重新完整编译

当怀疑旧 `.obj`、旧 CMake cache、旧生成文件干扰时使用。

⚠️ 会删除整个 Windows 构建目录：

```powershell
Remove-Item "$OverlayRoot\build-win" -Recurse -Force
```

然后重新：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File "$OverlayRoot\build_windows.ps1" `
    -Configuration Debug
```

开发过程中不要每次都 Clean Build；只有怀疑缓存问题时再用。

---

# 8. 准备 HOI3 MOD 描述文件

当前开发 MOD 描述文件：

```text
overlay\build-win\hoi3_scripted_gui_dev.mod
```

其中应该类似：

```text
name = "Scripted GUI Development"
path = "D:/80th_special_version/Shower-of-Flowers-Brambles"
user_dir = "scripted_gui_development"
```

HOI3 的工作目录是：

```text
D:\80th_special_version\hoi3
```

因此最稳妥的方式是把 `.mod` 描述文件复制到：

```text
D:\80th_special_version\hoi3\mod\
```

执行：

```powershell
Copy-Item `
    $DevModSrc `
    $DevModDst `
    -Force
```

检查：

```powershell
Test-Path $DevModDst
```

应返回：

```text
True
```

查看内容：

```powershell
Get-Content $DevModDst
```

注意：

> 只需要复制 `.mod` 描述文件。
>
> 真正的 MOD 内容仍然位于：
>
> `D:\80th_special_version\Shower-of-Flowers-Brambles`

---

# 9. 使用 Injector 启动 HOI3 + Script GUI + MOD

先确认文件：

```powershell
Test-Path $Injector
Test-Path $OverlayDll
Test-Path $Hoi3Exe
Test-Path $DevModDst
```

然后：

```powershell
& $Injector `
    $Hoi3Exe `
    $OverlayDll `
    $ProjectRoot `
    "-mod" `
    "mod/hoi3_scripted_gui_dev.mod"
```

Injector 应最终打印类似：

```text
HOI3 command line: ...
HOI3 started with Scripted GUI, pid=12345
```

其中 HOI3 command line 应包含：

```text
-mod=mod/hoi3_scripted_gui_dev.mod
```

---

# 10. 一条龙：编译 → 测试 → 复制 MOD → 启动 HOI3

适合确认代码稳定后使用。

```powershell
$ProjectRoot = "D:\80th_special_version\Shower-of-Flowers-Brambles"
$OverlayRoot = "$ProjectRoot\overlay"
$Hoi3Root    = "D:\80th_special_version\hoi3"
$Hoi3Exe     = "$Hoi3Root\hoi3_tfh.exe"

powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File "$OverlayRoot\build_windows.ps1" `
    -Configuration Debug

Copy-Item `
    "$OverlayRoot\build-win\hoi3_scripted_gui_dev.mod" `
    "$Hoi3Root\mod\hoi3_scripted_gui_dev.mod" `
    -Force

& "$OverlayRoot\build-win\Debug\scripted_gui_injector.exe" `
    $Hoi3Exe `
    "$OverlayRoot\build-win\Debug\hoi3_new_core.dll" `
    $ProjectRoot `
    "-mod" `
    "mod/hoi3_scripted_gui_dev.mod"
```

---

# 11. 快速开发循环：只编译 → 启动

如果 CTest 已经全部通过，日常修改小功能时可以：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File "$OverlayRoot\build_windows.ps1" `
    -Configuration Debug `
    -SkipTests
```

然后：

```powershell
& $Injector `
    $Hoi3Exe `
    $OverlayDll `
    $ProjectRoot `
    "-mod" `
    "mod/hoi3_scripted_gui_dev.mod"
```

推荐开发流程：

```text
修改代码
  ↓
Debug Build -SkipTests
  ↓
HOI3 实机验证
  ↓
阶段性功能完成
  ↓
完整 CTest
```

---

# 12. 检查 HOI3 是否正在运行

```powershell
Get-Process hoi3_tfh -ErrorAction SilentlyContinue
```

只看 PID：

```powershell
(Get-Process hoi3_tfh -ErrorAction SilentlyContinue).Id
```

结束 HOI3：

```powershell
Stop-Process -Name hoi3_tfh -Force
```

⚠️ `-Force` 会直接终止游戏，未保存进度会丢失。

---

# 13. 检查 Script GUI DLL 是否已加载

HOI3 运行后，可尝试：

```powershell
$Hoi3Process = Get-Process hoi3_tfh -ErrorAction SilentlyContinue

$Hoi3Process.Modules |
    Where-Object {
        $_.ModuleName -like "*scripted_gui*"
    } |
    Select-Object ModuleName, FileName
```

正常应能看到类似：

```text
hoi3_new_core.dll
```

如果 PowerShell 因权限或位数问题无法枚举模块，可尝试以管理员身份运行 PowerShell。

---

# 14. 查找 Script GUI 日志

如果不知道日志实际写在哪里：

```powershell
Get-ChildItem `
    $ProjectRoot,$Hoi3Root `
    -Filter "scripted_gui_overlay.log" `
    -Recurse `
    -ErrorAction SilentlyContinue |
    Select-Object FullName, LastWriteTime, Length
```

找到以后：

```powershell
Get-Content "实际日志路径\scripted_gui_overlay.log"
```

实时监控日志：

```powershell
Get-Content "实际日志路径\scripted_gui_overlay.log" -Wait
```

看最后 100 行：

```powershell
Get-Content "实际日志路径\scripted_gui_overlay.log" -Tail 100
```

筛选常见错误：

```powershell
Get-Content "实际日志路径\scripted_gui_overlay.log" |
    Select-String `
        -Pattern "error|failed|exception|warning|reset|hook|lua|d3d9"
```

---

# 15. 最近修改代码后快速找文件

查某个符号：

```powershell
Get-ChildItem "$OverlayRoot\src" -Recurse -File |
    Select-String "DrawSprite"
```

例如：

```powershell
Get-ChildItem "$OverlayRoot\src" -Recurse -File |
    Select-String "GuiImageScaleMode"
```

```powershell
Get-ChildItem "$OverlayRoot\src" -Recurse -File |
    Select-String "nineSlice"
```

```powershell
Get-ChildItem "$OverlayRoot\src" -Recurse -File |
    Select-String "opacity"
```

显示上下文：

```powershell
Get-ChildItem "$OverlayRoot\src" -Recurse -File |
    Select-String "DrawTextureRegion" -Context 5,10
```

---

# 16. 查看某个源码文件指定行附近

例如看 `gui_host_d3d9.cpp` 第 1420 行附近：

```powershell
Get-Content "$OverlayRoot\src\gui_host_d3d9.cpp" |
    Select-Object -Skip 1410 -First 40
```

如果编译器报：

```text
gui_host_d3d9.cpp(736,13)
```

可以：

```powershell
Get-Content "$OverlayRoot\src\gui_host_d3d9.cpp" |
    Select-Object -Skip 720 -First 35
```

注意：

> PowerShell `Select-Object -Skip` 是从 0 开始近似跳行，
> 所以为了看到上下文，通常比报错行少跳 10～20 行。

---

# 17. Git Diff（如果工程使用 Git）

查看修改：

```powershell
git status
```

查看全部差异：

```powershell
git diff
```

只看某个文件：

```powershell
git diff -- new_core/src/gui_host_d3d9.cpp
```

查看最近提交：

```powershell
git log --oneline -10
```

建议每完成一个稳定阶段做一次 commit，例如：

```text
scaleMode
opacity
nineSlice
tooltip
```

分别提交，方便回滚。

---

# 18. 当前三个新功能的实机测试重点

## scaleMode

测试：

```text
stretch
contain
center
```

建议用非正方形 DDS，例如：

```text
256 × 128
```

放到：

```text
200 × 200
```

控件中观察。

预期：

```text
stretch
→ 拉伸到整个 Widget

contain
→ 保持宽高比，完整显示

center
→ 保持原始像素尺寸并居中，超出部分裁切
```

---

## opacity

测试：

```text
opacity = 1.0
opacity = 0.5
opacity = 0.0
```

再测试：

```text
parent.opacity = 0.5
child.opacity  = 0.5
```

Child 最终应约等于：

```text
0.25
```

还应测试：

```text
Button
Text
ColorBox
ProgressBar
IndexedMap
Scrollbar
MarkerLayer
ListBox Item
```

---

## nineSlice

建议测试纹理：

```text
100 × 100
```

设置：

```text
left   = 20
top    = 20
right  = 20
bottom = 20
```

Widget：

```text
500 × 300
```

预期：

```text
四角不变形
上下只横向拉伸
左右只纵向拉伸
中心双向拉伸
```

边界测试：

```text
Widget width = 30

left  = 20
right = 20
```

因为：

```text
20 + 20 > 30
```

此时应触发 `FitSlicePair()`，不能出现负宽度、纹理翻转、巨大三角形或崩溃。

---

# 19. 出问题时建议按这个顺序排查

```text
1. C++ 是否成功编译？
        ↓
2. CTest 是否 19/19？
        ↓
3. injector.exe / overlay.dll 是否存在？
        ↓
4. .mod 描述文件是否已复制到 hoi3\mod？
        ↓
5. Injector 打印的 HOI3 command line 是否包含 -mod=...？
        ↓
6. HOI3 是否真的加载了 Shower-of-Flowers-Brambles？
        ↓
7. hoi3_new_core.dll 是否加载？
        ↓
8. scripted_gui_overlay.log 是否有错误？
        ↓
9. 原有 Script GUI 是否正常？
        ↓
10. 再单独测试新功能
```

---

# 20. 最常用命令速查

## 编译，不测试

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File "$OverlayRoot\build_windows.ps1" `
    -Configuration Debug `
    -SkipTests
```

## 完整编译测试

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File "$OverlayRoot\build_windows.ps1" `
    -Configuration Debug
```

## CTest 显示错误

```powershell
& $CTest `
    --test-dir "$BuildRoot" `
    -C Debug `
    --output-on-failure
```

## 重跑失败测试

```powershell
& $CTest `
    --test-dir "$BuildRoot" `
    -C Debug `
    --rerun-failed `
    --output-on-failure `
    -V
```

## 复制 MOD 描述文件

```powershell
Copy-Item $DevModSrc $DevModDst -Force
```

## Injector 启动

```powershell
& $Injector `
    $Hoi3Exe `
    $OverlayDll `
    $ProjectRoot `
    "-mod" `
    "mod/hoi3_scripted_gui_dev.mod"
```

## 查看 HOI3 进程

```powershell
Get-Process hoi3_tfh -ErrorAction SilentlyContinue
```

## 搜索 Script GUI 日志

```powershell
Get-ChildItem `
    $ProjectRoot,$Hoi3Root `
    -Filter "scripted_gui_overlay.log" `
    -Recurse `
    -ErrorAction SilentlyContinue
```

---

# 21. 推荐的日常工作方式

小改动：

```text
修改代码
→ Debug Build -SkipTests
→ Injector
→ HOI3 实机测试
```

一个功能完成后：

```text
完整 Debug Build
→ CTest 19/19
→ 实机测试
→ Git commit
```

出现崩溃或异常：

```text
先看第一条编译错误 / 第一个失败 Probe / Overlay 日志第一条异常
```

不要优先处理最后一条：

```text
CMake build failed with exit code 1
CTest failed with exit code 8
```

因为这些只是汇总结果，不是真正根因。
