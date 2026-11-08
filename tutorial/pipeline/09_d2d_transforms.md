---
title: 坐标变换
current_status:
  title: 正在打磨
  detail: 正在打磨中，随时会修改陈述，发现问题速速issue杀过来
  variant: polishing
---

# 坐标变换

上一篇[《几何与路径》](08_d2d_geometry.md)的结尾咱们留了一句“对勾也就可以转起来了”，本篇就来兑现它了。要转的东西其实一直都有：书架（也就是 `tutorial/native_win32/` 那套带完整示例的老教程）的[Direct2D 架构章](../native_win32/29_ProgrammingGUI_Graphics_Direct2D_Architecture.md)附过一个旋转三角形的最小程序，三角形也确实转得挺顺的，咱们把它绘制的关键两行搬过来看看：

```cpp
float a = g_angle + i * (2.0f * 3.14159f / 3.0f);
points[i] = D2D1::Point2F(cx + r * cosf(a), cy + r * sinf(a));
```

三个顶点的坐标是每一帧现算的：`WM_TIMER` 把角度推了一步，咱们就得拿三角函数把每个顶点的落点重算一遍。三角形的三个点咱们算得起，咱们把画面换成一块表盘试试：画面上的十二个刻度和两根指针全都绕着表心转，照书架的写法，每个能动的端点都得配一轮 `cosf` 与 `sinf`，绘制代码里就满眼都是三角函数了。

本篇讲的是另一条路：咱们让坐标的数字原封不动，让动的是坐标系本身。咱们把转了 30 度的线段，改写成永远朝 12 点方向画的线段，把转 30 度的活单独交给目标身上的矩阵，三角函数的部分全由 Direct2D 内部代劳了。上一篇攒下的对勾在本篇末尾也会回来客串一把，客串的正是摆好了就不动的静态形状。

读者的设定不变，您会 C++ 和 Win32 的基础用法、跟着咱们读到第九篇，就够用了。

## 让坐标动，形状不动

入口是渲染目标身上的 `SetTransform`，配套的还有把当前矩阵读回来的 `GetTransform`。咱们调用 `SetTransform` 传进去一个矩阵，从调用返回的那一刻起，后面所有绘制调用的坐标都要过一遍这个矩阵才落到画面上。注意咱们说的是所有：`FillRectangle`、`DrawLine`、`DrawGeometry`、`DrawText` 全都在它的管辖之内。上一篇咱们画的对勾要是想斜 15 度，`DrawGeometry` 的参数一个都不用动，外面套一层旋转矩阵就行了。

它管的所有绘制里还包括上一篇遗留的渐变轴问题。第七篇[《画刷：纯色、渐变、位图》](07_d2d_brushes.md)里咱们领教过，渐变轴活在渲染目标的坐标系里，不会自动跟着被填充的形状走。目标坐标系要是被矩阵转了，轴就跟着坐标系一起转了，因为轴的坐标本来就是拿这套坐标系写的。咱们再补一句官方文档里的机制：画刷自己身上还有一个 `SetTransform`，管的是画刷自己的坐标空间，跟目标身上的矩阵各管各的一层。两边的关系咱们记成一句话就够：目标矩阵挪的是画布，画刷矩阵挪的是颜料。

咱们拿对勾试一把斜过来的写法，几行就够了：

```cpp
// The check itself stays in plain coordinates.
D2D1_MATRIX_3X2_F tilt = D2D1::Matrix3x2F::Rotation(
    -15.0f, D2D1::Point2F(120.0f, 100.0f));

pRenderTarget->SetTransform(tilt);
pRenderTarget->DrawGeometry(g_pCheck, pBrush, 9.0f, pInkStyle);

// Back to the untwisted space for everything drawn afterwards.
pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
```

