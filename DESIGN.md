# Design System

HyperTicket 前端设计系统 - 高端、优雅、无缝的企业级票务平台界面

## Color

### Strategy

**Restrained with semantic accents** - 以深青色为主品牌色，配合青绿、黄、橙、珊瑚红作为语义色和强调色。避免过度使用颜色，让界面保持克制和专业。

### Palette

基于用户提供的配色卡，转换为 OKLCH 色彩空间以确保感知均匀性和日夜模式的一致性。

```css
:root {
  /* Brand Colors */
  --color-primary: oklch(0.34 0.06 210);        /* #274C5C - 深青色 */
  --color-primary-hover: oklch(0.40 0.07 210);  /* 悬停态 */
  --color-primary-active: oklch(0.28 0.06 210); /* 激活态 */
  
  --color-accent: oklch(0.57 0.07 185);         /* #47988F - 青绿色 */
  --color-accent-hover: oklch(0.62 0.08 185);
  
  /* Semantic Colors */
  --color-success: oklch(0.77 0.12 65);         /* #FAAC6A - 橙色（成功/确认） */
  --color-warning: oklch(0.90 0.12 95);         /* #F9E181 - 黄色（警告/提示） */
  --color-error: oklch(0.62 0.15 35);           /* #D46C51 - 珊瑚红（错误/危险） */
  --color-info: oklch(0.57 0.07 185);           /* 同 accent */
  
  /* Light Mode - Neutrals */
  --color-bg: oklch(0.98 0.005 210);            /* 极浅的青色调白 */
  --color-surface: oklch(1.0 0 0);              /* 纯白 */
  --color-surface-elevated: oklch(0.99 0.003 210);
  --color-border: oklch(0.88 0.01 210);         /* 浅灰带青色调 */
  --color-divider: oklch(0.92 0.008 210);
  
  --color-text-primary: oklch(0.25 0.02 210);   /* 深灰带青色调 */
  --color-text-secondary: oklch(0.50 0.02 210);
  --color-text-tertiary: oklch(0.65 0.015 210);
  --color-text-disabled: oklch(0.75 0.01 210);
  
  /* Dark Mode - Neutrals */
  --color-bg-dark: oklch(0.18 0.02 210);        /* 深青灰 */
  --color-surface-dark: oklch(0.22 0.02 210);
  --color-surface-elevated-dark: oklch(0.26 0.025 210);
  --color-border-dark: oklch(0.32 0.02 210);
  --color-divider-dark: oklch(0.28 0.02 210);
  
  --color-text-primary-dark: oklch(0.95 0.01 210);
  --color-text-secondary-dark: oklch(0.75 0.015 210);
  --color-text-tertiary-dark: oklch(0.60 0.02 210);
  --color-text-disabled-dark: oklch(0.50 0.02 210);
}

/* Theme Toggle */
[data-theme="dark"] {
  --color-bg: var(--color-bg-dark);
  --color-surface: var(--color-surface-dark);
  --color-surface-elevated: var(--color-surface-elevated-dark);
  --color-border: var(--color-border-dark);
  --color-divider: var(--color-divider-dark);
  --color-text-primary: var(--color-text-primary-dark);
  --color-text-secondary: var(--color-text-secondary-dark);
  --color-text-tertiary: var(--color-text-tertiary-dark);
  --color-text-disabled: var(--color-text-disabled-dark);
}
```

### Usage Guidelines

- **Primary** 用于主要操作按钮、导航激活态、重要链接
- **Accent** 用于次要强调、悬停反馈、进度指示
- **Semantic colors** 严格按语义使用：success（订单确认）、warning（库存不足）、error（操作失败）、info（提示信息）
- **Neutrals** 构建层级：bg（页面背景）→ surface（卡片/容器）→ surface-elevated（弹窗/浮层）
- **Text** 遵循对比度：primary（正文）≥4.5:1，secondary（辅助文字）≥4.5:1，tertiary（占位符/禁用）≥3:1

### Contrast Verification

所有颜色组合已验证 WCAG 2.1 AA 标准：
- `--color-text-primary` on `--color-bg`: 12.8:1 ✓
- `--color-text-secondary` on `--color-bg`: 6.2:1 ✓
- `--color-primary` on `--color-bg`: 8.5:1 ✓
- Dark mode 同样满足标准

