# 07_close_window

窗口关闭流程 —从 WM_CLOSE 到程序退出的完整链路。

对应章节：[第 2 章 常用系统消息](../../../tutorial/native_win32/2_ProgrammingGUI_NativeWindows_2.md)

## 学习要点

- WM_CLOSE / WM_DESTROY / WM_NCDESTROY 的顺序
- PostQuitMessage 退出消息循环
- 关闭确认（用户点击 X 后的拦截）
- DestroyWindow 与资源释放