把这几步念一遍：矩阵摆在绘制调用的前面，绘制用的还是上一篇的原始坐标，第四个参数 `pInkStyle` 也是上一篇那个圆头线帽的钢笔描边样式，咱们原样借来，画完了再把矩阵摆回单位阵。上一篇那个 `g_pCheck` 的坐标一个都没改，斜过来的活全交给了矩阵，您把它跟书架的架构章里现算顶点的写法一比，就能看清两条路的分岔了。

铺垫到了这儿，机制咱们用一句话就能说清：`SetTransform` 改变的是整个坐标系的姿态。形状的坐标数字没有变，变的是数字的含义。咱们要是写下一个 (0、-150)、拿它指“正上方 150 个 DIP（设备无关像素，Direct2D 的坐标单位，换算的内情在[《坐标、DIP 与 DPI》](03_dip_and_dpi.md)讲过）”，过了矩阵之后它就会落到别的地方。

## 状态还压在目标上

咱们把矩阵设上去之后，它不会因为一笔画完了就自动复原。咱们在[《工厂、设备与渲染目标》](06_d2d_targets.md)里提过一嘴：Direct2D 把画刷、颜色都改成了随调用走的参数，唯独变换留在了目标上，一直作用在后面的绘制身上，直到咱们再调一次 `SetTransform` 为止。您要是画完一根旋转的指针就撒手不管，同一帧里后面画的背景、文字会一齐转下去。次序写反的代码不报错也不崩溃，画面偏偏就是不对，排查的时候全靠咱们自己记得目标身上还留着矩阵。

咱们自然会想到 GDI 的老办法。[《HDC 与 GDI 对象模型》](04_gdi_objects.md)里咱们见过 `SaveDC`，它把全套的面板设置压进 DC 自带的状态栈，之后一句 `RestoreDC` 就把它弹了回来，中间随咱们怎么拧。咱们把 D2D 这边的 `ID2D1RenderTarget` 方法表翻一遍，上面没有 `PushMatrix` 和 `PopMatrix` 这样的东西。它家的 Push 系列管的是别的事：`PushLayer` 推的是一层透明度或蒙版，`PushAxisAlignedClip` 推的是一块裁剪矩形。咱们把后者的官方文档读仔细一点，还能读出一层有意思的东西：咱们传进去的 clipRect 会被当前的世界变换变过去，然后取的是变换后区域的轴对齐包围盒，实际裁的也是那个包围盒。您看，它非但不管变换的保存恢复，自己的行为还得看变换的脸色。

所以变换的复位没有栈可依赖，责任全在咱们自己手上。惯用的写法有两个，咱们都摆在明面上：画完了一段局部效果之后，咱们调 `SetTransform(D2D1::Matrix3x2F::Identity())` 把矩阵摆回单位阵。或者干脆在每个绘制的开头，把当帧要用的矩阵显式地设一遍，不指望目标替咱们保管上一帧留下过什么。您可以把这里跟第四篇的 GDI 状态机对照着回味：GDI 是十几个旋钮全要咱们配对管理，Direct2D 把它们缩到了变换等几处，但是只要还有状态留在目标上，“用完恢复”的纪律就还在，只是清单短了。

书架在 GDI+ 的世界里也备着同款机器，咱们顺路看一眼。[变换一章](../native_win32/27_ProgrammingGUI_Graphics_GdiPlus_Transform.md)里的 `Graphics::SetTransform` 配 `Matrix` 类干的是一样的活，`RotateTransform` 一类的调用还是叠加式的，书架还特意提醒咱们在每个绘制循环的开头重置变换，免得上一帧的变换累积到这一帧。

## 三行两列的仿射矩阵

`SetTransform` 收的类型是 `D2D1_MATRIX_3X2_F`，名字里的 3X2 说的就是它的身材，三行两列的六个浮点数。咱们把六个数的位置和默认值摆成表，右边的默认值凑成的正是单位阵：