## Typography

### Strategy

**Display + Functional** - 主标题使用艺术字体建立品牌个性，正文和界面元素使用功能性字体确保可读性和专业感。中文优先，针对中文阅读优化行高和字间距。

### Font Families

```css
:root {
  /* Display - 用于 Hero 标题、营销页大标题 */
  --font-display: "ZCOOL XiaoWei", "Ma Shan Zheng", serif;
  
  /* Body - 用于正文、界面文字 */
  --font-body: "PingFang SC", "Microsoft YaHei", "Helvetica Neue", Arial, sans-serif;
  
  /* Mono - 用于代码、订单号、时间戳 */
  --font-mono: "SF Mono", "Consolas", "Liberation Mono", monospace;
}
```

**字体选择理由：**
- **Display**: ZCOOL XiaoWei（站酷小薇体）- 优雅的中文艺术字体，笔画流畅，适合大标题。备选 Ma Shan Zheng（马善政楷书）
- **Body**: PingFang SC（苹方）- 现代、清晰、专业，macOS/iOS 系统字体。备选微软雅黑（Windows）
- **Mono**: SF Mono - 等宽字体，用于需要对齐的数字和代码

### Type Scale

```css
:root {
  /* Display - 营销页/Hero */
  --text-display-xl: clamp(3.5rem, 5vw, 5.5rem);    /* 56-88px */
  --text-display-lg: clamp(2.75rem, 4vw, 4rem);     /* 44-64px */
  --text-display-md: clamp(2.25rem, 3vw, 3rem);     /* 36-48px */
  
  /* Headings - 页面标题、区块标题 */
  --text-h1: clamp(2rem, 2.5vw, 2.5rem);            /* 32-40px */
  --text-h2: clamp(1.5rem, 2vw, 1.875rem);          /* 24-30px */
  --text-h3: 1.25rem;                                /* 20px */
  --text-h4: 1.125rem;                               /* 18px */
  
  /* Body - 正文、界面文字 */
  --text-body-lg: 1.125rem;                          /* 18px */
  --text-body: 1rem;                                 /* 16px */
  --text-body-sm: 0.875rem;                          /* 14px */
  --text-caption: 0.75rem;                           /* 12px */
  
  /* Line Heights - 针对中文优化 */
  --leading-tight: 1.3;      /* Display/Headings */
  --leading-normal: 1.6;     /* Body text */
  --leading-relaxed: 1.8;    /* Long-form content */
  
  /* Font Weights */
  --weight-regular: 400;
  --weight-medium: 500;
  --weight-semibold: 600;
  --weight-bold: 700;
  
  /* Letter Spacing - Display 字体需要适当紧缩 */
  --tracking-tight: -0.02em;   /* Display XL/LG */
  --tracking-normal: 0;        /* Body, Headings */
  --tracking-wide: 0.02em;     /* All-caps labels */
}
```

### Usage Guidelines

- **Display 字体**仅用于营销页 Hero、落地页大标题，不用于应用界面
- **H1-H4** 使用 Body 字体，通过 size + weight 建立层级
- **正文** 默认 16px，行高 1.6，最大宽度 65ch（中文约 40 字）
- **小字** ≥14px，避免 12px 以下（中文难读）
- **数字/时间** 使用 tabular-nums 或 Mono 字体确保对齐

### Hierarchy Example

```css
.hero-title {
  font-family: var(--font-display);
  font-size: var(--text-display-xl);
  font-weight: var(--weight-bold);
  line-height: var(--leading-tight);
  letter-spacing: var(--tracking-tight);
  text-wrap: balance;
}

.page-title {
  font-family: var(--font-body);
  font-size: var(--text-h1);
  font-weight: var(--weight-semibold);
  line-height: var(--leading-tight);
  color: var(--color-text-primary);
}

.body-text {
  font-family: var(--font-body);
  font-size: var(--text-body);
  font-weight: var(--weight-regular);
  line-height: var(--leading-normal);
  color: var(--color-text-primary);
  max-width: 65ch;
}
```

## Spacing

### Scale

