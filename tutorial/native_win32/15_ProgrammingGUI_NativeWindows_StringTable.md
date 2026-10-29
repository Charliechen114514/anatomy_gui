# 通用GUI编程技术——Win32 原生编程实战（十四）——字符串表与国际化

> 在之前的文章里，我们聊过如何用资源文件定义图标、光标和位图。但你可能已经注意到一个问题：我们的代码里到处都是硬编码的中文字符串。这不仅让代码看起来很乱，更重要的是，如果你的程序需要支持多语言，这种写法会让你陷入维护噩梦。今天我们要聊的就是这个问题的解决方案——字符串表资源，以及如何用它来实现程序的国际化。

---

## 为什么要关心字符串表和国际化

说实话，在我刚开始做项目的时候，对国际化这事儿基本上是无所谓的态度。觉得反正都是给国内用户用，中文就够了，何必折腾那些英文翻译？但后来有几个现实问题狠狠地教育了我。

首先是字符串管理的问题。当一个项目稍微大一点之后，你会发现同样的字符串可能出现在多个地方。比如"文件未找到"这个错误提示，可能在文件打开模块、导入导出模块、拖放处理模块都要用到。如果用硬编码字符串，要么每次都重新写一遍（容易打错字），要么复制粘贴（万一要改提示文字就要改好多地方）。更糟糕的是，如果产品经理突然决定把"文件未找到"改成"无法定位文件"，你得在整个项目里搜索替换，祈祷不要漏掉什么或者改错什么。

第二个现实问题是代码可读性。当代码里充斥着各种中文字符串时，代码会变得很难读：

```cpp
// 这样写很混乱
if (error == ERROR_FILE_NOT_FOUND)
{
    MessageBox(hwnd, L"文件未找到，请检查文件路径是否正确", L"错误", MB_OK | MB_ICONERROR);
}
```

而使用字符串资源之后，代码会变得更清晰：

```cpp
// 这样写更清晰
if (error == ERROR_FILE_NOT_FOUND)
{
    MessageBox(hwnd, LoadStringDx(hInstance, IDS_ERROR_FILE_NOT_FOUND),
        LoadStringDx(hInstance, IDS_ERROR_TITLE), MB_OK | MB_ICONERROR);
}
```

第三个也是最重要的原因，就是国际化。当你的程序需要面向海外用户时，硬编码字符串会让你陷入绝望。你需要把所有字符串提取出来、翻译、重新编译、测试、发布。而使用字符串表的话，只需要创建一个新的资源 DLL，包含翻译后的字符串，程序无需修改就能支持新语言。

这篇文章会带着你从零开始，把 Win32 的字符串表和国际化机制彻底搞透。我们不只是学会怎么用，更重要的是理解"为什么要这么用"。

---

## 环境说明

在我们正式开始之前，先明确一下我们这次动手的环境：

* **平台**：Windows 10/11（理论上 Windows Vista+ 都支持）
* **开发工具**：Visual Studio 2019 或更高版本
* **编程语言**：C++（C++17 或更新）
* **项目类型**：桌面应用程序（Win32 项目）

代码假设你已经熟悉前面文章的内容——至少知道怎么创建一个基本的窗口、资源文件是怎么回事、怎么处理基本的窗口消息。如果这些概念对你来说还比较陌生，建议先去看看前面的笔记。

---

## 第一步——理解 STRINGTABLE 资源

STRINGTABLE 是 Win32 资源系统中专门用于存储字符串的资源类型。它本质上是一个字符串数组，每个字符串有一个数字 ID，程序可以通过这个 ID 来加载对应的字符串。

### STRINGTABLE 的语法

在 .rc 文件中定义字符串表非常简单：

```rc
#include <windows.h>
#include "resource.h"

STRINGTABLE
BEGIN
    IDS_APP_NAME           "我的应用程序"
    IDS_FILE_MENU          "文件"
    IDS_EDIT_MENU          "编辑"
    IDS_HELP_MENU          "帮助"
    IDS_OK                 "确定"
    IDS_CANCEL             "取消"
    IDS_YES                "是"
    IDS_NO                 "否"
    IDS_ERROR_TITLE        "错误"
    IDS_WARNING_TITLE      "警告"
    IDS_INFO_TITLE         "信息"
    IDS_ERROR_FILE_NOT_FOUND   "文件未找到，请检查文件路径是否正确"
    IDS_ERROR_ACCESS_DENIED    "访问被拒绝，您可能没有足够的权限"
    IDS_ERROR_OUT_OF_MEMORY    "内存不足"
    IDS_CONFIRM_DELETE         "确定要删除这个文件吗？"
    IDS_CONFIRM_EXIT           "您有未保存的更改，确定要退出吗？"
END
```