| | 第 1 列 | 第 2 列 |
|:---|:---|:---|
| 第 1 行 | m11，默认 1.0 | m12，默认 0.0 |
| 第 2 行 | m21，默认 0.0 | m22，默认 1.0 |
| 第 3 行 | m31（即 dx），默认 0.0 | m32（即 dy），默认 0.0 |

六个数是怎么把一个点搬走的？咱们拿点的变换式看，行向量写法下它是这样的：

```
x' = x * m11 + y * m21 + dx
y' = x * m12 + y * m22 + dy
```

前四个数 m11、m12、m21、m22 排成的二乘二方块管的是线性部分，也就是旋转、缩放、斜切的出处。末尾的 dx、dy 管平移，它们在结构体的联合里还有一套名字叫 `_31`、`_32`，两套名字指的是同样的两个数（`OffsetX`、`OffsetY` 是另一家 DXGI_MATRIX_3X2_F 的字段名，别跟这里混了）。您拿帮手类 `Matrix3x2F` 身上的 `TransformPoint` 方法可以直接验算：咱们喂它一个点，它就按上面两行把落点算了出来。

六个数为什么装得下平移、旋转、缩放、斜切四种动作？因为它们合起来表达的是一个仿射变换（affine transform），咱们用白话讲：坐标纸上越分越细的方格网，过完仿射变换就成了平行四边形网，横线之间还是平行的，竖线之间也还是平行的，只是整体被拉伸、转过角度、挪了位置。

咱们把四种动作都看作它的特例，每一种都是把方格网掰成平行四边形网的手段。

- 平移是整张网原样挪窝。
- 旋转把整张网转过一个角度。
- 缩放沿横竖两个方向把网拉开或者收拢。
- 斜切把整片网格推斜，推完之后横线竖线依旧各自平行。

咱们再点一句，装不下的只有透视这类“近大远小”的效果，它要到了三维图形那边才登场，二维的 Direct2D 不接它。

## 四位帮手与一个单位阵

真要咱们自己手填六个数，光绕表心转 30 度的一个矩阵，就得现算上一轮 cos 与 sin 的值，写出来的代码反而更累。SDK 在 `d2d1helper.h` 里给了一个帮手类 `D2D1::Matrix3x2F`（包含 `d2d1.h` 的时候也就带上了），四位静态的帮手各造一种矩阵，参数咱们列成表：

| 帮手 | 参数 | 造出什么 |
|:---|:---|:---|
| `Translation(x, y)` | x、y 方向的位移量，单位是 DIP | 平移 |
| `Rotation(angle)` 与 `Rotation(angle, center)` | 旋转的角度，单位是度，可带旋转中心 | 旋转 |
| `Scale(x, y)` 与 `Scale(x, y, center)` | 两个方向的缩放倍数，可带中心 | 缩放 |
| `Skew(angleX, angleY)` 与 `Skew(angleX, angleY, center)` | 两个方向的斜切角度，单位是度，可带中心 | 斜切 |

角度的单位全是度，咱们跟用弧度的 `cosf` 那一套划清了界限。旋转的正方向是顺时针，因为 D2D 的 y 轴朝下，咱们把数学课里按逆时针推的公式搬到这块坐标纸上画，出来的就是咱们看到的顺时针。

center 参数是四位帮手里最贴心的设计：不带 center 的 `Rotation` 绕的是原点 (0、0)，而表盘的指针显然要绕表心转，带 center 的版本替咱们把“挪到原点、转、挪回去”的三段组合打包好了，咱们不必自己拼。

咱们要是拿不准想要的角度，`TransformPoint` 就是现成的验算工具：给四位帮手各造一个矩阵，拿它算几个点的落点，比盯着六个数反推的写法直观得多。

帮手类身上还有几个好用的成员，咱们顺路记下。`operator*` 负责矩阵的乘法，下一节的乘法次序靠的就是它。`IsInvertible` 和 `Invert` 管的是求逆矩阵，命中测试的坐标换算会用到它。`TransformPoint` 前面已经出场过了，咱们拿它单算一个点的落点。`Identity()` 造的是单位阵，六个数里的 m11、m22 是 1，其余的都是 0，任何点过了它都原地不动，咱们正好拿它当复位的那一手。