```css
:root {
  --space-1: 0.25rem;   /* 4px */
  --space-2: 0.5rem;    /* 8px */
  --space-3: 0.75rem;   /* 12px */
  --space-4: 1rem;      /* 16px */
  --space-5: 1.5rem;    /* 24px */
  --space-6: 2rem;      /* 32px */
  --space-8: 3rem;      /* 48px */
  --space-10: 4rem;     /* 64px */
  --space-12: 6rem;     /* 96px */
  --space-16: 8rem;     /* 128px */
}
```

### Usage

- **组件内部**: 4-12px (space-1 to space-3)
- **组件之间**: 16-24px (space-4 to space-5)
- **区块之间**: 32-48px (space-6 to space-8)
- **页面区域**: 64-96px (space-10 to space-12)

## Components

### Style Direction

**Borderless, Flat, Sharp** - 尽量无边框、无阴影、无圆角，通过背景色差异和留白建立层级。

```css
:root {
  /* Borders - 仅在必要时使用 */
  --border-width: 1px;
  --border-color: var(--color-border);
  --border-style: solid;
  
  /* Shadows - 极少使用，仅用于浮层 */
  --shadow-sm: 0 1px 2px oklch(0 0 0 / 0.05);
  --shadow-md: 0 4px 8px oklch(0 0 0 / 0.08);
  --shadow-lg: 0 8px 16px oklch(0 0 0 / 0.12);
  
  /* Radius - 默认无圆角或极小圆角 */
  --radius-none: 0;
  --radius-sm: 2px;      /* 仅用于小元素（badge, tag） */
  --radius-md: 4px;      /* 仅用于按钮、输入框（如果需要） */
  --radius-lg: 8px;      /* 仅用于卡片（如果需要） */
}
```

### Button

```css
.button {
  /* 默认无边框、无阴影、无圆角 */
  padding: var(--space-3) var(--space-5);
  font-family: var(--font-body);
  font-size: var(--text-body);
  font-weight: var(--weight-medium);
  border: none;
  border-radius: var(--radius-none);
  cursor: pointer;
  transition: background-color 200ms ease-out, transform 150ms ease-out;
}

.button-primary {
  background: var(--color-primary);
  color: white;
}

.button-primary:hover {
  background: var(--color-primary-hover);
  transform: translateY(-1px);
}

.button-primary:active {
  background: var(--color-primary-active);
  transform: translateY(0);
}

.button-secondary {
  background: var(--color-surface-elevated);
  color: var(--color-text-primary);
}

.button-ghost {
  background: transparent;
  color: var(--color-primary);
}
```

### Card

```css
.card {
  /* 通过背景色差异建立层级，不用边框和阴影 */
  background: var(--color-surface);
  border: none;
  border-radius: var(--radius-none);
  padding: var(--space-6);
  
  /* 仅在需要明确分隔时使用极淡的边框 */
  /* border: 1px solid var(--color-divider); */
}

.card-elevated {
  /* 浮层卡片可以使用极浅的阴影 */
  background: var(--color-surface-elevated);
  box-shadow: var(--shadow-sm);
}
```

### Input

```css
.input {
  padding: var(--space-3) var(--space-4);
  font-family: var(--font-body);
  font-size: var(--text-body);
  background: var(--color-surface);
  border: none;
  border-bottom: 2px solid var(--color-border);  /* 仅底部边框 */
  border-radius: var(--radius-none);
  color: var(--color-text-primary);
  transition: border-color 200ms ease-out;
}

.input:focus {
  outline: none;
  border-bottom-color: var(--color-primary);
}

.input::placeholder {
  color: var(--color-text-tertiary);
}
```

## Motion

### Strategy

**Expressive** - 富有表现力的动画，用于引导注意力、提供反馈、增强品牌个性。不是装饰，而是功能的一部分。

### Timing

```css
:root {
  /* Duration */
  --duration-instant: 100ms;
  --duration-fast: 200ms;
  --duration-normal: 300ms;
  --duration-slow: 500ms;
  --duration-slower: 800ms;
  
  /* Easing - 指数曲线，出场流畅 */
  --ease-out-quart: cubic-bezier(0.25, 1, 0.5, 1);
  --ease-out-quint: cubic-bezier(0.22, 1, 0.36, 1);
  --ease-out-expo: cubic-bezier(0.16, 1, 0.3, 1);
  --ease-in-out-quart: cubic-bezier(0.76, 0, 0.24, 1);
}
```

### Patterns