每个字符串的 ID 通常在 resource.h 中定义：

```cpp
// 字符串 ID 定义（建议从 1000 开始，避免和其他资源冲突）
#define IDS_APP_NAME                1000
#define IDS_FILE_MENU               1001
#define IDS_EDIT_MENU               1002
#define IDS_HELP_MENU               1003
#define IDS_OK                      1004
#define IDS_CANCEL                  1005
#define IDS_YES                     1006
#define IDS_NO                      1007
#define IDS_ERROR_TITLE             1008
#define IDS_WARNING_TITLE           1009
#define IDS_INFO_TITLE              1010
#define IDS_ERROR_FILE_NOT_FOUND    1011
#define IDS_ERROR_ACCESS_DENIED     1012
#define IDS_ERROR_OUT_OF_MEMORY     1013
#define IDS_CONFIRM_DELETE          1014
#define IDS_CONFIRM_EXIT            1015
```

### STRINGTABLE 的限制

在使用 STRINGTABLE 之前，需要了解它的一些限制：

1. **字符串长度限制**：每个字符串最多 4097 个字符（包括 null 终止符）
2. **必须使用连续 ID**：资源编译器要求 STRINGTABLE 中的 ID 连续，或者至少在同一个段内连续
3. **不能包含特殊字符**：不能直接在字符串中嵌入双引号、换行符等，需要使用转义序列
4. **区分大小写**：资源 ID 是区分大小写的

对于换行等特殊字符，可以使用转义序列：

```rc
STRINGTABLE
BEGIN
    IDS_MULTI_LINE        "第一行\n第二行\n第三行"
    IDS_WITH_QUOTE        "这是\"引号\"内的文字"
    IDS_WITH_TAB          "列1\t列2\t列3"
END
```

---

## 第二步——使用 LoadString 加载字符串

`LoadString` 函数用于从字符串表中加载字符串：

```cpp
int LoadString(
    HINSTANCE hInstance,  // 应用程序实例句柄
    UINT uID,             // 字符串 ID
    LPSTR lpBuffer,       // 接收缓冲区
    int cchBufferMax      // 缓冲区大小
);
```

### 基本用法

```cpp
wchar_t szBuffer[256];
LoadString(hInstance, IDS_ERROR_FILE_NOT_FOUND, szBuffer, 256);
MessageBox(hwnd, szBuffer, L"提示", MB_OK);
```

### 处理长字符串

如果你不确定字符串的长度，可以先用 `LoadString` 的返回值来检查：

```cpp
// 首先尝试加载，获取实际长度
int nLength = LoadString(hInstance, IDS_SOME_LONG_STRING, NULL, 0);

if (nLength > 0)
{
    // 分配足够的缓冲区（+1 用于 null 终止符）
    wchar_t* pBuffer = new wchar_t[nLength + 1];
    LoadString(hInstance, IDS_SOME_LONG_STRING, pBuffer, nLength + 1);

    // 使用字符串...
    MessageBox(hwnd, pBuffer, L"提示", MB_OK);

    delete[] pBuffer;
}
```

### 创建一个方便的包装函数

每次调用 `LoadString` 都要准备缓冲区确实有点麻烦，我们可以写一个包装函数：

```cpp
// 从资源加载字符串的便利函数
CString LoadStringDx(HINSTANCE hInstance, UINT uID)
{
    CString str;
    int nLength = ::LoadString(hInstance, uID, str.GetBuffer(512), 512);
    if (nLength > 0)
    {
        str.ReleaseBuffer();
    }
    else
    {
        str.ReleaseBuffer(0);
    }
    return str;
}

// 或者使用 std::wstring
std::wstring LoadStringW(HINSTANCE hInstance, UINT uID)
{
    // 首先获取字符串长度
    int nLength = LoadStringW(hInstance, uID, NULL, 0);

    if (nLength > 0)
    {
        std::wstring buffer(nLength, L'\0');
        LoadStringW(hInstance, uID, &buffer[0], nLength + 1);
        return buffer;
    }

    return std::wstring();
}
```