## 乘法从左往右读

单个矩阵认完了，连续两个变换该怎么拼？您要是上过线性代数的课，脑子里留着的多半是矩阵叠乘、从右往左读的那一版印象，这里咱们得把话分开说清楚，因为 D2D 的约定跟数学课的直觉正好相反。

数学课用的是列向量：点写成竖的一列，复合变换写成两个矩阵的乘积，作用的次序从右往左数，离点最近的矩阵头一个作用。D2D 用的是行向量：点写成横的一行 `[x y 1]`，复合的矩阵从左往右读，写在前面的矩阵头一个作用，跟咱们读代码的顺序一致。两套写法表达的是同一个复合，只是排版上的不同，可咱们写代码时拼错了次序，画出来的就是另一副样子了。官方的多变换文档给了现成的对照，咱们把关键的行搬过来：

```cpp
D2D1_MATRIX_3X2_F rotation = D2D1::Matrix3x2F::Rotation(
    45.0f, D2D1::Point2F(330.0f, 70.0f));

D2D1_MATRIX_3X2_F translation = D2D1::Matrix3x2F::Translation(20.0f, 10.0f);

// 绕 (330, 70) 转 45 度，再向右移 20、向下移 10：
m_pRenderTarget->SetTransform(rotation * translation);

D2D1_MATRIX_3X2_F rotation2 = D2D1::Matrix3x2F::Rotation(
    45.0f, D2D1::Point2F(70.0f, 70.0f));

// 向右移 20、向下移 10，再绕 (70, 70) 转 45 度：
m_pRenderTarget->SetTransform(translation * rotation2);
```

咱们手上是同一组素材，可两行 `SetTransform` 画出来的矩形位置就是不一样，官方文档还给两个结果配了并排的示意图，摆出来供您对照。矩阵的乘法不交换，作用的次序一换，复合出来的矩阵也就变了样。

咱们拿数字亲手算一遍更踏实。咱们取的点记作 (0、0)，平移矩阵 T 用的是 `Translation(100, 0)`，旋转矩阵 R 用的是不带 center 的 `Rotation(90)`，转的方式是绕原点顺时针拧 90 度。平移在前而旋转在后时，咱们写下的乘法是 T * R：点 (0、0) 过了 T 就到了 (100、0)，向右挪了 100，再过了 R 顺时针转 90 度，向右的方向就成了向下，点也就落到了 (0、100)。旋转在前而平移在后时，咱们写 R * T：点 (0、0) 绕着原点转了半天也还在原点，再平移就到了 (100、0)。同样两个矩阵的两种次序，落点一个成了 (0、100)，另一个成了 (100、0)，差别咱们肉眼都看得见。

差别出在哪儿？平移在前的时候，向右挪的 100 个 DIP 会被后面的旋转连着转掉，转 90 度之后横的就成了竖的。旋转在前的时候，旋转对原点是无能为力的，平移的分量原封不动地留了下来。咱们还能把两个复合矩阵真的乘出来核对，六个数摆在下面咱们一眼就能比：

```
T * R：m11=0   m12=1   m21=-1  m22=0   dx=0    dy=100
R * T：m11=0   m12=1   m21=-1  m22=0   dx=100  dy=0
```

您要是想自己验乘积，`Matrix3x2F` 的 `operator*` 就是现成的工具，咱们把 T 与 R 乘开再打印 dx、dy 两个分量，表里的数就对上了。

两个复合的线性部分一模一样，差别全都落在平移的分量上，跟逐点算的结果对得严丝合缝。往后您拼复合矩阵拿不准的时候，就回到行向量的定义上：写在前面的头一个作用，咱们从左往右念，念出来的顺序就是画面上发生的顺序。

## 示例：一个会走的表盘

