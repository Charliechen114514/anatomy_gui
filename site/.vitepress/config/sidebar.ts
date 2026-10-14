import type { DefaultTheme } from 'vitepress'
import { readdirSync, readFileSync, existsSync } from 'fs'
import { join } from 'path'

type SidebarItem = DefaultTheme.SidebarItem

const DOCS_ROOT = join(import.meta.dirname, '../../../tutorial')

function extractTitle(filePath: string): string | null {
  try {
    const content = readFileSync(filePath, 'utf-8')
    const fmMatch = content.match(/^---[\s\S]*?^title:\s*['"]?(.+?)['"]?\s*$/m)
    if (fmMatch) return fmMatch[1]
    const h1 = content.match(/^#\s+(.+)$/m)
    if (h1) return h1[1].replace(/\{.*?\}/g, '').trim()
  } catch { /* ignore */ }
  return null
}

function humanize(name: string): string {
  return name
    .replace(/^\d+[-_]?/, '')
    .replace(/[-_]/g, ' ')
    .replace(/\b\w/g, c => c.toUpperCase())
}

function sortEntries(a: string, b: string): number {
  const na = a.match(/^(\d+)/)?.[1]
  const nb = b.match(/^(\d+)/)?.[1]
  if (na && nb) return parseInt(na) - parseInt(nb)
  if (na) return -1
  if (nb) return 1
  return a.localeCompare(b, 'zh-CN')
}

function scanAct0(dir: string): SidebarItem[] {
  let entries: string[]
  try {
    entries = readdirSync(dir)
      .filter(e => !e.startsWith('.') && e.endsWith('.md') && e !== 'index.md')
      .sort(sortEntries)
  } catch { return [] }

  if (entries.length === 0) return []

  return [
    {
      text: 'Act 0 · GUI 理论篇',
      collapsed: false,
      items: entries.map(name => {
        const title = extractTitle(join(dir, name)) || humanize(name.replace(/\.md$/, ''))
        return {
          text: title,
          link: `/act0-theory/${name.replace(/\.md$/, '')}`,
        }
      }),
    },
  ]
}

export function buildSidebar(): DefaultTheme.Sidebar {
  const sidebar: DefaultTheme.Sidebar = {}

  const act0Dir = join(DOCS_ROOT, 'act0-theory')
  if (existsSync(act0Dir)) {
    sidebar['/act0-theory/'] = scanAct0(act0Dir)
  }

  // ModernWindows 系列：线性分阶段，按文件名前缀数字排序（00_、01_、…）。
  const modernWindowsDir = join(DOCS_ROOT, 'modern_windows')
  if (existsSync(modernWindowsDir)) {
    let entries: string[]
    try {
      entries = readdirSync(modernWindowsDir)
        .filter(e => !e.startsWith('.') && e.endsWith('.md') && e !== 'index.md')
        .sort(sortEntries)
    } catch { entries = [] }

    if (entries.length > 0) {
      sidebar['/modern_windows/'] = entries.map(name => {
        const fullPath = join(modernWindowsDir, name)
        const title = extractTitle(fullPath) || humanize(name.replace(/\.md$/, ''))
        return { text: title, link: `/modern_windows/${name.replace(/\.md$/, '')}` }
      })
    }
  }

  // 造骨架（主线）：按文件名前缀数字排序。
  const guiDir = join(DOCS_ROOT, 'gui')
  if (existsSync(guiDir)) {
    let entries: string[]
    try {
      entries = readdirSync(guiDir)
        .filter(e => !e.startsWith('.') && e.endsWith('.md') && e !== 'index.md')
        .sort(sortEntries)
    } catch { entries = [] }

    if (entries.length > 0) {
      sidebar['/gui/'] = entries.map(name => {
        const fullPath = join(guiDir, name)
        const title = extractTitle(fullPath) || humanize(name.replace(/\.md$/, ''))
        return { text: title, link: `/gui/${name.replace(/\.md$/, '')}` }
      })
    }
  }

  return sidebar
}