---

## 第三步——在程序中使用字符串表

让我们把前面文章中的示例程序改造成使用字符串表。这样做的好处是代码更清晰，而且以后要支持多语言会非常方便。

### 资源文件

```rc
#include <windows.h>
#include "resource.h"

// 字符串表
STRINGTABLE
BEGIN
    // 应用程序名称和标题
    IDS_APP_TITLE         "字符串表示例程序"
    IDS_APP_NAME          "StringTableDemo"

    // 菜单项
    IDS_MENU_FILE         "文件"
    IDS_MENU_EDIT         "编辑"
    IDS_MENU_HELP         "帮助"

    // 菜单命令
    IDS_CMD_NEW           "新建"
    IDS_CMD_OPEN          "打开..."
    IDS_CMD_SAVE          "保存"
    IDS_CMD_EXIT          "退出"
    IDS_CMD_ABOUT         "关于..."

    // 按钮文本
    IDS_OK                "确定"
    IDS_CANCEL            "取消"

    // 消息框标题
    IDS_TITLE_INFO        "信息"
    IDS_TITLE_WARNING     "警告"
    IDS_TITLE_ERROR       "错误"

    // 提示消息
    IDS_MSG_CLICK_COUNT   "点击次数：%d"
    IDS_MSG_WELCOME       "欢迎使用字符串表示例程序！"
    IDS_MSG_CONFIRM_EXIT  "确定要退出吗？"
    IDS_MSG_ABOUT         "这是一个演示字符串表和国际化功能的 Win32 程序。\r\n\r\n版本 1.0"

    // 错误消息
    IDS_ERR_LOAD_FAILED   "加载资源失败"
END
```

### 使用字符串表的窗口过程

```cpp
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static int s_nClickCount = 0;
    static HINSTANCE s_hInstance = NULL;

    switch (message)
    {
    case WM_CREATE:
    {
        s_hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

        // 创建菜单
        HMENU hMenu = CreateMenu();
        HMENU hFileMenu = CreatePopupMenu();

        AppendMenuW(hFileMenu, MF_STRING, ID_CMD_NEW,
            LoadStringDx(s_hInstance, IDS_CMD_NEW).c_str());
        AppendMenuW(hFileMenu, MF_STRING, ID_CMD_OPEN,
            LoadStringDx(s_hInstance, IDS_CMD_OPEN).c_str());
        AppendMenuW(hFileMenu, MF_STRING, ID_CMD_SAVE,
            LoadStringDx(s_hInstance, IDS_CMD_SAVE).c_str());
        AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hFileMenu, MF_STRING, ID_CMD_EXIT,
            LoadStringDx(s_hInstance, IDS_CMD_EXIT).c_str());

        HMENU hHelpMenu = CreatePopupMenu();
        AppendMenuW(hHelpMenu, MF_STRING, ID_CMD_ABOUT,
            LoadStringDx(s_hInstance, IDS_CMD_ABOUT).c_str());

        InsertMenuW(hMenu, 0, MF_POPUP, (UINT_PTR)hFileMenu,
            LoadStringDx(s_hInstance, IDS_MENU_FILE).c_str());
        InsertMenuW(hMenu, 1, MF_POPUP, (UINT_PTR)hHelpMenu,
            LoadStringDx(s_hInstance, IDS_MENU_HELP).c_str());

        SetMenu(hwnd, hMenu);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        s_nClickCount++;
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // 显示欢迎消息
        std::wstring welcome = LoadStringDx(s_hInstance, IDS_MSG_WELCOME);
        DrawText(hdc, welcome.c_str(), -1, &rcClient,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 在底部显示点击次数
        std::wstring clickMsg =
            FormatString(LoadStringDx(s_hInstance, IDS_MSG_CLICK_COUNT), s_nClickCount);

        RECT rcBottom = rcClient;
        rcBottom.top = rcClient.bottom - 50;
        DrawText(hdc, clickMsg.c_str(), -1, &rcBottom,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case ID_CMD_ABOUT:
        {
            std::wstring title = LoadStringDx(s_hInstance, IDS_TITLE_INFO);
            std::wstring msg = LoadStringDx(s_hInstance, IDS_MSG_ABOUT);
            MessageBox(hwnd, msg.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        case ID_CMD_EXIT:
        {
            std::wstring title = LoadStringDx(s_hInstance, IDS_TITLE_INFO);
            std::wstring msg = LoadStringDx(s_hInstance, IDS_MSG_CONFIRM_EXIT);
            if (MessageBox(hwnd, msg.c_str(), title.c_str(),
                MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        }
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

// 简单的字符串格式化函数
std::wstring FormatString(const std::wstring& format, int value)
{
    wchar_t buffer[256];
    swprintf_s(buffer, 256, format.c_str(), value);
    return std::wstring(buffer);
}
```

