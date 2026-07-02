<script setup lang="ts">
import { onMounted, onBeforeUnmount, ref } from 'vue'

// 可拖拽侧栏宽度：左导航树 + 右大纲栏(TOC)。
// 左栏 --vp-sidebar-width 由 VitePress 全链路消费(sidebar 自身 / 正文 padding-left / 顶栏对齐)，
//   改这一个变量即全联动。handle 的定位与拖动计算读 .VPSidebar 的实际几何，故 left 用 JS 读
//   getBoundingClientRect / offsetLeft 精确设定。
// 右栏 --vp-aside-width 自定义变量(custom.css 覆盖 aside max-width)；右 handle absolute
//   注入 aside 内，MutationObserver 在路由切换重建 aside 时重新注入。
// 宽度持久化 localStorage；首屏防闪由 config head 内联脚本负责。

type Side = 'left' | 'right'
interface Dim { min: number; max: number; def: number; key: string; cssVar: string }

const CONF: Record<Side, Dim> = {
  left: { min: 200, max: 480, def: 272, key: 'vp-sidebar-width', cssVar: '--vp-sidebar-width' },
  right: { min: 180, max: 360, def: 256, key: 'vp-aside-width', cssVar: '--vp-aside-width' },
}

const leftHandle = ref<HTMLElement | null>(null)
const RIGHT_HANDLE_ID = 'rs-right-handle'

const clamp = (v: number, min: number, max: number) => Math.min(max, Math.max(min, v))
const applyVar = (side: Side, px: number) =>
  document.documentElement.style.setProperty(CONF[side].cssVar, px + 'px')
const persist = (side: Side, px: number) => {
  try { localStorage.setItem(CONF[side].key, String(px)) } catch { /* 隐私模式 / 配额 */ }
}

interface DragCtx {
  side: Side
  dim: Dim
  lastV: number
  handle: HTMLElement
  onMove: (e: MouseEvent) => void
  onUp: () => void
}
let drag: DragCtx | null = null

function startDrag(side: Side, e: MouseEvent) {
  e.preventDefault()
  const dim = CONF[side]
  const handle = e.currentTarget as HTMLElement
  handle.classList.add('is-active')
  document.body.classList.add('rs-resizing')

  // 用「位移」而非「绝对坐标」算新宽度（居中布局下绝对坐标会多算居中偏移）
  const startX = e.clientX
  const startWidth =
    parseInt(getComputedStyle(document.documentElement).getPropertyValue(dim.cssVar)) || dim.def

  const onMove = (ev: MouseEvent) => {
    const delta = ev.clientX - startX
    const v = clamp(
      Math.round(side === 'left' ? startWidth + delta : startWidth - delta),
      dim.min,
      dim.max
    )
    if (drag) drag.lastV = v
    applyVar(side, v)
    if (side === 'left' && drag?.handle) {
      drag.handle.style.left = ev.clientX + 'px'
    }
  }
  const onUp = () => {
    if (drag) persist(side, drag.lastV)
    handle.classList.remove('is-active')
    document.body.classList.remove('rs-resizing')
    document.removeEventListener('mousemove', onMove)
    document.removeEventListener('mouseup', onUp)
    drag = null
    if (side === 'left') updateLeftPosition()
  }
  drag = { side, dim, lastV: startWidth, handle, onMove, onUp }
  document.addEventListener('mousemove', onMove)
  document.addEventListener('mouseup', onUp)
}

function reset(side: Side) {
  applyVar(side, CONF[side].def)
  persist(side, CONF[side].def)
  if (side === 'left') updateLeftPosition()
}

function updateLeftVisibility() {
  if (!leftHandle.value) return
  leftHandle.value.style.display = document.querySelector('.VPSidebar') ? '' : 'none'
}

function updateLeftPosition() {
  if (!leftHandle.value) return
  const sb = document.querySelector('.VPSidebar') as HTMLElement | null
  if (sb) leftHandle.value.style.left = (sb.offsetLeft + sb.offsetWidth) + 'px'
}

function injectRightHandle() {
  const aside = document.querySelector('.VPDoc.has-aside .aside') as HTMLElement | null
  if (!aside || aside.querySelector('#' + RIGHT_HANDLE_ID)) return
  const handle = document.createElement('div')
  handle.id = RIGHT_HANDLE_ID
  handle.className = 'rs-handle rs-handle--right'
  handle.setAttribute('role', 'separator')
  handle.setAttribute('aria-orientation', 'vertical')
  handle.setAttribute('aria-label', '拖动调整右侧大纲栏宽度(双击重置)')
  handle.addEventListener('mousedown', (ev) => startDrag('right', ev))
  handle.addEventListener('dblclick', () => reset('right'))
  aside.style.position = 'relative'
  aside.appendChild(handle)
}

let observer: MutationObserver | null = null
let leftTimer = 0
const onMutate = () => {
  updateLeftVisibility()
  updateLeftPosition()
  injectRightHandle()
}

onMounted(() => {
  ;(['left', 'right'] as Side[]).forEach((side) => {
    const dim = CONF[side]
    try {
      const v = parseInt(localStorage.getItem(dim.key) || '')
      if (v >= dim.min && v <= dim.max) applyVar(side, v)
    } catch {}
  })
  onMutate()
  requestAnimationFrame(updateLeftPosition)
  leftTimer = window.setTimeout(updateLeftPosition, 350)
  window.addEventListener('resize', updateLeftPosition, { passive: true })
  if (document.readyState !== 'complete') {
    window.addEventListener('load', updateLeftPosition)
  }
  const root = document.querySelector('.VPContent') || document.body
  if ('MutationObserver' in window) {
    observer = new MutationObserver(onMutate)
    observer.observe(root, { childList: true, subtree: true })
  }
})

onBeforeUnmount(() => {
  observer?.disconnect()
  observer = null
  window.clearTimeout(leftTimer)
  window.removeEventListener('resize', updateLeftPosition)
  window.removeEventListener('load', updateLeftPosition)
  document.querySelectorAll('#' + RIGHT_HANDLE_ID).forEach((n) => n.remove())
  if (drag) {
    document.removeEventListener('mousemove', drag.onMove)
    document.removeEventListener('mouseup', drag.onUp)
    drag = null
  }
})
</script>

<template>
  <div
    ref="leftHandle"
    class="rs-handle rs-handle--left"
    role="separator"
    aria-orientation="vertical"
    aria-label="拖动调整左侧导航栏宽度(双击重置)"
    @mousedown="startDrag('left', $event)"
    @dblclick="reset('left')"
  ></div>
</template>
