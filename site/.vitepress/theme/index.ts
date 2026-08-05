import DefaultTheme from 'vitepress/theme'
import { defineComponent, h } from 'vue'
import type { Theme } from 'vitepress'
import HomeRoadmap from './components/HomeRoadmap.vue'
import AnatomyHero from './components/AnatomyHero.vue'
import ResizableSidebar from './components/ResizableSidebar.vue'
import FontSizeSwitcher from './components/FontSizeSwitcher.vue'
import ReadingProgress from './components/ReadingProgress.vue'
import ChapterHero from './components/ChapterHero.vue'
import OrganCard from './components/OrganCard.vue'
import { setupMermaid } from './mermaid-client'
import './custom.css'

// Layout 必须是带 setup 的组件（defineComponent），把 setupMermaid 放进组件 setup，
// onAfterRouteChanged / onMounted 才会真正注册 —— 否则 mermaid-diagram div 永远空着。
const Layout = defineComponent({
  setup() {
    setupMermaid()
    return () =>
      h(DefaultTheme.Layout, null, {
        'home-hero-before': () => h(AnatomyHero),
        'home-features-after': () => h(HomeRoadmap),
        'nav-bar-content-after': () => h(FontSizeSwitcher),
        'nav-screen-content-after': () => h(FontSizeSwitcher),
        'layout-top': () => [h(ReadingProgress), h(ResizableSidebar)],
      })
  },
})

export default {
  extends: DefaultTheme,
  Layout,
  enhanceApp({ app }) {
    app.component('ChapterHero', ChapterHero)
    app.component('OrganCard', OrganCard)
  },
} satisfies Theme
