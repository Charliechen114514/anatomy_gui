# Act 4 · 成型与交付 · 执行计划（精简）

> 幕旨：把「能跑的标本」变成「可交付的产品」。跨框架（Win32/GTK/WinUI3 各讲对应方案）。精简版——砍纯工具书内容，留与「理解」相关的工程主题。

---

## 要写的章

| 章 | 写什么 | 做出什么 | 备注 |
|:---|:---|:---|:---|
| **DPI 适配** | Per-Monitor V2、各框架 DPI 处理、双/混合显示器测试 | DPI 感知窗口（跨拖屏正确缩放） | 接 Act 1 miniui DPI 深化 |
| **主题与视觉** | 沉浸式深色模式 + DWM（Mica/Acrylic/圆角）、GTK CSS 主题、WinUI3 Style | 系统主题跟随应用 | 补复查 **H1** |
| **性能优化** | 脏区最小化、虚拟列表（LVS_OWNERDATA）、启动优化、双缓冲 | 百万行虚拟列表 60fps | 接器官 5 绘制/脏区 |
| **多线程与 UI 线程安全** | PostMessage/g_idle_add/Dispatcher、线程安全队列、Worker 模式 | 多线程安全日志控制台 | 接器官 8 异步/并发 |
| **打包与部署** | Inno Setup/MSIX/AppImage/DMG、VC Redist、代码签名、**应用清单 Manifest** | 三平台安装包模板 | 补复查 **H2** |
| **国际化 i18n** | UTF-8/16、Satellite DLL、gettext、运行时切语言 | 中英双语 + 运行时切换 | |
| **辅助功能 a11y**（跨器官专题） | WM_GETOBJECT / UI Automation Provider、GTK AT-SPI、WinUI3 AutomationProperties | 一个可被屏幕阅读器读出的自绘控件 | 补复查 **H3**（ch48–50 自绘控件最致命的隐藏代价） |

---

## 砍（→ `reference/`）
- 注册表、系统服务、纯 Windows 系统 API 集成、**日志 / 配置管理（INI·JSON）/ 插件架构** → `reference/`（复查 L4 GetLastError 错误处理规范也入此）
- ARM64 / GDI 批处理等小众主题 → `reference/`

---

## 跨框架对照原则
每章覆盖 Win32 / GTK / WinUI3 三框架的对应方案；**声明要诚实**——注册表等 Windows 专属能力不强行编造 GTK 对应（旧规划的承诺不可实现），注明「Windows 专属」。

---

## 产出验收
- [ ] 7 章完成（DPI/主题/性能/线程/打包/i18n/a11y）
- [ ] 注册表/系统服务移入 reference/
- [ ] a11y 章把 Act 1 自绘控件（ch48–50）补上无障碍接口
- [ ] 跨框架三线对照真实可行（不编造不存在的对应）
