# Act 3 · 渲染栈 · 执行计划

> 幕旨：显微镜下的像素——GDI(CPU) → Direct2D(GPU 2D) → D3D11/12(可编程管线)。回答「一个像素从代码到屏幕经历了什么」。资产已有，以**迁移 + 修图形高危 bug + 加 Vulkan 对比章**为主。

---

## A. 迁移（现有 graphics → 本幕）
- `gdi/`（HDC/对象/形状/文字/位图/双缓冲/Region/AlphaBlend）← ch18–25
- `gdiplus/`（架构/变换/编解码）← ch26–28
- `direct2d/`（架构/几何/效果/DirectWrite/GDI 互操作）← ch29–33
- `hlsl/`（基础/编译调试/cbuffer）← ch34–36
- `d3d11/`（InitSwapChain/VertexInput/TextureSampler/Depth3D/Lighting/BlendAlpha + 精灵渲染器阶段项目）← ch37–42, 61
- `d3d12/`（哲学/CmdQueue/ResourceHeap/RootSig/D3D11 互操作）← ch43–47
- `opengl/`（Win32 嵌入）← ch51

---

## B. 新增
- **`vulkan-vs-d3d12.md`**（单章）：Vulkan 的实例/设备/队列/SwapChain/Render Pass/Pipeline/命令缓冲/帧同步，**对照 D3D12 同概念**讲差异——不系统教 Vulkan，只做「同一管线两种现代 API」的剖析对比。（落实定位收窄：砍掉独立 Vulkan 2 章）

---

## C. 复查待修（图形高危，迁移时必修）

**P0 API（崩溃/泄漏/无效）**
- `hlsl/03_cbuffer` **H2**：DYNAMIC+CPU_ACCESS_WRITE 的 cbuffer 却用 `UpdateSubresource`（无效）→ 改 `Map/Unmap`(WRITE_DISCARD)
- `sprite_renderer/TextRenderer.hpp` **H8**：析构未 `DeleteObject(m_font)` → HFONT 泄漏
- `sprite_renderer/SpriteBatch.hpp` **H6**：`Map` HRESULT 丢弃直接 memcpy → Map 失败写未初始化内存
- `gdiplus/03_image_codec` **GP4**：每次 WM_PAINT 都 `Bitmap::FromFile` 重解码 → 加载时缓存一次
- `gdi/01_hdc_basics` **G2**：WM_LBUTTONDOWN 先 InvalidateRect(TRUE) 再 GetDC 直绘 → 持久绘制竞态，只在 WM_PAINT 绘
- `gdiplus/01_gdiplus_hello` **GP1**：星形路径自相交 + 两分支等价 → 五角星缠绕错误

**P0 代码—文章脱节（路线相悖，最重）**
- `sprite_renderer`（ch61）：文章以 D2D/DirectWrite 互操作为卖点，实现全改 GDI 文字渲染，CMakeLists 未链 d2d1/dwrite → **统一为「教学骨架 vs 仓库源码」提示或对齐路线**
- `opengl/01_opengl_win32`（ch51）：文章核心是「两步法 + wglCreateContextAttribsARB Core Profile」，main.cpp 直接用反对的 `wglCreateContext` → 对齐

**P1 实质性技术错误**
- ch25 预乘 Alpha：AlphaBlend/UpdateLayeredWindow 源位图**必须预乘 Alpha**，示例未预乘 → 半透明边缘黑晕（最关键技术错误）
- ch36 cbuffer 16 字节对齐：正文未讲对齐规则，练习却要求；float3 对齐论断与微软 Packing Rules 相悖
- ch46 根签名 1.1：`D3D12_DESCRIPTOR_RANGE1` 结构体字段错误
- ch39 GenerateMips / ch51 着色器编译过程缺失 / ch28 FromStream 所有权自相矛盾

**P2 篇幅拆分**（迁移时顺带）
- ch8 MoreControls(2000行) → 拆 3 篇；ch6 ListView/ch7 TreeView → 理论+实战；ch19/ch20/ch22/ch21 各拆上下

**P3 配图**
- 40+ 完整示例缺运行截图（读者无法确认「跑出来该长这样」）→ 优先批量补齐（价值最高）
- 核心概念图：D2D 资源分类(ch29)、D3D11 三件套(ch37)、D3D12 上传流程(ch45)、描述符堆+根签名(ch46)、WGL 两步法(ch51)

---

## D. 产出验收
- [ ] graphics 全部迁移到 act3-render/ 子目录
- [ ] Vulkan vs D3D12 对比章完成
- [ ] 本幕复查 P0/P1 图形条目全修
- [ ] 至少补齐 40+ 示例的运行截图