机制认齐了，咱们动手把开头许下的表盘画出来。设备一侧的骨架沿用第六篇那套 `CreateDeviceResources` 与 `DiscardDeviceResources` 回路，三把纯色刷照[第七篇《画刷：纯色、渐变、位图》](07_d2d_brushes.md)的样子建在“指针非空就返回”的守卫里，画完释放刷子的 `DiscardBrushResources` 跟设备一侧那对是同一个路数，设备丢失时咱们把它们同批重建，这两段咱们不重贴。书架里没有现成的变换示例，`Matrix3x2F` 在书架的正文里只露过一次面，出场的是[特效与图层章](../native_win32/31_ProgrammingGUI_Graphics_Direct2D_EffectsLayer.md)给阴影做偏移的那行 `Translation`，所以本篇的示例代码是笔者按 SDK 的接口签名与官方文档写的，咱们在篇末“您可以动手的三处”里备了专门针对它的观察点。咱们只看绘制本身：

```cpp
void OnPaint(HWND hwnd)
{
    CreateDeviceResources(hwnd);
    if (g_pRenderTarget == NULL) {
        return;
    }

    g_pRenderTarget->BeginDraw();
    g_pRenderTarget->Clear(D2D1::ColorF(0.12f, 0.12f, 0.16f, 1.0f));

    RECT rc;
    GetClientRect(hwnd, &rc);
    const float cx = (rc.right - rc.left) / 2.0f;
    const float cy = (rc.bottom - rc.top) / 2.0f;
    const float kRadius = 150.0f;
    const D2D1_POINT_2F center = D2D1::Point2F(cx, cy);

    // Face circle: drawn under the identity transform.
    g_pRenderTarget->DrawEllipse(
        D2D1::Ellipse(center, kRadius, kRadius), g_pFaceBrush, 2.0f);

    // 12 ticks: one fixed segment shape, twelve matrices.
    for (int i = 0; i < 12; i++) {
        g_pRenderTarget->SetTransform(
            D2D1::Matrix3x2F::Rotation(i * 30.0f, center));
        g_pRenderTarget->DrawLine(
            D2D1::Point2F(cx, cy - kRadius),
            D2D1::Point2F(cx, cy - kRadius + 10.0f),
            g_pTickBrush, 2.0f);
    }

    // Hands: each drawn pointing at 12, the matrix does the turning.
    SYSTEMTIME st;
    GetLocalTime(&st);
    const float kMinuteAngle = st.wMinute * 6.0f + st.wSecond * 0.1f;
    const float kHourAngle = (st.wHour % 12) * 30.0f + st.wMinute * 0.5f;

    g_pRenderTarget->SetTransform(
        D2D1::Matrix3x2F::Rotation(kHourAngle, center));
    g_pRenderTarget->DrawLine(
        center, D2D1::Point2F(cx, cy - kRadius * 0.55f), g_pHandBrush, 6.0f);

    g_pRenderTarget->SetTransform(
        D2D1::Matrix3x2F::Rotation(kMinuteAngle, center));
    g_pRenderTarget->DrawLine(
        center, D2D1::Point2F(cx, cy - kRadius * 0.85f), g_pHandBrush, 3.0f);

    // Done with matrices: put the space back for whatever comes next.
    g_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    HRESULT hr = g_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardBrushResources();
        DiscardDeviceResources();
        InvalidateRect(hwnd, NULL, FALSE);  // repaint to rebuild
    }
}
```

咱们把绘制代码里最要紧的机关读一遍。十二个刻度是同一段线段画了十二遍：线段的坐标永远是“12 点方向、从外圈往里 10 个 DIP 的那一小段”，转过去的活全交给了 `Rotation(i * 30.0f, center)`，循环体里没有一行三角函数的影子。两根指针的道理是同样的：线段永远是朝正上方画的，长度上有一短一长的差别，角度则全在矩阵的手里。角度的换算咱们顺口念一遍：分针每分钟走 6 度、秒针每秒再贴 0.1 度的细分、时针每小时挪 30 度再搭上每分钟 0.5 度的零头。咱们拿它跟书架三角形的那两行 `cosf` 与 `sinf` 一比，绘制代码里跟角度沾边的算术就只剩乘法和加法了。

