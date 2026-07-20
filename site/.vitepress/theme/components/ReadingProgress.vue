<script setup lang="ts">
import { ref, onMounted, onBeforeUnmount } from 'vue'
import { subscribeAfterRouteChange } from '../router-hooks'

// 顶部阅读进度条：滚动时按 scrollTop / (scrollHeight - clientHeight) 算百分比。
// brand 渐变 + 微光，固定在视口顶部。路由切换重算（短页也要归零）。
const progress = ref(0)

function update() {
  const el = document.documentElement
  const scrollTop = el.scrollTop || document.body.scrollTop
  const scrollHeight = el.scrollHeight - el.clientHeight
  progress.value = scrollHeight > 0 ? Math.min(100, (scrollTop / scrollHeight) * 100) : 0
}

onMounted(() => {
  update()
  window.addEventListener('scroll', update, { passive: true })
  window.addEventListener('resize', update, { passive: true })
})

subscribeAfterRouteChange(() => {
  requestAnimationFrame(update)
  setTimeout(update, 300)
})

onBeforeUnmount(() => {
  window.removeEventListener('scroll', update)
  window.removeEventListener('resize', update)
})
</script>

<template>
  <div class="reading-progress" aria-hidden="true">
    <div class="reading-progress__bar" :style="{ width: progress + '%' }" />
  </div>
</template>

<style scoped>
.reading-progress {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  height: 3px;
  z-index: 100;
  pointer-events: none;
  background: transparent;
}

.reading-progress__bar {
  height: 100%;
  background: linear-gradient(90deg, var(--vp-c-brand-1), var(--vp-c-brand-3));
  box-shadow: 0 0 8px rgba(181, 57, 74, 0.35);
  transition: width 0.12s ease-out;
}

@media print {
  .reading-progress { display: none; }
}
</style>