这样改写之后，代码确实变长了一点，但好处是显而易见的：
- 所有字符串都集中管理在资源文件中
- 代码的可读性大大提高
- 以后要添加其他语言支持只需要添加新的资源文件

---

## 第四步——理解国际化机制

国际化（Internationalization，常简写为 i18n）是指让程序能够适应不同语言和地区的需求。在 Win32 中，国际化主要通过资源语言标识符和资源 DLL 来实现。

### 资源语言标识符

每个资源文件都有一个语言标识符，用于指定资源的语言。在 .rc 文件中，可以通过 `LANGUAGE` 语句设置：

```rc
// 中文（简体，中国）
LANGUAGE LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED

#pragma code_page(936)

STRINGTABLE
BEGIN
    IDS_HELLO             "你好"
    IDS_GOODBYE           "再见"
END
```

```rc
// 英文（美国）
LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US

#pragma code_page(1252)

STRINGTABLE
BEGIN
    IDS_HELLO             "Hello"
    IDS_GOODBYE           "Goodbye"
END
```

常用的语言标识符：

| 语言 | 主语言 | 子语言 |
|------|--------|--------|
| 简体中文 | LANG_CHINESE (0x04) | SUBLANG_CHINESE_SIMPLIFIED (0x02) |
| 繁体中文 | LANG_CHINESE (0x04) | SUBLANG_CHINESE_TRADITIONAL (0x01) |
| 英语（美国） | LANG_ENGLISH (0x09) | SUBLANG_ENGLISH_US (0x01) |
| 英语（英国） | LANG_ENGLISH (0x09) | SUBLANG_ENGLISH_UK (0x02) |
| 日语 | LANG_JAPANESE (0x11) | SUBLANG_JAPANESE_JAPAN (0x01) |
| 韩语 | LANG_KOREAN (0x12) | SUBLANG_KOREAN (0x01) |

### 资源 DLL 结构

标准的 Win32 国际化方案是使用"资源 DLL"。基本思想是：
- 主程序（.exe）不包含任何本地化资源，或者只包含默认语言的资源
- 每种语言创建一个单独的 DLL，只包含翻译后的资源
- 运行时根据系统语言设置加载相应的资源 DLL

这种结构的优点是：
- 不需要重新编译主程序就能添加新语言
- 不同语言的资源可以独立开发和维护
- 用户可以只安装需要的语言包

---

## 第五步——创建多语言资源

让我们来实际创建一个支持中文和英文的程序。

### 项目结构

```
MyApp/
  +-- MyApp.vcxproj
  +-- MyApp.cpp
  +-- resource.h
  +-- MyApp.rc              (默认语言资源，如中文)
  +-- en-US/
  |    +-- MyAppEn.rc        (英文资源)
  +-- zh-CN/
       +-- MyAppZh.rc        (中文资源)
```

### 默认资源文件（中文）

