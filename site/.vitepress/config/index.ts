import { defineConfig } from 'vitepress'
import { navZh } from './nav'
import { buildSidebar } from './sidebar'
import { cppTemplateEscapePlugin } from '../plugins/escape-cpp-templates'
import { viteCppEscape } from '../plugins/vite-escape-cpp'

export default defineConfig({
  srcDir: '../tutorial',

  title: 'anatomy_gui 文档',
  description: '面向 C++ 开发者的图形界面编程完整路线图',
  lang: 'zh-CN',
  base: '/anatomy_gui/',
  cleanUrls: true,
  lastUpdated: true,

  vite: {
    build: {
      chunkSizeWarningLimit: 5000,
    },
    plugins: [viteCppEscape()],
  },

  vue: {
    template: {
      compilerOptions: {
        isCustomElement: (tag: string) => tag.includes('-') || tag.includes('.'),
      },
    },
  },

  head: [
    ['link', { rel: 'icon', href: '/anatomy_gui/favicon.ico' }],
  ],

  markdown: {
    lineNumbers: true,
    theme: {
      light: 'github-light',
      dark: 'github-dark',
    },
    languages: ['c'],
    languageAlias: {
      rc: 'c',
    },
    config(md) {
      cppTemplateEscapePlugin(md)
    },
  },

  themeConfig: {
    nav: navZh,
    sidebar: buildSidebar(),

    search: {
      provider: 'local',
    },

    editLink: {
      pattern: 'https://github.com/Charliechen114514/anatomy_gui/edit/main/tutorial/:path',
      text: '在 GitHub 上编辑此页',
    },

    footer: {
      message: '基于 VitePress 构建',
      copyright: 'Copyright 2025-2026 Charliechen',
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/Charliechen114514/anatomy_gui' },
    ],
  },
})