驱动它的定时器跟书架三角形用的是同一个路数，只是咱们给表盘按秒走就够用了：

```cpp
SetTimer(hwnd, 1, 1000, NULL);  // one tick per second

// ...
case WM_TIMER:
    InvalidateRect(hwnd, NULL, FALSE);  // ask for the next frame
    return 0;
```

表盘的指针要是想走得更平滑一些，咱们把定时器的间隔改回书架三角形那样的 16 毫秒就行，指针角度里秒的细分代码里已经配好了，帧率一高走起来就是连续的。

收尾那句 `Identity()` 复位不是可有可无的装饰。您把复位那句注释掉，本篇的示例也确实不会当场出错：下一帧的第一笔表盘圆会踩在上一帧分针留下的矩阵上画出来，好在咱们画的圆绕着表心转多少度都还是同一个圆，这笔绘制咱们侥幸没看出错，矩阵真正的重设要等到刻度循环的第一句 `SetTransform`。可是绘制代码是会长个头的：下个月您往末尾添一行表盘下面的文字，变换要是还挂在分针的角度上，文字就带着分针转了起来。复位一行代码的成本，换的是“后面的代码不用知道前面转过什么”，咱们认为划算。

裁剪那里咱们再补一小段，把 `PushAxisAlignedClip` 的怪脾气落到眼前。官方文档画的四步图咱们用文字复述一遍：渲染目标上已经设着一个旋转的变换，咱们再推一块 clipRect 进去，Direct2D 会把咱们传的 clipRect 也一起转过去，然后取转完之后那块区域的轴对齐包围盒，真正裁的是包围盒。所以变换之下咱们以为裁的是一个斜着的矩形，实际裁到的却是一个比它更大的正矩形，官方也直言这是为了效率的取舍。咱们在带变换的绘制里用裁剪的时候，矩形的坐标要拿变换后的世界来心算一遍，不然裁出来的范围会比预期大一圈。

## 把变换烘进几何

拿矩阵去转目标坐标系的路子适合会动的东西，画面上另外还有一大批摆好了就不动的形状，等着它们的是更合身的去处。工厂的身上还备着一个 `CreateTransformedGeometry`，咱们拿上一篇攒下的对勾当原料看它的用法：

```cpp
static ID2D1TransformedGeometry* g_pBigCheck = NULL;

// ... inside CreateGeometryResources, after g_pCheck exists:
    pFactory->CreateTransformedGeometry(
        g_pCheck,
        D2D1::Matrix3x2F::Scale(2.0f, 2.0f, D2D1::Point2F(120.0f, 100.0f)),
        &g_pBigCheck);
```

它收一个现成的几何加一个矩阵，交还给咱们一个 `ID2D1TransformedGeometry`。矩阵在创建的那一刻就烘进去了，官方文档写明这个成品是不可变（immutable）的对象，咱们也就别指望改动源几何能让烘好的成品跟着变。绘制的时候把它当普通几何交给 `DrawGeometry` 或 `FillGeometry` 就行，目标上不用设任何的矩阵。值不值的问题，咱们分两条来看。