```rc
// MyApp.rc - 中文（简体）资源
#include <windows.h>
#include "resource.h"

LANGUAGE LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED
#pragma code_page(936)

STRINGTABLE
BEGIN
    IDS_APP_TITLE         "我的应用程序"
    IDS_MENU_FILE         "文件"
    IDS_MENU_EDIT         "编辑"
    IDS_MENU_HELP         "帮助"
    IDS_CMD_NEW           "新建"
    IDS_CMD_OPEN          "打开..."
    IDS_CMD_SAVE          "保存"
    IDS_CMD_EXIT          "退出"
    IDS_CMD_ABOUT         "关于..."
    IDS_TITLE_INFO        "信息"
    IDS_TITLE_WARNING     "警告"
    IDS_TITLE_ERROR       "错误"
    IDS_MSG_WELCOME       "欢迎使用多语言程序！"
    IDS_MSG_CONFIRM_EXIT  "确定要退出吗？"
    IDS_MSG_ABOUT         "这是一个支持多语言的 Win32 程序。\r\n\r\n版本 1.0"
END
```

### 英文资源文件

```rc
// MyAppEn.rc - 英文（美国）资源
#include <windows.h>
#include "resource.h"

LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US
#pragma code_page(1252)

STRINGTABLE
BEGIN
    IDS_APP_TITLE         "My Application"
    IDS_MENU_FILE         "&File"
    IDS_MENU_EDIT         "&Edit"
    IDS_MENU_HELP         "&Help"
    IDS_CMD_NEW           "&New"
    IDS_CMD_OPEN          "&Open..."
    IDS_CMD_SAVE          "&Save"
    IDS_CMD_EXIT          "E&xit"
    IDS_CMD_ABOUT         "&About..."
    IDS_TITLE_INFO        "Information"
    IDS_TITLE_WARNING     "Warning"
    IDS_TITLE_ERROR       "Error"
    IDS_MSG_WELCOME       "Welcome to the multilingual application!"
    IDS_MSG_CONFIRM_EXIT  "Are you sure you want to exit?"
    IDS_MSG_ABOUT         "This is a multilingual Win32 application.\r\n\r\nVersion 1.0"
END
```

### 编译资源 DLL

创建资源 DLL 的项目配置：
1. 创建一个新的 DLL 项目
2. 只包含 .rc 文件，不需要任何 C++ 代码
3. 在链接器设置中指定 `/NOENTRY` 选项，这样 DLL 就没有入口点

Visual Studio 项目文件配置示例：

```xml
<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|Win32'">
  <LinkIncremental>false</LinkIncremental>
  <NoEntryPoint>true</NoEntryPoint>
  <ResourceOnlyDLL>true</ResourceOnlyDLL>
</PropertyGroup>
```

或者使用命令行：

```cmd
rc.exe /fo MyAppEn.res MyAppEn.rc
link.exe /dll /noentry /out:MyAppEn.dll MyAppEn.res
```

---

## 第六步——运行时加载语言资源

主程序需要在启动时检测系统语言，然后加载相应的资源 DLL。

### 检测系统语言

```cpp
#include <winnls.h>

// 获取当前系统语言
LANGID GetSystemLanguage()
{
    return GetUserDefaultUILanguage();
}

// 获取当前线程语言
LANGID GetThreadLanguage()
{
    return LANGIDFROMLCID(GetThreadLocale());
}

// 判断语言是否为中文（简体）
BOOL IsSimplifiedChinese(LANGID langid)
{
    return (PRIMARYLANGID(langid) == LANG_CHINESE) &&
           (SUBLANGID(langid) == SUBLANG_CHINESE_SIMPLIFIED);
}

// 判断语言是否为英语
BOOL IsEnglish(LANGID langid)
{
    return PRIMARYLANGID(langid) == LANG_ENGLISH;
}
```

### 加载语言资源 DLL

