import type { PluginSimple } from 'markdown-it'
import type MarkdownIt from 'markdown-it'

/**
 * 长代码折叠：把超过阈值行数的代码块包成
 * <div class="vp-code-fold"><details><summary/></details><代码块/></div>
 * 用原生 <details> 做开关、纯 CSS :has() 控展开。零 JS、无 JS 也能展开、零 FOUC。
 *
 * 关键：整段包 <details>（含 VitePress 的 copy/行号/preWrapper），保持 copy 按钮的
 * nextElementSibling 链不断；代码放 <details> 外、与 <details> 同级，靠 CSS :has 控制 display。
 * code-group 内的 fence 不折叠（tab 本身已是折叠语义）。
 *
 * 阈值 20：20 行以内不误伤，超过的（大段演示/完整工程）才折。改这处常量全局调档。
 */
const FOLD_THRESHOLD = 20

export const codeFoldPlugin: PluginSimple = (md: MarkdownIt) => {
  // ① core ruler：标记 code-group 内的 fence，折叠时跳过
  md.core.ruler.push('code_fold_mark_codegroup', (state) => {
    let depth = 0
    for (const token of state.tokens) {
      if (token.type === 'container_code-group_open') {
        depth++
      } else if (token.type === 'container_code-group_close') {
        if (depth > 0) depth--
      } else if (depth > 0 && token.type === 'fence') {
        if (!token.meta) token.meta = {}
        token.meta.inCodeGroup = true
      }
    }
    return true
  })

  // ② 覆写 fence：整段包 <details>
  const originalFence = md.renderer.rules.fence
  if (!originalFence) return

  md.renderer.rules.fence = (tokens, idx, options, env, self) => {
    const html = originalFence(tokens, idx, options, env, self)
    const token = tokens[idx]

    if (token.meta && token.meta.inCodeGroup) return html

    const body = token.content.replace(/\n$/, '')
    const lineCount = body === '' ? 0 : body.split('\n').length
    if (lineCount <= FOLD_THRESHOLD) return html

    return (
      `<div class="vp-code-fold" data-lines="${lineCount}">` +
      `<details><summary><span class="vp-cf-closed">展开代码 <em>(共 ${lineCount} 行)</em></span>` +
      `<span class="vp-cf-open">收起代码</span></summary></details>` +
      html +
      `</div>`
    )
  }
}
