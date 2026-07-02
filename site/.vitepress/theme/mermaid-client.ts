import { nextTick, onMounted } from 'vue'
import { useRouter } from 'vitepress'

declare global {
  interface Window {
    mermaid?: {
      initialize: (config: Record<string, unknown>) => void
      render: (id: string, text: string) => Promise<{ svg: string; bindFunctions?: (el: Element) => void }>
    }
    __mermaidLoadingPromise__?: Promise<void>
    __mermaidInitialized__?: boolean
  }
}

const MERMAID_CDN = 'https://cdn.jsdelivr.net/npm/mermaid@10.9.6/dist/mermaid.min.js'

// 解剖学配色 themeVariables（呼应站点主题：骨白底 / 解剖红强调 / 深炭文）
const LIGHT_VARS = {
  fontSize: '15px',
  primaryColor: '#F7F3EC',
  primaryTextColor: '#2A2622',
  primaryBorderColor: '#B5394A',
  lineColor: '#8A8077',
  secondaryColor: '#EFE8DC',
  tertiaryColor: '#FBF8F2',
  tertiaryBorderColor: '#D6CDBF',
  background: '#F7F3EC',
  mainBkg: '#F7F3EC',
  altBackground: '#EFE8DC',
  clusterBkg: '#EFE8DC',
  clusterBorder: '#D6CDBF',
}

const DARK_VARS = {
  fontSize: '15px',
  primaryColor: '#1F1B18',
  primaryTextColor: '#E0D8CA',
  primaryBorderColor: '#D85968',
  lineColor: '#7F776C',
  secondaryColor: '#24201D',
  tertiaryColor: '#161412',
  tertiaryBorderColor: '#3D3733',
  background: '#161412',
  mainBkg: '#1F1B18',
  altBackground: '#24201D',
  clusterBkg: '#1F1B18',
  clusterBorder: '#3D3733',
}

function isDark() {
  return typeof document !== 'undefined'
    && document.documentElement.classList.contains('dark')
}

function loadMermaid(): Promise<void> {
  if (typeof window === 'undefined') return Promise.resolve()

  if (window.mermaid) {
    initMermaid()
    return Promise.resolve()
  }

  if (window.__mermaidLoadingPromise__) return window.__mermaidLoadingPromise__

  window.__mermaidLoadingPromise__ = new Promise<void>((resolve, reject) => {
    const existing = document.querySelector<HTMLScriptElement>('script[data-mermaid-runtime]')
    if (existing) {
      existing.addEventListener('load', () => { initMermaid(); resolve() })
      existing.addEventListener('error', () => reject(new Error('Failed to load Mermaid')))
      return
    }

    const script = document.createElement('script')
    script.src = MERMAID_CDN
    script.async = true
    script.dataset.mermaidRuntime = 'true'
    script.onload = () => { initMermaid(); resolve() }
    script.onerror = () => reject(new Error(`Failed to load Mermaid from ${MERMAID_CDN}`))
    document.head.appendChild(script)
  })

  return window.__mermaidLoadingPromise__
}

function initMermaid() {
  if (!window.mermaid || window.__mermaidInitialized__) return
  window.mermaid.initialize({
    startOnLoad: false,
    securityLevel: 'loose',
    theme: 'base',
    flowchart: {
      htmlLabels: true,
      nodeSpacing: 50,
      rankSpacing: 50,
      padding: 15,
    },
    themeVariables: isDark() ? DARK_VARS : LIGHT_VARS,
  })
  window.__mermaidInitialized__ = true
}

async function renderMermaidDiagrams() {
  if (typeof window === 'undefined') return

  await loadMermaid()
  await nextTick()
  await new Promise<void>((r) => requestAnimationFrame(() => r()))

  const mermaid = window.mermaid
  if (!mermaid) return

  const nodes = Array.from(
    document.querySelectorAll<HTMLElement>('.mermaid-diagram[data-rendered="false"]')
  )

  for (let i = 0; i < nodes.length; i++) {
    const el = nodes[i]
    const raw = el.dataset.mermaid
    if (!raw) continue

    const source = decodeURIComponent(raw)
    const id = `mermaid-${Date.now()}-${i}-${Math.random().toString(36).slice(2, 8)}`

    try {
      const { svg } = await mermaid.render(id, source)
      el.innerHTML = svg
      el.dataset.rendered = 'true'
    } catch {
      el.dataset.rendered = 'error'
      el.innerHTML = `<pre class="mermaid-error">${escapeHtml(source)}</pre>`
    }
  }
}

function escapeHtml(s: string) {
  return s.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;').replaceAll("'", '&#39;')
}

export function setupMermaid() {
  const router = useRouter()

  onMounted(() => renderMermaidDiagrams())
  router.onAfterRouteChange = () => renderMermaidDiagrams()
}