```cpp
// 全局变量，保存资源 DLL 句柄
HINSTANCE g_hResourceDll = NULL;

// 加载适合当前语言的资源 DLL
BOOL LoadLanguageResources(HINSTANCE hInstance)
{
    LANGID langid = GetUserDefaultUILanguage();

    // 根据语言决定加载哪个 DLL
    const wchar_t* dllName = NULL;

    if (PRIMARYLANGID(langid) == LANG_ENGLISH)
    {
        dllName = L"MyAppEn.dll";
    }
    else if (PRIMARYLANGID(langid) == LANG_CHINESE &&
             SUBLANGID(langid) == SUBLANG_CHINESE_SIMPLIFIED)
    {
        // 使用主程序中的默认资源
        return TRUE;
    }

    if (dllName != NULL)
    {
        // 尝试从程序目录加载 DLL
        wchar_t szDllPath[MAX_PATH];
        GetModuleFileName(hInstance, szDllPath, MAX_PATH);

        // 获取目录路径
        wchar_t* pLastSlash = wcsrchr(szDllPath, L'\\');
        if (pLastSlash != NULL)
        {
            *(pLastSlash + 1) = L'\0';
            wcscat_s(szDllPath, MAX_PATH, dllName);

            // 加载资源 DLL
            g_hResourceDll = LoadLibraryEx(szDllPath, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);

            if (g_hResourceDll == NULL)
            {
                // 加载失败，使用主程序资源
                DWORD dwError = GetLastError();
                wchar_t szError[256];
                swprintf_s(szError, 256, L"无法加载语言资源 DLL：%s (错误码：%lu)",
                    szDllPath, dwError);
                OutputDebugString(szError);
                return FALSE;
            }

            return TRUE;
        }
    }

    return FALSE;
}

// 卸载资源 DLL
void FreeLanguageResources()
{
    if (g_hResourceDll != NULL)
    {
        FreeLibrary(g_hResourceDll);
        g_hResourceDll = NULL;
    }
}
```

### 修改 LoadString 调用以使用正确的资源实例

```cpp
// 获取当前资源实例句柄
HINSTANCE GetResourceInstance()
{
    return g_hResourceDll ? g_hResourceDll : GetModuleHandle(NULL);
}

// 修改后的字符串加载函数
std::wstring LoadLocalizedString(UINT uID)
{
    HINSTANCE hInst = GetResourceInstance();
    int nLength = LoadStringW(hInst, uID, NULL, 0);

    if (nLength > 0)
    {
        std::wstring buffer(nLength, L'\0');
        LoadStringW(hInst, uID, &buffer[0], nLength + 1);
        return buffer;
    }

    return std::wstring();
}
```

---

## 第七步——手动切换语言

除了根据系统语言自动选择外，很多应用程序还允许用户手动切换语言。这需要重新加载资源并更新界面。

### 切换语言并刷新界面

```cpp
// 切换到指定语言
BOOL SwitchLanguage(const wchar_t* langCode)
{
    // 卸载当前资源 DLL
    if (g_hResourceDll != NULL)
    {
        FreeLibrary(g_hResourceDll);
        g_hResourceDll = NULL;
    }

    // 加载新的资源 DLL
    wchar_t szDllPath[MAX_PATH];
    GetModuleFileName(NULL, szDllPath, MAX_PATH);

    wchar_t* pLastSlash = wcsrchr(szDllPath, L'\\');
    if (pLastSlash != NULL)
    {
        *(pLastSlash + 1) = L'\0';

        // 构建语言 DLL 名称（如 MyAppEn.dll）
        wcscat_s(szDllPath, MAX_PATH, L"MyApp");
        wcscat_s(szDllPath, MAX_PATH, langCode);
        wcscat_s(szDllPath, MAX_PATH, L".dll");

        g_hResourceDll = LoadLibraryEx(szDllPath, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);

        if (g_hResourceDll != NULL)
        {
            // 通知所有窗口更新界面
            PostMessage(HWND_BROADCAST, WM_COMMAND, IDM_LANGUAGE_CHANGED, 0);
            return TRUE;
        }
    }

    return FALSE;
}

// 在窗口过程中处理语言切换消息
case WM_LANGUAGE_CHANGED:
case WM_COMMAND:
{
    if (LOWORD(wParam) == IDM_LANGUAGE_CHANGED || LOWORD(wParam) == IDM_LANGUAGE_ENGLISH)
    {
        // 重新创建菜单
        RecreateMenu(hwnd);

        // 刷新窗口标题
        SetWindowText(hwnd, LoadLocalizedString(IDS_APP_TITLE).c_str());

        // 强制重绘
        InvalidateRect(hwnd, NULL, TRUE);
        UpdateWindow(hwnd);
        return 0;
    }
    break;
}
```

---

## 第八步——完整示例程序

下面是一个完整的多语言示例程序框架。

### 主程序文件