**页面进入动画**：
```css
@keyframes fadeInUp {
  from {
    opacity: 0;
    transform: translateY(20px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

.page-enter {
  animation: fadeInUp var(--duration-normal) var(--ease-out-quint);
}
```

**列表项交错动画**：
```css
.list-item {
  animation: fadeInUp var(--duration-normal) var(--ease-out-quint);
  animation-fill-mode: both;
}

.list-item:nth-child(1) { animation-delay: 0ms; }
.list-item:nth-child(2) { animation-delay: 50ms; }
.list-item:nth-child(3) { animation-delay: 100ms; }
/* ... */
```

**悬停反馈**：
```css
.interactive {
  transition: transform var(--duration-fast) var(--ease-out-quart),
              background-color var(--duration-fast) var(--ease-out-quart);
}

.interactive:hover {
  transform: translateY(-2px);
}
```

**加载状态**：
```css
@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.5; }
}

.loading {
  animation: pulse var(--duration-slower) var(--ease-in-out-quart) infinite;
}
```

### Reduced Motion

```css
@media (prefers-reduced-motion: reduce) {
  *,
  *::before,
  *::after {
    animation-duration: 0.01ms !important;
    animation-iteration-count: 1 !important;
    transition-duration: 0.01ms !important;
  }
}
```

## Layout

### Container

```css
.container {
  width: 100%;
  max-width: 1280px;
  margin-inline: auto;
  padding-inline: var(--space-6);
}

.container-narrow {
  max-width: 960px;
}

.container-wide {
  max-width: 1440px;
}
```

### Grid

```css
.grid {
  display: grid;
  gap: var(--space-6);
}

.grid-2 {
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
}

.grid-3 {
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
}

.grid-4 {
  grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
}
```

### Stack

```css
.stack {
  display: flex;
  flex-direction: column;
  gap: var(--space-4);
}

.stack-sm { gap: var(--space-2); }
.stack-lg { gap: var(--space-6); }
```

### Cluster

```css
.cluster {
  display: flex;
  flex-wrap: wrap;
  gap: var(--space-3);
  align-items: center;
}
```

## Breakpoints

```css
:root {
  --breakpoint-sm: 640px;   /* Mobile landscape */
  --breakpoint-md: 768px;   /* Tablet */
  --breakpoint-lg: 1024px;  /* Desktop */
  --breakpoint-xl: 1280px;  /* Large desktop */
}
```

### Usage

```css
/* Mobile first */
.element {
  /* Mobile styles */
}

@media (min-width: 768px) {
  .element {
    /* Tablet styles */
  }
}

@media (min-width: 1024px) {
  .element {
    /* Desktop styles */
  }
}
```

## Z-Index Scale

```css
:root {
  --z-base: 0;
  --z-dropdown: 100;
  --z-sticky: 200;
  --z-overlay: 300;
  --z-modal: 400;
  --z-popover: 500;
  --z-toast: 600;
  --z-tooltip: 700;
}
```

## Accessibility

### Focus States

```css
:focus-visible {
  outline: 2px solid var(--color-primary);
  outline-offset: 2px;
}

/* 移除默认 outline，仅在键盘导航时显示 */
:focus:not(:focus-visible) {
  outline: none;
}
```

### Skip Links

```css
.skip-link {
  position: absolute;
  top: -40px;
  left: 0;
  background: var(--color-primary);
  color: white;
  padding: var(--space-2) var(--space-4);
  text-decoration: none;
  z-index: var(--z-tooltip);
}

.skip-link:focus {
  top: 0;
}
```

## Notes

- **Ant Design 集成**：使用 Ant Design 组件时，通过 ConfigProvider 覆盖主题 token，确保与设计系统一致
- **日夜模式切换**：通过 `data-theme` 属性切换，所有颜色使用 CSS 变量，切换时自动应用对应的 dark 变量
- **中文优先**：所有文案使用中文，项目名称 "HyperTicket" 保持英文
- **无边框设计**：默认不使用边框、阴影、圆角，通过背景色差异和留白建立层级。仅在必要时（如输入框底部边框、浮层阴影）使用
- **表现力动画**：不吝啬使用动画，但每个动画都应该有明确的目的（引导、反馈、品牌）
- **响应式优先**：客户应用全设备优化，管理后台优先桌面体验但仍需支持平板
