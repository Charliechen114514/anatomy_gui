<script setup lang="ts">
import { withBase } from 'vitepress'

const layers = [
  {
    id: 'act0-theory',
    title: 'Act 0 · GUI 理论篇',
    status: 'done',
    statusText: '已完成',
    count: '10 篇 · 12 器官世界观',
    desc: '保留 vs 即时、命令/声明/响应、事件循环、可访问性、隐藏成本——建立看穿任何框架的眼睛',
    link: '/act0-theory/',
  },
  {
    id: 'gui',
    title: '造骨架 · 从具象到抽象',
    status: 'done',
    statusText: 'M1 完成',
    count: '8 篇 · 两个可执行文件',
    desc: '裸 Win32 → 五层封装 → 写回同一个程序；先攒痛点，再让抽象从痛处长出来',
    link: '/gui/',
  },
  {
    id: 'takeover',
    title: '接管阶段',
    status: 'progress',
    statusText: '连载中',
    count: '六档 · 一档一个标签',
    desc: '自绘、合成、文本、输入、无障碍、响应式——同一棵树，换掉一个器官的实现',
    link: '/gui/',
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
  background: var(--vp-c-brand-soft);
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
  background: var(--vp-c-brand-1);
  color: var(--vp-c-bg-elv);
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
  flex-wrap: wrap;
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
  background: var(--vp-c-brand-soft);
  color: var(--vp-c-brand-1);
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
