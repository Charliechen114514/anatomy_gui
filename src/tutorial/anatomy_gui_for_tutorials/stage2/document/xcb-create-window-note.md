# XCB 常见陷阱

## 1. xcb_create_window 的 width/height 不能为 0

X11 协议明确规定窗口 width/height 必须 nonzero，传 0 是协议违规。
部分 X Server 会发 `BadValue` 错误，部分表现为窗口不可见/后续请求失效。

```cpp
// 错误：0x0 是非法值
xcb_create_window(conn, depth, window, parent,
                  0, 0, 0, 0, 0,
                  XCB_WINDOW_CLASS_INPUT_OUTPUT, visual, 0, NULL);

// 正确：创建时就给合法非零尺寸
xcb_create_window(conn, depth, window, parent,
                  100, 100, 500, 500, 0,
                  XCB_WINDOW_CLASS_INPUT_OUTPUT, visual, 0, NULL);
```

建议用 `xcb_create_window_checked` + `xcb_request_check` 捕获错误。

## 2. 必须注册事件掩码才能收到事件

`xcb_create_window` 的 `value_mask` 传 0 意味着不订阅任何事件，
自然收不到 `XCB_KEY_PRESS` 等事件。

```cpp
uint32_t values[] = {
    XCB_EVENT_MASK_EXPOSURE |
    XCB_EVENT_MASK_KEY_PRESS |
    XCB_EVENT_MASK_KEY_RELEASE |
    XCB_EVENT_MASK_STRUCTURE_NOTIFY |
    XCB_EVENT_MASK_FOCUS_CHANGE
};

xcb_create_window(conn, depth, window, parent,
                  100, 100, 500, 500, 0,
                  XCB_WINDOW_CLASS_INPUT_OUTPUT, visual,
                  XCB_CW_EVENT_MASK,   // ← 关键：设置事件掩码
                  values);
```

也可后置：`xcb_change_window_attributes(conn, window, XCB_CW_EVENT_MASK, &mask)`。

## 3. 窗口不一定自动获得键盘焦点

即使窗口可见，键盘焦点可能在其他窗口上。建议先监听 `XCB_EVENT_MASK_FOCUS_CHANGE`
确认是否收到 `XCB_FOCUS_IN`。调试阶段可临时用 `xcb_set_input_focus` 强抢焦点，
但生产代码应交给窗口管理器处理。
