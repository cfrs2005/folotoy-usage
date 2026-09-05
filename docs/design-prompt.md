[简体中文](design-prompt.zh_CN.md)

# FoloToy instrument design brief

## Research

- [Wear OS design principles](https://developer.android.com/design/ui/wear/guides/get-started/design-for-wearables/principles): prioritize critical tasks, brief interactions, relevant information, and offline states.
- [Anthropic frontend design guidance](https://github.com/anthropics/skills/blob/main/skills/frontend-design/SKILL.md?plain=1): choose a subject-specific visual language, let structure convey meaning, and critique the result. This is inspiration, not a requirement to adopt web effects on an MCU.
- The local pinned LVGL 9.5 display implementation is the rendering authority: partial RGB565 strips and bounded memory, without a full framebuffer.

## Working prompts

### Product judgment

Design a desktop instrument, not a compressed website. In two seconds, the owner must identify the Claude and Codex limits, their respective reset times, and whether those observations are current. Preserve the personal avatar, name, local date/time, both providers on the home page, and a separate consumption page. The display is 240x320 and is controlled with three physical buttons. Do not invent additional touch controls or a bottom menu.

### Visual judgment

Give each provider a compact colored identity strip. Put the five-hour and weekly limits side by side so the same number has the same position in both providers. Use a dedicated monospaced numeral face and a quieter CJK text face. Make the quantity large, the period explicit and the reset secondary. Every line and color must encode identity, a quantity or state. Remove decoration that competes with the data.

### Hardware judgment

Use native LVGL objects and partial rendering. Reuse objects; do not allocate or delete screens on each refresh. Avoid full-frame transitions. Budget a fixed 32 KB UI pool and verify the actual board after font, style or object-count changes. Buttons must dispatch events without performing network or drawing work. Screenshots must come from actual device flush strips.

### Adversarial review

Test unknown quotas, zero usage, values over 100 percent, overdue resets, aged data, a disconnected network, a rejected display credential and large token/cost values. Old data must never imply an expired account. Refresh must never make old observations look new. Do not trust an attractive enlarged mockup: inspect the real 240x320 output, check clipping and perform repeated page/lens changes on the device.

## Decisions

- Home: two stacked instruments, two side-by-side quota columns each.
- Consumption: two providers, today and cumulative values retained together.
- UP/DOWN: switch pages. OK on Home: switch used/remaining. Hold OK: refresh; refresh does not reset upstream timestamps.
- Static, high-contrast presentation; color identifies provider and freshness. No animation is required to make the design readable.
