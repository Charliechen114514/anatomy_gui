import DefaultTheme from 'vitepress/theme'
import { h } from 'vue'
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

export default {
  extends: DefaultTheme,
  Layout() {
    return h(DefaultTheme.Layout, null, {
      'home-hero-before': () => h(AnatomyHero),
      'home-features-after': () => h(HomeRoadmap),
      'nav-bar-content-after': () => h(FontSizeSwitcher),
      'nav-screen-content-after': () => h(FontSizeSwitcher),
      'layout-top': () => [h(ReadingProgress), h(ResizableSidebar)],
    })
  },
  setup() {
    setupMermaid()
  },
  enhanceApp({ app }) {
    app.component('ChapterHero', ChapterHero)
    app.component('OrganCard', OrganCard)
  },
} satisfies Theme
