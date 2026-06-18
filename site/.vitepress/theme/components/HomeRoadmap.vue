<script setup lang="ts">
import { withBase } from 'vitepress'

const layers = [
  {
    id: 'win32-basic',
    title: 'Win32 基础',
    status: 'done',
    statusText: '已完成',
    count: '4 章 · 基础概念到 DPI 适配',
    desc: 'Windows 程序本质、消息机制、窗口创建、DPI 适配',
    link: '/native_win32/0_ProgrammingGUI_Basic',
  },
  {
    id: 'win32-controls',
    title: '控件与对话框',
    status: 'done',
    statusText: '已完成',
    count: '8 章 · 标准控件到对话框过程',
    desc: '按钮、编辑框、ListView、TreeView、模态/非模态对话框',
    link: '/native_win32/4_ProgrammingGUI_NativeWindows_Controls',
  },
  {
    id: 'win32-resource',
    title: '资源与工具栏',
    status: 'done',
    statusText: '已完成',
    count: '7 章 · 资源文件到状态栏',
    desc: '菜单、图标、位图、字符串表、对话框模板、工具栏与状态栏',
    link: '/native_win32/12_ProgrammingGUI_NativeWindows_ResourceFiles',
  },
  {
    id: 'gdi',
    title: 'GDI 图形',
    status: 'done',
    statusText: '已完成',
    count: '8 章 · 设备上下文到 Alpha 混合',
    desc: 'HDC、画笔画刷、图形绘制、文字渲染、位图、双缓冲、Region、Alpha',
    link: '/native_win32/18_ProgrammingGUI_NativeWindows_GDI_HDC',
  },
  {
    id: 'gdiplus',
    title: 'GDI+',
    status: 'done',
    statusText: '已完成',
    count: '3 章 · 抗锯齿到图像编解码',
    desc: '架构、坐标变换与矩阵、图像格式与编解码',
    link: '/native_win32/26_ProgrammingGUI_Graphics_GdiPlus_Architecture',
  },
  {
    id: 'direct2d',
    title: 'Direct2D / DirectWrite',
    status: 'done',
    statusText: '已完成',
    count: '5 章 · 硬件加速 2D 渲染',
    desc: '架构、几何体、效果与图层、高质量文字排版、GDI 互操作',
    link: '/native_win32/29_ProgrammingGUI_Graphics_Direct2D_Architecture',
  },
  {
    id: 'hlsl-d3d11',
    title: 'HLSL 与 Direct3D 11',
    status: 'done',
    statusText: '已完成',
    count: '9 章 · 着色器到 3D 渲染',
    desc: 'HLSL 语言、编译调试、CBuffer、D3D11 初始化、纹理、光照、混合',
    link: '/native_win32/34_ProgrammingGUI_Graphics_HLSL_Basics',
  },
  {
    id: 'd3d12',
    title: 'Direct3D 12',
    status: 'done',
    statusText: '已完成',
    count: '5 章 · 现代图形 API',
    desc: '设计哲学、命令队列、资源堆、描述符与根签名、D3D11 互操作',
    link: '/native_win32/43_ProgrammingGUI_Graphics_D3D12_Philosophy',
  },
  {
    id: 'custom-opengl',
    title: '自定义控件与 OpenGL',
    status: 'done',
    statusText: '已完成',
    count: '4 章 · 自绘到跨平台',
    desc: 'Owner-Draw、完全自绘、命中测试、Win32 嵌入 OpenGL',
    link: '/native_win32/48_ProgrammingGUI_Graphics_CustomCtrl_OwnerDraw',
  },
  {
    id: 'advanced',
    title: 'Win32 进阶',
    status: 'done',
    statusText: '已完成',
    count: '9 章 · 高级消息到系统通信',
    desc: '触控、Raw Input、子类化、Hook、托盘、拖放、定时器、消息机制',
    link: '/native_win32/52_ProgrammingGUI_NativeWindows_AdvancedMessages',
  },
  {
    id: 'miniui',
    title: 'MiniUI 实战',
    status: 'done',
    statusText: '已完成',
    count: '10 章 · 从 XCB 到线程池',
    desc: '用 XCB+Cairo 从零手搓教学级 GUI 框架：事件循环、控件树、布局引擎、信号系统、渲染管线、响应式与线程池',
    link: '/hands_on_ur_own_gui/',
  },
]
</script>

