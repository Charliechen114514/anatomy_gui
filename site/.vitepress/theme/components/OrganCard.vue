<script setup>
import { computed } from 'vue'
import { withBase } from 'vitepress'

const props = defineProps({
  organNumber: { type: Number, required: true },
  name: { type: String, required: true },
  latin: { type: String, default: '' },
  oneLine: { type: String, required: true },
  link: { type: String, default: '' },
})

const ROMAN = ['', 'I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X', 'XI', 'XII']
const roman = computed(() => ROMAN[props.organNumber] || String(props.organNumber))
</script>

<template>
  <component
    :is="link ? 'a' : 'div'"
    :href="link ? withBase(link) : undefined"
    class="organ-card"
  >
    <span class="organ-card__rail" />
    <div class="organ-card__main">
      <span class="organ-card__num">{{ roman }}</span>
      <h3 class="organ-card__name">
        {{ name }}
        <span v-if="latin" class="organ-card__latin">{{ latin }}</span>
      </h3>
      <p class="organ-card__line">{{ oneLine }}</p>
    </div>
  </component>
</template>

<style scoped>
.organ-card {
  position: relative;
  display: flex;
  gap: 0.75rem;
  padding: 1rem 1.1rem 1rem 0.85rem;
  background: var(--vp-c-bg);
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  text-decoration: none;
  color: inherit;
  transition: border-color 0.25s ease, transform 0.25s ease, box-shadow 0.25s ease;
}

a.organ-card:hover {
  border-color: var(--vp-c-brand-1);
  transform: translateY(-3px);
  box-shadow: 0 8px 20px rgba(0, 0, 0, 0.06);
}

.organ-card__rail {
  width: 3px;
  flex-shrink: 0;
  border-radius: 2px;
  background: var(--vp-c-brand-1);
  opacity: 0.5;
  transition: opacity 0.25s ease, width 0.25s ease;
}

a.organ-card:hover .organ-card__rail {
  opacity: 1;
  width: 5px;
}

.organ-card__main {
  flex: 1;
  min-width: 0;
}

.organ-card__num {
  display: block;
  font-size: 0.78rem;
  font-weight: 700;
  letter-spacing: 0.12em;
  color: var(--vp-c-brand-1);
  margin-bottom: 0.25rem;
}

.organ-card__name {
  font-size: 1.02rem;
  font-weight: 600;
  margin: 0 0 0.3rem;
  color: var(--vp-c-text-1);
  line-height: 1.35;
  border: none;
  padding: 0;
}

.organ-card__latin {
  font-size: 0.75rem;
  font-style: italic;
  font-weight: 400;
  color: var(--vp-c-text-3);
  margin-left: 0.4rem;
}

.organ-card__line {
  font-size: 0.85rem;
  color: var(--vp-c-text-2);
  margin: 0;
  line-height: 1.55;
}
</style>
