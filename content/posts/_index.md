---
title: "Posts"
description: "Longer-form rendering articles and write-ups."
---

<style>
@keyframes fadeSlideUp {
  from { opacity: 0; transform: translateY(16px); }
  to   { opacity: 1; transform: translateY(0); }
}
@keyframes shimmer {
  0%   { background-position: -200% center; }
  100% { background-position: 200% center; }
}

.posts-hero {
  animation: fadeSlideUp .6s ease-out both;
  position: relative;
}
.posts-hero::before {
  content: '';
  position: absolute; inset: -1.5px; border-radius: 15px;
  background: linear-gradient(90deg, transparent 25%, rgba(99,102,241,.2) 50%, transparent 75%);
  background-size: 200% 100%;
  animation: shimmer 4s ease-in-out infinite;
  pointer-events: none;
  mask: linear-gradient(#fff 0 0) content-box, linear-gradient(#fff 0 0);
  mask-composite: exclude; -webkit-mask-composite: xor;
  padding: 1.5px;
}

.series-card {
  animation: fadeSlideUp .55s .15s ease-out both;
  transition: transform .2s ease, box-shadow .2s ease;
}
.series-card:hover {
  transform: translateY(-2px);
  box-shadow: 0 8px 24px rgba(99,102,241,.1);
}

.part-link {
  animation: fadeSlideUp .45s ease-out both;
  transition: transform .18s ease, border-color .18s ease, box-shadow .18s ease;
  text-decoration: none !important;
  color: inherit !important;
  display: block;
}
.part-link:nth-child(1) { animation-delay: .25s; }
.part-link:nth-child(2) { animation-delay: .35s; }
.part-link:nth-child(3) { animation-delay: .45s; }
.part-link:hover {
  transform: translateY(-2px);
  border-color: rgba(99,102,241,.35) !important;
  box-shadow: 0 4px 16px rgba(99,102,241,.08);
}
.part-link:hover .part-title {
  color: #6366f1;
}

.part-num {
  transition: transform .2s ease, background .2s ease;
}
.part-link:hover .part-num {
  transform: scale(1.1);
  background: #6366f1 !important;
}

@media (prefers-reduced-motion: reduce) {
  *, *::before, *::after {
    animation-duration: 0.01ms !important;
    animation-delay: 0ms !important;
    transition-duration: 0.01ms !important;
  }
}
</style>

## Rendering Architecture Series

<div class="series-card" style="margin:1.2em 0 2em;border-radius:12px;border:1.5px solid rgba(99,102,241,.18);overflow:hidden;background:linear-gradient(135deg,rgba(99,102,241,.03),transparent);">
<div style="padding:1em 1.3em;border-bottom:1px solid rgba(99,102,241,.1);display:flex;align-items:center;gap:.8em;flex-wrap:wrap;">
<span style="font-size:1.3em;">🏗️</span>
<div>
<div style="font-weight:800;font-size:1.05em;">Frame Graph</div>
<div style="font-size:.82em;opacity:.55;">3 parts · theory → implementation → production engines</div>
</div>
<div style="margin-left:auto;display:flex;gap:.4em;">
<span style="font-size:.72em;padding:.2em .5em;border-radius:5px;border:1px solid rgba(99,102,241,.15);background:rgba(99,102,241,.05);font-weight:600;">rendering</span>
<span style="font-size:.72em;padding:.2em .5em;border-radius:5px;border:1px solid rgba(99,102,241,.15);background:rgba(99,102,241,.05);font-weight:600;">gpu</span>
<span style="font-size:.72em;padding:.2em .5em;border-radius:5px;border:1px solid rgba(99,102,241,.15);background:rgba(99,102,241,.05);font-weight:600;">architecture</span>
</div>
</div>
<div style="display:grid;gap:0;">
<div class="part-link" onclick="window.location='/posts/frame-graph-theory/'" style="cursor:pointer;padding:.9em 1.3em;border-bottom:1px solid rgba(99,102,241,.07);display:flex;align-items:center;gap:1em;">
<div class="part-num" style="width:2em;height:2em;border-radius:50%;background:#3b82f6;display:flex;align-items:center;justify-content:center;color:#fff;font-weight:800;font-size:.85em;flex-shrink:0;">I</div>
<div style="flex:1;min-width:0;">
<div class="part-title" style="font-weight:700;font-size:.95em;transition:color .18s ease;">Theory</div>
<div style="font-size:.82em;opacity:.6;line-height:1.5;margin-top:.1em;">What a render graph is, what problems it solves, and why every major engine uses one.</div>
</div>
<div style="font-size:.78em;opacity:.4;flex-shrink:0;">12 min read</div>
</div>
<div class="part-link" onclick="window.location='/posts/frame-graph-build-it/'" style="cursor:pointer;padding:.9em 1.3em;border-bottom:1px solid rgba(99,102,241,.07);display:flex;align-items:center;gap:1em;">
<div class="part-num" style="width:2em;height:2em;border-radius:50%;background:#8b5cf6;display:flex;align-items:center;justify-content:center;color:#fff;font-weight:800;font-size:.85em;flex-shrink:0;">II</div>
<div style="flex:1;min-width:0;">
<div class="part-title" style="font-weight:700;font-size:.95em;transition:color .18s ease;">Build It</div>
<div style="font-size:.82em;opacity:.6;line-height:1.5;margin-top:.1em;">Three iterations from blank file to working frame graph with automatic barriers and memory aliasing.</div>
</div>
<div style="font-size:.78em;opacity:.4;flex-shrink:0;">30 min read</div>
</div>
<div class="part-link" onclick="window.location='/posts/frame-graph-production/'" style="cursor:pointer;padding:.9em 1.3em;display:flex;align-items:center;gap:1em;">
<div class="part-num" style="width:2em;height:2em;border-radius:50%;background:#22c55e;display:flex;align-items:center;justify-content:center;color:#fff;font-weight:800;font-size:.85em;flex-shrink:0;">III</div>
<div style="flex:1;min-width:0;">
<div class="part-title" style="font-weight:700;font-size:.95em;transition:color .18s ease;">Production Engines</div>
<div style="font-size:.82em;opacity:.6;line-height:1.5;margin-top:.1em;">How UE5, Frostbite, and Unity implement frame graphs at scale.</div>
</div>
<div style="font-size:.78em;opacity:.4;flex-shrink:0;">9 min read</div>
</div>
</div>
<div style="padding:.6em 1.3em;border-top:1px solid rgba(99,102,241,.1);background:rgba(99,102,241,.02);display:flex;align-items:center;gap:.6em;">
<div style="width:100%;height:4px;border-radius:2px;background:rgba(99,102,241,.08);overflow:hidden;">
<div style="width:100%;height:100%;border-radius:2px;background:linear-gradient(90deg,#3b82f6,#8b5cf6,#22c55e);"></div>
</div>
<span style="font-size:.75em;opacity:.45;white-space:nowrap;font-weight:600;">3 / 3</span>
</div>
</div>

---

## All Posts