<template>
  <div class="roadmap">
    <h2 class="roadmap-title">学习路线</h2>
    <div class="roadmap-track">
      <a
        v-for="(layer, i) in layers"
        :key="layer.id"
        :href="withBase(layer.link)"
        class="roadmap-node"
        :class="[layer.status, { last: i === layers.length - 1 }]"
      >
        <div class="node-indicator">
          <span v-if="layer.status === 'done'" class="node-check">&#10003;</span>
          <span v-else class="node-dot" />
        </div>
        <div v-if="i < layers.length - 1" class="node-line" />
        <div class="node-body">
          <div class="node-header">
            <span class="node-title">{{ layer.title }}</span>
            <span class="node-badge" :class="layer.status">{{ layer.statusText }}</span>
          </div>
          <span class="node-count">{{ layer.count }}</span>
          <p class="node-desc">{{ layer.desc }}</p>
        </div>
      </a>
    </div>
  </div>
</template>

<style scoped>
.roadmap {
  max-width: 720px;
  margin: 3rem auto 0;
  padding: 0 1.5rem;
}

.roadmap-title {
  text-align: center;
  font-size: 1.25rem;
  font-weight: 600;
  margin-bottom: 2rem;
  color: var(--vp-c-text-1);
}

.roadmap-track {
  display: flex;
  flex-direction: column;
}

.roadmap-node {
  display: flex;
  align-items: flex-start;
  gap: 1rem;
  position: relative;
  text-decoration: none;
  color: inherit;
  padding: 0.75rem 1rem;
  border-radius: 10px;
  transition: background 0.2s;
}

.roadmap-node:hover {
  background: var(--vp-c-default-soft);
}

.node-indicator {
  display: flex;
  flex-direction: column;
  align-items: center;
  flex-shrink: 0;
  width: 28px;
  position: relative;
  z-index: 1;
}

.node-check {
  width: 28px;
  height: 28px;
  border-radius: 50%;
  background: var(--vp-c-green-1);
  color: #fff;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
  font-weight: 700;
}

.node-dot {
  width: 14px;
  height: 14px;
  border-radius: 50%;
  border: 3px solid var(--vp-c-brand-1);
  background: var(--vp-c-bg);
  margin: 7px 0;
}

.node-line {
  position: absolute;
  left: 13px;
  top: 36px;
  bottom: -8px;
  width: 2px;
  background: var(--vp-c-divider);
}

.roadmap-node.last .node-line {
  display: none;
}

.node-body {
  flex: 1;
  min-width: 0;
}

.node-header {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  margin-bottom: 0.15rem;
}

.node-title {
  font-size: 1.05rem;
  font-weight: 600;
  color: var(--vp-c-text-1);
}

.node-badge {
  font-size: 0.7rem;
  font-weight: 600;
  padding: 1px 8px;
  border-radius: 20px;
  line-height: 1.6;
  letter-spacing: 0.02em;
}

.node-badge.done {
  background: var(--vp-c-green-soft);
  color: var(--vp-c-green-1);
}

.node-badge.progress {
  background: var(--vp-c-brand-soft);
  color: var(--vp-c-brand-1);
}

.node-count {
  font-size: 0.8rem;
  color: var(--vp-c-text-3);
}

.node-desc {
  font-size: 0.85rem;
  color: var(--vp-c-text-2);
  margin: 0.2rem 0 0;
  line-height: 1.5;
}

@media (max-width: 639px) {
  .roadmap {
    padding: 0 0.75rem;
    margin-top: 2rem;
  }
  .roadmap-node {
    padding: 0.6rem 0.5rem;
  }
}
</style>