```cpp
#include <windows.h>
#include <string>
#include "resource.h"

// 全局变量
HINSTANCE g_hResourceDll = NULL;
int g_nClickCount = 0;

// 函数声明
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void RecreateMenu(HWND);
HINSTANCE GetResourceInstance();
std::wstring LoadLocalizedString(UINT uID);
std::wstring FormatString(const std::wstring& format, int value);

// WinMain 入口点
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 尝试加载适合当前语言的资源
    wchar_t szDllPath[MAX_PATH];
    GetModuleFileName(hInstance, szDllPath, MAX_PATH);

    wchar_t* pLastSlash = wcsrchr(szDllPath, L'\\');
    if (pLastSlash != NULL)
    {
        *(pLastSlash + 1) = L'\0';
        wcscat_s(szDllPath, MAX_PATH, L"MyAppEn.dll");

        g_hResourceDll = LoadLibraryEx(szDllPath, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        // 如果加载失败也没关系，会使用主程序中的默认资源
    }

    // 注册窗口类
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"MultiLangDemo";

    if (!RegisterClassEx(&wcex))
    {
        return FALSE;
    }

    // 创建主窗口
    HWND hwnd = CreateWindowEx(
        0, L"MultiLangDemo", L"",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd)
    {
        return FALSE;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理资源 DLL
    if (g_hResourceDll != NULL)
    {
        FreeLibrary(g_hResourceDll);
    }

    return (int)msg.wParam;
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HINSTANCE s_hInstance = NULL;

    switch (message)
    {
    case WM_CREATE:
    {
        s_hInstance = ((LPCREATESTRUCT)lParam)->hInstance;
        RecreateMenu(hwnd);

        // 设置窗口标题
        SetWindowText(hwnd, LoadLocalizedString(IDS_APP_TITLE).c_str());
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        g_nClickCount++;
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        // 显示欢迎消息
        std::wstring welcome = LoadLocalizedString(IDS_MSG_WELCOME);
        DrawText(hdc, welcome.c_str(), -1, &rcClient,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // 显示点击次数
        std::wstring clickMsg =
            FormatString(LoadLocalizedString(IDS_MSG_CLICK_COUNT), g_nClickCount);

        RECT rcBottom = rcClient;
        rcBottom.top = rcClient.bottom - 50;
        DrawText(hdc, clickMsg.c_str(), -1, &rcBottom,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDM_CMD_ABOUT:
        {
            std::wstring title = LoadLocalizedString(IDS_TITLE_INFO);
            std::wstring msg = LoadLocalizedString(IDS_MSG_ABOUT);
            MessageBox(hwnd, msg.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
            return 0;
        }

        case IDM_CMD_EXIT:
        {
            std::wstring title = LoadLocalizedString(IDS_TITLE_INFO);
            std::wstring msg = LoadLocalizedString(IDS_MSG_CONFIRM_EXIT);
            if (MessageBox(hwnd, msg.c_str(), title.c_str(),
                MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
                DestroyWindow(hwnd);
            }
            return 0;
        }

        case IDM_LANG_ENGLISH:
        case IDM_LANG_CHINESE:
        {
            // 切换语言
            wchar_t szDllPath[MAX_PATH];
            GetModuleFileName(NULL, szDllPath, MAX_PATH);

            wchar_t* pLastSlash = wcsrchr(szDllPath, L'\\');
            if (pLastSlash != NULL)
            {
                *(pLastSlash + 1) = L'\0';

                if (LOWORD(wParam) == IDM_LANG_ENGLISH)
                {
                    wcscat_s(szDllPath, MAX_PATH, L"MyAppEn.dll");
                }
                else
                {
                    wcscat_s(szDllPath, MAX_PATH, L"MyAppZh.dll");
                }

                HINSTANCE hNewDll = LoadLibraryEx(szDllPath, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);

                if (hNewDll != NULL)
                {
                    if (g_hResourceDll != NULL)
                    {
                        FreeLibrary(g_hResourceDll);
                    }
                    g_hResourceDll = hNewDll;

                    // 刷新界面
                    SetWindowText(hwnd, LoadLocalizedString(IDS_APP_TITLE).c_str());
                    RecreateMenu(hwnd);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            return 0;
        }
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

// 重新创建菜单
void RecreateMenu(HWND hwnd)
{
    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();

    AppendMenuW(hFileMenu, MF_STRING, IDM_CMD_EXIT,
        LoadLocalizedString(IDS_CMD_EXIT).c_str());

    HMENU hLangMenu = CreatePopupMenu();
    AppendMenuW(hLangMenu, MF_STRING, IDM_LANG_ENGLISH, L"English");
    AppendMenuW(hLangMenu, MF_STRING, IDM_LANG_CHINESE, L"中文");

    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, IDM_CMD_ABOUT,
        LoadLocalizedString(IDS_CMD_ABOUT).c_str());

    InsertMenuW(hMenu, 0, MF_POPUP, (UINT_PTR)hFileMenu,
        LoadLocalizedString(IDS_MENU_FILE).c_str());
    InsertMenuW(hMenu, 1, MF_POPUP, (UINT_PTR)hLangMenu, L"Language");
    InsertMenuW(hMenu, 2, MF_POPUP, (UINT_PTR)hHelpMenu,
        LoadLocalizedString(IDS_MENU_HELP).c_str());

    SetMenu(hwnd, hMenu);
}

// 获取资源实例句柄
HINSTANCE GetResourceInstance()
{
    return g_hResourceDll ? g_hResourceDll : GetModuleHandle(NULL);
}

// 加载本地化字符串
std::wstring LoadLocalizedString(UINT uID)
{
    HINSTANCE hInst = GetResourceInstance();
    int nLength = LoadStringW(hInst, uID, NULL, 0);

    if (nLength > 0)
    {
        std::wstring buffer(nLength, L'\0');
        LoadStringW(hInst, uID, &buffer[0], nLength + 1);
        return buffer;
    }

    return std::wstring();
}

// 字符串格式化
std::wstring FormatString(const std::wstring& format, int value)
{
    wchar_t buffer[256];
    swprintf_s(buffer, 256, format.c_str(), value);
    return std::wstring(buffer);
}
```