头一条是描边的效果，官方文档在这里藏了一条容易漏看的差别：目标变换作用的是填充和描边两样，几何变换却只作用在填充的那一半上，因为变换在几何被描边之前就定型了。摆在咱们眼前的效果是：咱们拿目标变换把形状放大两倍，线宽就跟着变粗了，拿几何变换烘进去的放大，描出来的线宽还是咱们传给 `DrawGeometry` 的那个数。`g_pBigCheck` 的描边要是传 9.0f，出来的勾就放大了一圈，笔锋却还是 9 个 DIP 的粗细，跟原版是一模一样的。哪个合用要看您的场景：跟手缩放的交互、动画里的中间帧，咱们要的是线宽跟着形状一起呼吸的感觉，那咱们用目标变换。机械制图式的等宽线条、定稿的静态装饰形状，咱们换几何变换。Windows 8 起还有 `D2D1_STROKE_TRANSFORM_TYPE_FIXED` 一类的描边变换类型，它能在目标变换之下守住原有的线宽，官方文档有它的完整说明，咱们知道有它就行。

第二条是开销的清单，咱们把话说实在：一排各自转过角度的静态形状靠 `SetTransform` 画，目标上的矩阵就得每个形状换一次。烘进了几何之后，整批的形状都能在单位变换下一次画完，矩阵切换的次数就从每个形状一次降到了零。省下的量级笔者没有实测过，定性地说，状态切换在 GPU 的命令流里不是白来的。

资源归属这边它跟上一篇也对得上：`CreateTransformedGeometry` 挂在工厂的身上，按第六篇的资源对照表来归类，它归的是设备无关一类，创建一次就从程序开头活到了结尾，设备丢失的重建名单里没有它。上一篇咱们说过几何的创建贵、要复用，烘变换正好搭在同一批对象的同一个生命周期上：启动的时候建好，帧里只画而不建。

## 您可以动手的三处

表盘是笔者写的示例，咱们自己验自己。下面三处的观察，每一处咱们只要改上一行、编上一遍：

1. 咱们把时针角度里的 `% 12` 去掉，表盘照样走它的，只是时针改在一天里转满两圈了。矩阵对 360 度以上的角度照样接得住，取模是写给咱们自己看的，矩阵压根用不着它。
2. 咱们把收尾那句 `Identity()` 复位注释掉，再在两根指针的后面添一句画矩形的 `FillRectangle`。矩形被带着分针的角度转了个方向画出来，变换还作用在后面的绘制身上，您亲眼看到它没有复原。
3. 咱们给表盘的全部绘制外围包一层 `SetTransform(D2D1::Matrix3x2F::Scale(1.5f, 1.5f, center))`，画完了再复位。整个表盘放大了一半，刻度和指针也跟着变粗了。您再拿 `CreateTransformedGeometry` 把刻度烘成放大版对照着看，线宽的差别就是几何变换那一节的机制现身。

## 自查几问

您读到这里，下面几个问题应该都能凭本篇的机制答上来，答不上的那一段值得您回去再看一遍。

1. `SetTransform` 设上去的变换，作用范围到哪儿为止？靠什么收回？
2. `ID2D1RenderTarget` 上有没有保存恢复变换的栈？惯用的复位写法是什么？
3. `D2D1_MATRIX_3X2_F` 的六个数各自管什么？平移分量在官方文档里的名字是什么？
4. 平移在前、旋转在后，两个矩阵该怎么乘？数学课的列向量写法跟它差在哪儿？
5. 目标变换与几何变换，对描边线宽的效果差在哪？各自适合什么场景？
6. `CreateTransformedGeometry` 创建出来的对象归哪类资源？设备丢失时要不要重建？
7. 带旋转的绘制里推 `PushAxisAlignedClip`，实际被裁的区域跟咱们传的矩形差在哪儿？

## 下一步

变换补完了之后，画面会动了，Direct2D 手里还欠着一块像素本身的搬运。`Clear` 铺的纯色和画刷的渐变都算“现算出来的颜料”，从图片文件解码出来的现成像素怎么进画面、怎么当离屏的画布用，那又是另一套机制了。咱们把这些活留给下一篇[《位图与离屏渲染》](10_d2d_bitmaps.md)。对勾的旋转要是还没过瘾，咱们把它跟 `CreateTransformedGeometry`、`Rotation` 拼起来，一个歪着头的对勾就算真转起来了。
