HOI3 Leader Capture RC1.7 - Windows build fix

本版只修复 RC1.6 的 Windows 批处理构建问题。

根因：RC1.6 的 BAT 使用 LF-only 换行；Windows cmd.exe 在用户环境中错误解析，导致 setlocal/VSWHERE/errorlevel 等命令被截断。

RC1.7：
- BUILD_RC1_7.bat：纯 ASCII + Windows CRLF。
- START_LEADER_CAPTURE.bat：纯 ASCII + Windows CRLF。
- 默认目标进程仍为 hoi3_tfh.exe。
- R06B 机制源码未修改。
- 不恢复 D328 SHA 硬门。
- 构建后自检 launcher 内不得出现 hoi3_tfh_GAME.exe 或 D328 SHA。

使用：
1. 解压到任意目录。
2. 双击 BUILD_RC1_7.bat。
3. 必须看到 BUILD PASS。
4. 启动 hoi3_tfh.exe，进入主菜单。
5. 双击 release\START_LEADER_CAPTURE.bat。

如果 BUILD_RC1_7.bat 明确提示没有 Visual Studio C++ x86 tools，则这是工具链缺失，不再是 BAT 解析错误。
