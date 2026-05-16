# 练习 8：全功能文本编辑器

使用 RichEdit 2.0 控件实现一个完整的文本编辑器，综合练习菜单栏、工具栏、状态栏、加速键和文件操作。

涉及知识点：RichEdit 控件、菜单栏（CreateMenu/AppendMenu）、工具栏（Toolbar）、状态栏（StatusBar）、运行时加速键表（CreateAcceleratorTable）、通用文件对话框（GetOpenFileName/GetSaveFileName）、UTF-8/UTF-16 文件编解码（BOM 检测、MultiByteToWideChar）

## 功能

- **菜单栏**：文件（新建/打开/保存/另存为/退出）、编辑（撤销/剪切/复制/粘贴/全选）、帮助（关于）
- **工具栏**：新建/打开/保存/剪切/复制/粘贴（使用标准系统位图）
- **状态栏**：行号、列号、文件名、修改状态
- **编辑区**：RichEdit 2.0 控件
- **快捷键**：Ctrl+N/O/S/Z/X/C/V/A
- **文件操作**：支持 UTF-8（带/不带 BOM）和 UTF-16 LE/BE 文件的打开与保存
- **未保存确认**：关闭窗口或新建/打开文件时，若有未保存修改会弹出确认对话框

## 构建步骤

```bash
# 在 src/tutorial/native_win32 目录下
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

可执行文件位于 `build/bin/08_text_editor.exe`。
