import type { PluginSimple } from 'markdown-it'
import type MarkdownIt from 'markdown-it'

export const mermaidPlugin: PluginSimple = (md: MarkdownIt) => {
  // 把 mermaid fence token 改成自定义类型，Shiki 就不会把它当代码块高亮。
  // core rules 在 tokenize 之后、render 之前跑，所以无论 VitePress 何时覆盖 fence renderer 都有效。
  md.core.ruler.push('mermaid_block', (state) => {
    for (let i = 0; i < state.tokens.length; i++) {
      const token = state.tokens[i]
      if (token.type === 'fence' && token.info.trim() === 'mermaid') {
        token.type = 'mermaid_diagram'
        token.tag = ''
        token.nesting = 0
      }
    }
    return true
  })

  md.renderer.rules.mermaid_diagram = (tokens, idx) => {
    const encoded = encodeURIComponent(tokens[idx].content.trim())
    return `<div class="mermaid-diagram" data-mermaid="${encoded}" data-rendered="false"></div>`
  }
}
