import type { DefaultTheme } from 'vitepress'
import { readdirSync, statSync, readFileSync, existsSync } from 'fs'
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

interface ChapterGroup {
  title: string
  prefix: string
}

const WIN32_GROUPS: ChapterGroup[] = [
  { title: '基础篇', prefix: '0' },
  { title: '控件篇', prefix: '8' },
  { title: '对话框篇', prefix: '11' },
  { title: '资源篇', prefix: '17' },
  { title: '工具栏与状态栏', prefix: '17_5' },
  { title: 'GDI 图形篇', prefix: '25' },
  { title: 'GDI+ 篇', prefix: '28' },
  { title: 'Direct2D / DirectWrite 篇', prefix: '33' },
  { title: 'HLSL 着色器篇', prefix: '36' },
  { title: 'Direct3D 11 篇', prefix: '42' },
  { title: 'Direct3D 12 篇', prefix: '47' },
  { title: '自定义控件篇', prefix: '50' },
  { title: 'OpenGL 篇', prefix: '51' },
  { title: '高级消息篇', prefix: '52' },
  { title: 'Win32 进阶篇', prefix: '60' },
]

function getGroupIndex(filename: string): number {
  const numStr = filename.match(/^(\d+(?:_\d+)?)/)?.[1]
  if (!numStr) return -1
  const num = parseFloat(numStr.replace('_', '.'))

  for (let i = WIN32_GROUPS.length - 1; i >= 0; i--) {
    const groupNum = parseFloat(WIN32_GROUPS[i].prefix.replace('_', '.'))
    if (num >= groupNum) return i
  }
  return 0
}

function scanNativeWin32(dir: string): SidebarItem[] {
  let entries: string[]
  try {
    entries = readdirSync(dir).filter(e =>
      !e.startsWith('.') &&
      e.endsWith('.md') &&
      e !== 'index.md'
    )
  } catch { return [] }

  entries.sort(sortEntries)

  const groups = new Map<number, { title: string; items: SidebarItem[] }>()

  for (const name of entries) {
    const fullPath = join(dir, name)
    const groupIdx = getGroupIndex(name)

    if (!groups.has(groupIdx)) {
      groups.set(groupIdx, {
        title: WIN32_GROUPS[groupIdx].title,
        items: [],
      })
    }

    const title = extractTitle(fullPath) || humanize(name.replace(/\.md$/, ''))
    groups.get(groupIdx)!.items.push({
      text: title,
      link: `/native_win32/${name.replace(/\.md$/, '')}`,
    })
  }

  const result: SidebarItem[] = []
  for (const [_, group] of groups) {
    result.push({
      text: group.title,
      items: group.items,
      collapsed: false,
    })
  }

  return result
}

export function buildSidebar(): DefaultTheme.Sidebar {
  const sidebar: DefaultTheme.Sidebar = {}

  const nativeWin32Dir = join(DOCS_ROOT, 'native_win32')
  if (existsSync(nativeWin32Dir)) {
    sidebar['/native_win32/'] = scanNativeWin32(nativeWin32Dir)
  }

  const bonusDir = join(DOCS_ROOT, 'bonus')
  if (existsSync(bonusDir)) {
    let entries: string[]
    try {
      entries = readdirSync(bonusDir)
        .filter(e => !e.startsWith('.') && e.endsWith('.md') && e !== 'index.md')
        .sort(sortEntries)
    } catch { entries = [] }

    if (entries.length > 0) {
      sidebar['/bonus/'] = entries.map(name => {
        const fullPath = join(bonusDir, name)
        const title = extractTitle(fullPath) || humanize(name.replace(/\.md$/, ''))
        return { text: title, link: `/bonus/${name.replace(/\.md$/, '')}` }
      })
    }
  }

  return sidebar
}