---

## 后续可以做什么

到这里，Win32 字符串表和国际化的基础知识就讲完了。你现在应该能够：
- 在资源文件中定义字符串表
- 使用 `LoadString` 加载字符串
- 创建多语言资源 DLL
- 在运行时动态切换语言

但国际化是个大话题，还有很多高级内容值得探索：

1. **格式化字符串的处理**：不同语言的语法结构不同，可能需要调整参数顺序
2. **日期和时间的本地化**：不同地区的日期时间格式差异很大
3. **数字和货币格式**：千位分隔符、小数点、货币符号等
4. **文本方向**：阿拉伯语、希伯来语等语言的从右到左显示
5. **MUI（Multilingual User Interface）技术**：Windows Vista+ 的现代国际化方案
6. **GetUserDefaultUILanguage vs GetSystemDefaultUILanguage**：理解两种语言设置的差异

建议你先做一些练习巩固一下：
1. 把你之前写的 Win32 程序改造成使用字符串表
2. 创建一个英文版本的资源文件
3. 实现一个语言切换菜单，运行时切换语言
4. 研究一下 MUI 资源技术，了解现代 Windows 程序的国际化方案

下一步，我们可以探讨 Win32 资源系统的另一个重要主题：对话框模板深入。这将帮助你更好地理解和使用 Win32 的对话框资源。

---

## 相关资源

- [本地化消息字符串 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/wes/localizing-message-strings)
- [STRINGTABLE 资源 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/menurc/stringtable-resource)
- [LoadString 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winuser/nf-winuser-loadstringa)
- [国际化支持 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/intl/international-support)
- [创建多语言用户界面应用程序 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/intl/creating-a-multilingual-user-interface-application)
- [SetThreadLocale 函数 - Microsoft Learn](https://learn.microsoft.com/zh-cn/windows/win32/api/winnls/nf-winnls-setthreadlocale)
- [Implementing Globalization / Multilingual feature in Win32 API - Stack Overflow](https://stackoverflow.com/questions/1653874/implementing-globalization-multilingual-feature-in-win32-api-application)
- [How do I create language satellite DLLs - Stack Overflow](https://stackoverflow.com/questions/5327734/how-do-i-create-language-satellite-dlls-for-a-c-msvc-2008-solution)
