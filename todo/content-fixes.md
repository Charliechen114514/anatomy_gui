# 内容修复清单 · 文字质量项（P1）

> 本文件收**纯文字质量缺陷**（错别字 / 病句 / 术语 / 断链），是维护现有 72 章时的机械性修复清单。
> 章节路径以现有 `tutorial/native_win32/`、`hands_on_ur_own_gui/handbook/` 为准。
> 技术性 bug（P0 结构 / API / 代码脱节）与结构性问题（P2 拆分 / 缺章）另行追踪，不在本清单。

---

## 一、错别字 / 事实性错误（11 处）

| 位置 | 错 | 改 |
|:---|:---|:---|
| `16_…DialogTemplate.md:343` | `这意仏着` | `这意味着` |
| `9_…ModalDialog.md:544` | `wcscopy_s(data.name,…)` | `wcscpy_s(data.name, 64, L"张三")`（同文件 250 行正确） |
| `11_…DialogProc.md:849` | `WM_NEXTDLGCTRL` | `WM_NEXTDLGCTL` |
| `38_…D3D11_VertexInput.md:75` | `ID2D1InputLayout` | `ID3D11InputLayout` |
| `22_…GDI_Bitmap.md:1023` | 「0xFF00FF 是青色」 | 删（COLORREF 为 0x00bbggrr，0x00FF00FF=洋红，与 364 行矛盾） |
| `22_…GDI_Bitmap.md:466/489/502` | 「AlphaBlend 传 `sizeof(bf)`」 | 按值传 `BLENDFUNCTION bf` |
| `43_…D3D12_Philosophy.md:48` | `变成着器资源` | `着色器资源` |
| `32_…DirectWrite.md:121` | `[DRITE_TEXT_RANGE]` | `[DWRITE_TEXT_RANGE]` |
| `14_…IconCursorBitmap.md:457-464` | StretchBlt 注释「居中」 | 与代码「铺满」矛盾，统一 |
| `6_…ListView.md:663` | 标题「重新创建窗口(最可靠)」 | 与代码（改 `GWL_STYLE`）不符，统一 |
| `53_…Subclassing.md` | `WM_DESTROY` vs `WM_NCDESTROY` | 清理时机全文统一 |

---

## 二、病句（代表 5 处）

- `0_…Basic.md:253` 草稿句粘连
- `21_…GDI_Text.md:166/170` y 坐标前后矛盾
- `24_…GDI_Region.md:19` 「只有三种形态」表述错
- `45_…D3D12_ResourceHeap.md:305` `VERTEX_BUFFER` 非 D3D12 资源状态
- `59_…MsgMechanism.md:131` 跨线程 `SendMessage` 机理描述不准

> 以上为代表项；中严重度病句共约 26 处，按章通读复核其余。

---

## 三、术语不一致

**跨篇核心 4 类（须统一）**
- **Client Area** → 统一「客户区 / 非客户区」（`0_…Basic.md:310` 同句混用「客户区坐标：原点是工作区左上角」）
- **modeless** → 统一「非模态」（`16_…DialogTemplate.md:619`「无模式」必改）
- **Brush** → 统一「画刷」（`23_…` 七处、`22_…:297`「刷子」）
- **Vertex Buffer** → D3D12 子系列统一「顶点缓冲区」（`45_…` 用「顶点缓冲」）

**篇内漂移**
- `5_…WM_NOTIFY.md:167` `NM_click` → `NM_CLICK`
- `16_…DialogTemplate.md:168`「字体大小(1/8 点)」→「磅 / point」
- `35_…HLSL_CompileDebug.md:118/158/504` `errorBlob` vs `pErrorBlob` 统一
- `36_…HLSL_CBuffer.md:43-76` float3 对齐表述与官方 Packing Rules 不符
- `67_…signals.md:20/24 vs 288/293` `text()` vs `displayText()` 统一
- `71_…threadpool.md:36`「亲缘关系线程」→「线程亲和性 thread affinity」
- handbook 用 Stage、native_win32 用 Phase → 导航处明确对应关系
- `55_…SystemTray.md:626` 标题 `cbSize`/`dwSize` → 只用 `dwSize`

---

## 四、断链与引用

- **断链**：`handbook/71_threadpool.md:508-512`「下一步」承诺「收官彩蛋——用现代 C++ 封装 Win32 API」，目标章节不存在 → 删承诺 / 补章节 / 改「系列暂告段落」
- ✅ 站内 `.md` 链接：`index.md` 62 条相对链接全部存在，无锚点失效（已确认无问题，无需动）

---

## 执行方式

维护现有文章时，对照本清单过一遍文字质量（错别字 / 病句 / 术语 / 断链）。技术性 bug 与结构性问题另行追踪。
