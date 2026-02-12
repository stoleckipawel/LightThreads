---
title: "Frame Graph — Theory"
date: 2026-02-09
draft: false
description: "What a render graph is, what problems it solves, and why every major engine uses one."
tags: ["rendering", "frame-graph", "gpu", "architecture"]
categories: ["analysis"]
series: ["Rendering Architecture"]
showTableOfContents: false
---

{{< article-nav >}}

<div style="margin:0 0 1.5em;padding:.7em 1em;border-radius:8px;background:rgba(99,102,241,.04);border:1px solid rgba(99,102,241,.12);font-size:.88em;line-height:1.6;opacity:.85;">
📖 <strong>Part I of III.</strong>&ensp; <em>Theory</em> → <a href="../frame-graph-build-it/">Build It</a> → <a href="../frame-graph-production/">Production Engines</a>
</div>

*What a render graph is, what problems it solves, and why every major engine uses one.*

---

## 🎯 Why You Want One

<div class="fg-reveal" style="margin:1.2em 0 1.5em;padding:1.3em 1.5em;border-radius:12px;border:1.5px solid rgba(99,102,241,.18);background:linear-gradient(135deg,rgba(99,102,241,.04),rgba(34,197,94,.03));">
  <div style="display:grid;grid-template-columns:1fr auto 1fr;gap:.3em .8em;align-items:center;font-size:1em;line-height:1.6;">
    <span style="text-decoration:line-through;opacity:.4;text-align:right;">Passes run in whatever order you wrote them.</span>
    <span style="opacity:.35;">→</span>
    <strong>Sorted by dependencies.</strong>
    <span style="text-decoration:line-through;opacity:.4;text-align:right;">Every GPU sync point placed by hand.</span>
    <span style="opacity:.35;">→</span>
    <strong>Barriers inserted for you.</strong>
    <span style="text-decoration:line-through;opacity:.4;text-align:right;">Each pass allocates its own memory — 900 MB gone.</span>
    <span style="opacity:.35;">→</span>
    <strong style="color:#22c55e;">Resources shared safely — ~450 MB back.</strong>
  </div>
  <div style="margin-top:.8em;padding-top:.7em;border-top:1px solid rgba(99,102,241,.1);font-size:.88em;opacity:.7;line-height:1.5;text-align:center;">
    You describe <em>what</em> each pass needs — the graph figures out the <em>how</em>.
  </div>
</div>

Frostbite introduced it at GDC 2017. UE5 ships it as **RDG**. Every major renderer uses one — this series shows you why, walks you through building your own in C++, and maps every piece to what ships in production engines.

<div class="fg-reveal" style="margin:1.5em 0;border-radius:12px;overflow:hidden;border:1.5px solid rgba(99,102,241,.25);background:linear-gradient(135deg,rgba(99,102,241,.04),transparent);">
  <div style="display:grid;grid-template-columns:repeat(3,1fr);gap:0;">
    <div style="padding:1em;text-align:center;border-right:1px solid rgba(99,102,241,.12);border-bottom:1px solid rgba(99,102,241,.12);">
      <div style="font-size:1.6em;margin-bottom:.15em;">�</div>
      <div style="font-weight:800;font-size:.95em;">Learn Theory</div>
      <div style="font-size:.82em;opacity:.7;line-height:1.4;margin-top:.2em;">What a frame graph is, why every engine uses one, and how each piece works</div>
    </div>
    <div style="padding:1em;text-align:center;border-right:1px solid rgba(99,102,241,.12);border-bottom:1px solid rgba(99,102,241,.12);">
      <div style="font-size:1.6em;margin-bottom:.15em;">🔨</div>
      <div style="font-weight:800;font-size:.95em;">Build MVP</div>
      <div style="font-size:.82em;opacity:.7;line-height:1.4;margin-top:.2em;">Working C++ frame graph, from scratch to prototype in ~300 lines</div>
    </div>
    <div style="padding:1em;text-align:center;border-bottom:1px solid rgba(99,102,241,.12);">
      <div style="font-size:1.6em;margin-bottom:.15em;">🗺️</div>
      <div style="font-weight:800;font-size:.95em;">Map to UE5</div>
      <div style="font-size:.82em;opacity:.7;line-height:1.4;margin-top:.2em;">Every piece maps to RDG — read the source with confidence</div>
    </div>
  </div>
</div>

If you've watched VRAM spike from non-overlapping textures or chased a black screen after reordering a pass — this is for you.

---

## 🔥 The Problem

<div class="fg-reveal" style="position:relative;margin:1.4em 0;padding-left:2.2em;border-left:3px solid var(--color-neutral-300,#d4d4d4);">

  <div style="margin-bottom:1.6em;">
    <div class="fg-dot-bounce" style="position:absolute;left:-0.8em;width:1.4em;height:1.4em;border-radius:50%;background:#22c55e;display:flex;align-items:center;justify-content:center;font-size:.7em;color:#fff;font-weight:700;">1</div>
    <div style="font-weight:800;font-size:1.05em;color:#22c55e;margin-bottom:.3em;">Month 1 — 3 passes, everything's fine</div>
    <div style="font-size:.92em;line-height:1.6;">
      Depth prepass → GBuffer → lighting. Two barriers, hand-placed. Two textures, both allocated at init. Code is clean, readable, correct.
    </div>
    <div style="margin-top:.4em;padding:.4em .8em;border-radius:6px;background:rgba(34,197,94,.06);font-size:.88em;font-style:italic;border-left:3px solid #22c55e;">
      At this scale, manual management works. You know every resource by name.
    </div>
  </div>

  <div style="margin-bottom:1.6em;">
    <div class="fg-dot-bounce" style="position:absolute;left:-0.8em;width:1.4em;height:1.4em;border-radius:50%;background:#f59e0b;display:flex;align-items:center;justify-content:center;font-size:.7em;color:#fff;font-weight:700;">6</div>
    <div style="font-weight:800;font-size:1.05em;color:#f59e0b;margin-bottom:.3em;">Month 6 — 12 passes, cracks appear</div>
    <div style="font-size:.92em;line-height:1.6;">
      Same renderer, now with SSAO, SSR, bloom, TAA, shadow cascades. Three things going wrong simultaneously:
    </div>
    <div style="margin-top:.5em;display:grid;gap:.4em;">
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(245,158,11,.2);background:rgba(245,158,11,.04);font-size:.88em;line-height:1.5;">
        <strong>Invisible dependencies</strong> — someone adds SSAO but doesn't realize GBuffer needs an updated barrier. Visual artifacts on fresh build.
      </div>
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(245,158,11,.2);background:rgba(245,158,11,.04);font-size:.88em;line-height:1.5;">
        <strong>Wasted memory</strong> — SSAO and bloom textures never overlap, but aliasing them means auditing every pass that might touch them. Nobody does it.
      </div>
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(245,158,11,.2);background:rgba(245,158,11,.04);font-size:.88em;line-height:1.5;">
        <strong>Silent reordering</strong> — two branches touch the render loop. Git merges cleanly, but the shadow pass ends up after lighting. Subtly wrong output ships unnoticed.
      </div>
    </div>
    <div style="margin-top:.5em;padding:.4em .8em;border-radius:6px;background:rgba(245,158,11,.06);font-size:.88em;font-style:italic;border-left:3px solid #f59e0b;">
      No single change broke it. The accumulation broke it.
    </div>
  </div>

  <div>
    <div class="fg-dot-bounce" style="position:absolute;left:-0.8em;width:1.4em;height:1.4em;border-radius:50%;background:#ef4444;display:flex;align-items:center;justify-content:center;font-size:.65em;color:#fff;font-weight:700;">18</div>
    <div style="font-weight:800;font-size:1.05em;color:#ef4444;margin-bottom:.3em;">Month 18 — 25 passes, nobody touches it</div>
    <div style="font-size:.92em;line-height:1.6;margin-bottom:.5em;">The renderer works, but:</div>
    <div style="display:grid;gap:.4em;">
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(239,68,68,.2);background:rgba(239,68,68,.04);font-size:.88em;line-height:1.5;">
        <strong>900 MB VRAM.</strong> Profiling shows 400 MB is aliasable — but the lifetime analysis would take a week and break the next time anyone adds a pass.
      </div>
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(239,68,68,.2);background:rgba(239,68,68,.04);font-size:.88em;line-height:1.5;">
        <strong>47 barrier calls.</strong> Three are redundant, two are missing, one is in the wrong queue. Nobody knows which.
      </div>
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(239,68,68,.2);background:rgba(239,68,68,.04);font-size:.88em;line-height:1.5;">
        <strong>2 days to add a new pass.</strong> 30 minutes for the shader, the rest to figure out where to slot it and what barriers it needs.
      </div>
    </div>
    <div style="margin-top:.5em;padding:.4em .8em;border-radius:6px;background:rgba(239,68,68,.06);font-size:.88em;font-style:italic;border-left:3px solid #ef4444;">
      The renderer isn't wrong. It's <em>fragile</em>. Every change is a risk.
    </div>
  </div>

</div>

<div class="diagram-bars" style="grid-template-columns:110px 1fr 1fr 1fr;gap:0.3em 0.6em;font-size:.8em">
  <div class="db-label"></div>
  <div style="font-weight:700;text-align:center">Month 1</div>
  <div style="font-weight:700;text-align:center">Month 6</div>
  <div style="font-weight:700;text-align:center">Month 18</div>
  <div class="db-label">Passes</div>
  <div><div class="db-bar" style="width:12%;min-width:18px"></div><span class="db-val">3</span></div>
  <div><div class="db-bar db-warn" style="width:48%"></div><span class="db-val">12</span></div>
  <div><div class="db-bar db-danger" style="width:100%"></div><span class="db-val">25</span></div>
  <div class="db-label">Barriers</div>
  <div><div class="db-bar" style="width:4%;min-width:18px"></div><span class="db-val">2</span></div>
  <div><div class="db-bar db-warn" style="width:38%"></div><span class="db-val">18</span></div>
  <div><div class="db-bar db-danger" style="width:100%"></div><span class="db-val">47</span></div>
  <div class="db-label">VRAM</div>
  <div><div class="db-bar" style="width:4%;min-width:18px"></div><span class="db-val">~40 MB</span></div>
  <div><div class="db-bar db-warn" style="width:42%"></div><span class="db-val">380 MB</span></div>
  <div><div class="db-bar db-danger" style="width:100%"></div><span class="db-val">900 MB</span></div>
  <div class="db-label">Aliasable</div>
  <div><div class="db-bar" style="width:0%;min-width:3px;opacity:.3"></div><span class="db-val">0</span></div>
  <div><div class="db-bar db-warn" style="width:20%"></div><span class="db-val">~80 MB</span></div>
  <div><div class="db-bar db-danger" style="width:100%"></div><span class="db-val">400 MB</span></div>
  <div class="db-label">Status</div>
  <div style="color:#22c55e;font-weight:700">✓ manageable</div>
  <div style="color:#f59e0b;font-weight:700">⚠ fragile</div>
  <div style="color:#ef4444;font-weight:700">✗ untouchable</div>
</div>

The pattern is always the same: manual resource management works at small scale and fails at compound scale. Not because engineers are sloppy — because *no human tracks 25 lifetimes and 47 transitions in their head every sprint*. You need a system that sees the whole frame at once.

---

## 💡 The Core Idea

A frame graph is a **directed acyclic graph (DAG)** — each node is a render pass, each edge is a resource one pass hands to the next. Here's what a typical deferred frame looks like:

<!-- DAG flow diagram -->
<div style="margin:1.2em 0 .3em;">
<div class="diagram-flow" style="justify-content:center">
  <div class="df-step df-primary">Depth<br>Prepass<span class="df-sub">depth</span></div>
  <div class="df-arrow"></div>
  <div class="df-step df-primary">GBuffer<br>Pass<span class="df-sub">albedo · normals · depth</span></div>
  <div class="df-arrow"></div>
  <div class="df-step" style="display:flex;flex-direction:column;gap:.3em;padding:.5em .8em">
    <div class="df-step df-primary" style="border:none;padding:.3em .6em;font-size:.9em">SSAO<span class="df-sub">occlusion</span></div>
    <div style="opacity:.4;font-size:.75em;">↕</div>
    <div class="df-step df-primary" style="border:none;padding:.3em .6em;font-size:.9em">Lighting<span class="df-sub">HDR color</span></div>
  </div>
  <div class="df-arrow"></div>
  <div class="df-step df-success">Tonemap<span class="df-sub">→ present</span></div>
</div>
<div style="text-align:center;margin-top:.1em;">
  <span style="display:inline-block;font-size:.76em;opacity:.55;border:1px solid rgba(99,102,241,.15);border-radius:6px;padding:.25em .7em;">nodes = passes &nbsp;·&nbsp; edges = resource flow &nbsp;·&nbsp; arrows = write → read</span>
</div>
</div>

You don't execute this graph directly. Every frame goes through three steps — first you **declare** all the passes and what they read/write, then the system **compiles** an optimized plan (ordering, memory, barriers), and finally it **executes** the result:

<!-- 3-step lifecycle — distinct style from the DAG above -->
<div class="fg-reveal" style="margin:.8em auto 1.2em;max-width:560px;">
  <div class="fg-lifecycle" style="display:flex;align-items:stretch;gap:0;border-radius:10px;overflow:hidden;border:1.5px solid rgba(99,102,241,.2);">
    <a href="#-the-declare-step" aria-label="Jump to Declare section" style="flex:1;padding:.7em .6em;text-align:center;background:rgba(59,130,246,.06);border-right:1px solid rgba(99,102,241,.12);text-decoration:none;color:inherit;transition:background .2s ease;cursor:pointer;" onmouseover="this.style.background='rgba(59,130,246,.14)'" onmouseout="this.style.background='rgba(59,130,246,.06)'">
      <div style="font-weight:800;font-size:.88em;letter-spacing:.04em;color:#3b82f6;">①&ensp;DECLARE</div>
      <div style="font-size:.75em;opacity:.6;margin-top:.2em;">passes &amp; dependencies</div>
    </a>
    <a href="#-the-compile-step" aria-label="Jump to Compile section" style="flex:1;padding:.7em .6em;text-align:center;background:rgba(139,92,246,.06);border-right:1px solid rgba(99,102,241,.12);text-decoration:none;color:inherit;transition:background .2s ease;cursor:pointer;" onmouseover="this.style.background='rgba(139,92,246,.14)'" onmouseout="this.style.background='rgba(139,92,246,.06)'">
      <div style="font-weight:800;font-size:.88em;letter-spacing:.04em;color:#8b5cf6;">②&ensp;COMPILE</div>
      <div style="font-size:.75em;opacity:.6;margin-top:.2em;">order · aliases · barriers</div>
    </a>
    <a href="#-the-execute-step" aria-label="Jump to Execute section" style="flex:1;padding:.7em .6em;text-align:center;background:rgba(34,197,94,.06);text-decoration:none;color:inherit;transition:background .2s ease;cursor:pointer;" onmouseover="this.style.background='rgba(34,197,94,.14)'" onmouseout="this.style.background='rgba(34,197,94,.06)'">
      <div style="font-weight:800;font-size:.88em;letter-spacing:.04em;color:#22c55e;">③&ensp;EXECUTE</div>
      <div style="font-size:.75em;opacity:.6;margin-top:.2em;">record GPU commands</div>
    </a>
  </div>
</div>

Let's look at each step.

---

## 📋 The Declare Step

Each frame starts on the CPU. You register passes, describe the resources they need, and declare who reads or writes what. No GPU work happens yet — you're building a description of the frame.

<div class="diagram-box">
  <div class="db-title">📋 DECLARE — building the graph</div>
  <div class="db-body">
    <div class="diagram-pipeline">
      <div class="dp-stage">
        <div class="dp-title">ADD PASSES</div>
        <ul><li><code>addPass(setup, execute)</code></li><li>setup runs now, execute runs later</li></ul>
      </div>
      <div class="dp-stage">
        <div class="dp-title">DECLARE RESOURCES</div>
        <ul><li><code>create({1920,1080, RGBA8})</code></li><li>returns a handle — no allocation yet</li></ul>
      </div>
      <div class="dp-stage">
        <div class="dp-title">WIRE DEPENDENCIES</div>
        <ul><li><code>read(h)</code> / <code>write(h)</code></li><li>these edges form the DAG</li></ul>
      </div>
    </div>
    <div style="text-align:center;font-size:.82em;opacity:.6;margin-top:.3em">CPU only — the GPU is idle during this phase</div>
  </div>
</div>

<div class="fg-reveal" style="margin:1.2em 0;padding:1.1em 1.3em;border-radius:10px;border:1.5px dashed rgba(99,102,241,.3);background:rgba(99,102,241,.04);display:flex;align-items:center;gap:1.2em;flex-wrap:wrap;">
  <div style="flex:1;min-width:180px;">
    <div style="font-size:1.15em;font-weight:800;margin:.1em 0;">Handle #3</div>
    <div style="font-size:.82em;opacity:.6;">1920×1080 · RGBA8 · render target</div>
  </div>
  <div style="flex-shrink:0;padding:.4em .8em;border-radius:6px;background:rgba(245,158,11,.1);color:#f59e0b;font-weight:700;font-size:.8em;">description only — no GPU memory yet</div>
</div>
<div style="text-align:center;font-size:.82em;opacity:.6;margin-top:-.2em;">
  Resources stay virtual at this stage — just a description and a handle. Memory comes later.
</div>

### 📦 Transient vs. imported

When you declare a resource, the graph needs to know one thing: **does it live inside this frame, or does it come from outside?**

<div class="fg-grid-stagger" style="display:grid;grid-template-columns:1fr 1fr;gap:1em;margin:1.2em 0;">
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(59,130,246,.3);overflow:hidden;">
    <div style="padding:.6em .9em;font-weight:800;font-size:.95em;background:rgba(59,130,246,.08);border-bottom:1px solid rgba(59,130,246,.15);color:#3b82f6;">⚡ Transient</div>
    <div style="padding:.7em .9em;font-size:.88em;line-height:1.7;">
      <strong>Lifetime:</strong> single frame<br>
      <strong>Declared as:</strong> description (size, format)<br>
      <strong>GPU memory:</strong> aliasing planned at compile, physically backed at execute<br>
      <strong>Aliasable:</strong> <span style="color:#22c55e;font-weight:700;">Yes</span> — non-overlapping lifetimes share physical memory<br>
      <strong>Examples:</strong> GBuffer MRTs, SSAO scratch, bloom scratch
    </div>
  </div>
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(139,92,246,.3);overflow:hidden;">
    <div style="padding:.6em .9em;font-weight:800;font-size:.95em;background:rgba(139,92,246,.08);border-bottom:1px solid rgba(139,92,246,.15);color:#8b5cf6;">📌 Imported</div>
    <div style="padding:.7em .9em;font-size:.88em;line-height:1.7;">
      <strong>Lifetime:</strong> across frames<br>
      <strong>Declared as:</strong> existing GPU handle<br>
      <strong>GPU memory:</strong> already allocated externally<br>
      <strong>Aliasable:</strong> <span style="color:#ef4444;font-weight:700;">No</span> — lifetime extends beyond the frame<br>
      <strong>Examples:</strong> backbuffer, TAA history, shadow atlas, blue noise LUT
    </div>
  </div>
</div>

### ⚠️ Aliasing pitfalls

Aliasing is one of the graph's biggest VRAM wins — but it has sharp edges:

<div class="diagram-warn">
  <div class="dw-title">⚠ Aliasing pitfalls</div>
  <div class="dw-row"><div class="dw-label">Format compat</div><div>depth/stencil metadata may conflict with color targets on same VkMemory → skip aliasing for depth formats</div></div>
  <div class="dw-row"><div class="dw-label">Initialization</div><div>reused memory = garbage contents → first use <strong>MUST</strong> be a full clear or fullscreen write</div></div>
  <div class="dw-row"><div class="dw-label">Imported res</div><div>survive across frames — <strong>never alias</strong>. Only transient resources qualify.</div></div>
</div>

Aliasing requires **placed resources** — you allocate a large `ID3D12Heap` (or `VkDeviceMemory`), then bind multiple resources at different offsets within it. Non-overlapping lifetimes land on the same physical memory. Production engines refine this further with **power-of-two bucketing** (reducing heap fragmentation) and **cross-frame pooling** (keeping heaps alive across frames so allocation cost amortizes to near zero). [Part III](/posts/frame-graph-production/) covers how each engine handles these.

---

## ⚙️ The Compile Step

The declared DAG goes in, an optimized execution plan comes out. Three things happen — all near-linear, all in microseconds for a typical frame.

<div class="diagram-box">
  <div class="db-title">🔍 COMPILE — turning the DAG into a plan</div>
  <div class="db-body">
    <div class="diagram-pipeline">
      <div class="dp-stage">
        <div class="dp-title">SCHEDULE</div>
        <ul><li>topological sort (Kahn's)</li><li>cycle detection</li><li>→ final pass order</li></ul>
      </div>
      <div class="dp-stage">
        <div class="dp-title">ALLOCATE</div>
        <ul><li>sort transients by first use</li><li>greedy-fit into physical slots</li><li>→ alias map</li></ul>
      </div>
      <div class="dp-stage">
        <div class="dp-title">SYNCHRONIZE</div>
        <ul><li>walk each resource handoff</li><li>emit minimal barriers</li><li>→ barrier list</li></ul>
      </div>
      <div class="dp-stage">
        <div class="dp-title">BACK RESOURCES</div>
        <ul><li>create or reuse physical memory</li><li>apply the alias map</li><li>→ physical bindings</li></ul>
      </div>
    </div>
    <div style="text-align:center;font-size:.82em;opacity:.6;margin-top:.3em">Still CPU — all decisions made before the GPU sees a single command</div>
  </div>
</div>

<div class="fg-reveal" style="margin:1.2em 0;display:flex;align-items:center;gap:1em;flex-wrap:wrap;">
  <div style="flex:1;min-width:180px;padding:1em 1.2em;border-radius:10px;border:1.5px dashed rgba(99,102,241,.3);background:rgba(99,102,241,.04);">
    <div style="font-size:1.1em;font-weight:800;">Handle #3</div>
    <div style="font-size:.8em;opacity:.5;">1920×1080 · RGBA8</div>
    <div style="margin-top:.4em;font-size:.75em;padding:.25em .6em;border-radius:6px;background:rgba(245,158,11,.1);color:#f59e0b;font-weight:700;display:inline-block;">virtual</div>
  </div>
  <div class="fg-resolve-arrow" style="font-size:1.4em;opacity:.3;flex-shrink:0;">→</div>
  <div style="flex:1;min-width:180px;padding:1em 1.2em;border-radius:10px;border:1.5px solid rgba(34,197,94,.3);background:rgba(34,197,94,.04);">
    <div style="font-size:1.1em;font-weight:800;">Handle #3 <span style="opacity:.35;">→</span> <span style="color:#22c55e;">Pool slot 0</span></div>
    <div style="font-size:.8em;opacity:.5;">shares 8 MB with Handle #7</div>
    <div style="margin-top:.4em;font-size:.75em;padding:.25em .6em;border-radius:6px;background:rgba(34,197,94,.1);color:#22c55e;font-weight:700;display:inline-block;">aliased</div>
  </div>
</div>
<div style="text-align:center;font-size:.82em;opacity:.6;margin-top:-.2em;">
  The compiler sees every resource lifetime at once — non-overlapping handles land on the same physical memory.
</div>

---

## ▶️ The Execute Step

The plan is ready — now the GPU gets involved. Every decision has already been made during compile: pass order, memory layout, barriers, physical resource bindings. Execute just walks the plan.

<div class="diagram-box">
  <div class="db-title">▶️ EXECUTE — recording GPU commands</div>
  <div class="db-body">
    <div class="diagram-pipeline">
      <div class="dp-stage">
        <div class="dp-title">RUN PASSES</div>
        <ul><li>for each pass in compiled order:</li><li>insert barriers → call <code>execute()</code></li></ul>
      </div>
      <div class="dp-stage">
        <div class="dp-title">CLEANUP</div>
        <ul><li>release transients (or pool them)</li><li>reset the frame allocator</li></ul>
      </div>
    </div>
    <div style="text-align:center;font-size:.82em;opacity:.6;margin-top:.3em">The only phase that touches the GPU API — resources already bound</div>
  </div>
</div>

<div class="fg-reveal" style="margin:1.2em 0;padding:1em 1.2em;border-radius:10px;border:1.5px solid rgba(34,197,94,.2);background:rgba(34,197,94,.04);font-size:.92em;line-height:1.6;">
  Each execute lambda sees a <strong>fully resolved environment</strong> — barriers already placed, memory already allocated, resources ready to bind. The lambda just records draw calls, dispatches, and copies. All the intelligence lives in the compile step.
</div>

---

## 🔄 Rebuild Strategies

How often should the graph recompile? Three approaches, each a valid tradeoff:

<div class="fg-grid-stagger" style="display:grid;grid-template-columns:repeat(3,1fr);gap:1em;margin:1.2em 0;">
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid #22c55e;overflow:hidden;">
    <div style="padding:.5em .8em;background:rgba(34,197,94,.1);font-weight:800;font-size:.95em;border-bottom:1px solid rgba(34,197,94,.2);">
      🔄 Dynamic
    </div>
    <div style="padding:.7em .8em;font-size:.88em;line-height:1.6;">
      Rebuild every frame.<br>
      <strong>Cost:</strong> microseconds<br>
      <strong>Flex:</strong> full — passes appear/disappear freely<br>
      <span style="opacity:.6;font-size:.9em;">Used by: Frostbite</span>
    </div>
  </div>
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid #3b82f6;overflow:hidden;">
    <div style="padding:.5em .8em;background:rgba(59,130,246,.1);font-weight:800;font-size:.95em;border-bottom:1px solid rgba(59,130,246,.2);">
      ⚡ Hybrid
    </div>
    <div style="padding:.7em .8em;font-size:.88em;line-height:1.6;">
      Cache compiled result, invalidate on change.<br>
      <strong>Cost:</strong> near-zero on hit<br>
      <strong>Flex:</strong> full + bookkeeping<br>
      <span style="opacity:.6;font-size:.9em;">Used by: UE5</span>
    </div>
  </div>
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid var(--color-neutral-400,#9ca3af);overflow:hidden;">
    <div style="padding:.5em .8em;background:rgba(156,163,175,.1);font-weight:800;font-size:.95em;border-bottom:1px solid rgba(156,163,175,.2);">
      🔒 Static
    </div>
    <div style="padding:.7em .8em;font-size:.88em;line-height:1.6;">
      Compile once at init, replay forever.<br>
      <strong>Cost:</strong> zero<br>
      <strong>Flex:</strong> none — fixed pipeline<br>
      <span style="opacity:.6;font-size:.9em;">Rare in practice</span>
    </div>
  </div>
</div>

Most engines use **dynamic** or **hybrid**. The compile is so cheap that caching buys little — but some engines do it anyway to skip redundant barrier recalculation.

---

## 💰 The Payoff

<div class="fg-compare" style="margin:1.2em 0;display:grid;grid-template-columns:1fr 1fr;gap:0;border-radius:10px;overflow:hidden;border:2px solid rgba(99,102,241,.25);box-shadow:0 2px 8px rgba(0,0,0,.08);">
  <div style="padding:.6em 1em;font-weight:800;font-size:.95em;background:rgba(239,68,68,.1);border-bottom:1.5px solid rgba(99,102,241,.15);border-right:1.5px solid rgba(99,102,241,.15);color:#ef4444;">❌ Without Graph</div>
  <div style="padding:.6em 1em;font-weight:800;font-size:.95em;background:rgba(34,197,94,.1);border-bottom:1.5px solid rgba(99,102,241,.15);color:#22c55e;">✅ With Graph</div>

  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(99,102,241,.1);border-right:1.5px solid rgba(99,102,241,.15);background:rgba(239,68,68,.02);">
    <strong>Memory aliasing</strong><br><span style="opacity:.65">Opt-in, fragile, rarely done</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(99,102,241,.1);background:rgba(34,197,94,.02);">
    <strong>Memory aliasing</strong><br>Automatic — compiler sees all lifetimes. <strong style="color:#22c55e;">30–50% VRAM saved.</strong>
  </div>

  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(99,102,241,.1);border-right:1.5px solid rgba(99,102,241,.15);background:rgba(239,68,68,.02);">
    <strong>Lifetimes</strong><br><span style="opacity:.65">Manual create/destroy, leaked or over-retained</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(99,102,241,.1);background:rgba(34,197,94,.02);">
    <strong>Lifetimes</strong><br>Scoped to first..last use. Zero waste.
  </div>

  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(99,102,241,.1);border-right:1.5px solid rgba(99,102,241,.15);background:rgba(239,68,68,.02);">
    <strong>Barriers</strong><br><span style="opacity:.65">Manual, per-pass</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(99,102,241,.1);background:rgba(34,197,94,.02);">
    <strong>Barriers</strong><br>Automatic from declared read/write
  </div>

  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(99,102,241,.1);border-right:1.5px solid rgba(99,102,241,.15);background:rgba(239,68,68,.02);">
    <strong>Pass reordering</strong><br><span style="opacity:.65">Breaks silently</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(99,102,241,.1);background:rgba(34,197,94,.02);">
    <strong>Pass reordering</strong><br>Safe — compiler respects dependencies
  </div>

  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(99,102,241,.1);border-right:1.5px solid rgba(99,102,241,.15);background:rgba(239,68,68,.02);">
    <strong>Pass culling</strong><br><span style="opacity:.65">Manual ifdef / flag checks</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(99,102,241,.1);background:rgba(34,197,94,.02);">
    <strong>Pass culling</strong><br>Automatic — unused outputs = dead pass
  </div>

  <div style="padding:.55em .8em;font-size:.88em;border-right:1.5px solid rgba(99,102,241,.15);background:rgba(239,68,68,.02);">
    <strong>Async compute</strong><br><span style="opacity:.65">Manual queue sync</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;background:rgba(34,197,94,.02);">
    <strong>Async compute</strong><br>Compiler schedules across queues
  </div>
</div>

<div class="fg-reveal" style="margin:1.2em 0;padding:.8em 1em;border-radius:8px;background:linear-gradient(135deg,rgba(34,197,94,.06),rgba(59,130,246,.06));border:1px solid rgba(34,197,94,.2);font-size:.92em;line-height:1.6;">
🏭 <strong>Not theoretical.</strong> Frostbite reported <strong>50% VRAM reduction</strong> from aliasing at GDC 2017. UE5's RDG ships the same optimization today — every <code>FRDGTexture</code> marked as transient goes through the same aliasing pipeline we build in <a href="../frame-graph-build-it/">Part II</a>.
</div>

---

## 🔬 Advanced Features

The core graph handles scheduling, barriers, and aliasing — but the same DAG enables the compiler to go further. It can merge adjacent render passes to eliminate redundant state changes, schedule independent work across GPU queues, and split barrier transitions to hide cache-flush latency. [Part II](/posts/frame-graph-build-it/) builds the core; [Part III](/posts/frame-graph-production/) shows how production engines deploy all of these.

### 🔗 Pass Merging

<div class="fg-reveal" style="margin:0 0 1.2em;padding:.85em 1.1em;border-radius:10px;border:1px solid rgba(99,102,241,.15);background:linear-gradient(135deg,rgba(99,102,241,.04),transparent);font-size:.92em;line-height:1.65;">
Every render pass boundary has a cost — the GPU resolves attachments, flushes caches, stores intermediate results to memory, and sets up state for the next pass. When two adjacent passes share the same render targets, that boundary is pure overhead. <strong>Pass merging</strong> fuses compatible passes into a single API render pass, eliminating the round-trip entirely.
</div>

<div class="fg-grid-stagger" style="display:grid;grid-template-columns:1fr 1fr;gap:1em;margin:1.2em 0">
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(239,68,68,.2);background:rgba(239,68,68,.03);padding:1em 1.1em;">
    <div style="font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:#ef4444;margin-bottom:.6em;">Without merging</div>
    <div style="font-size:.88em;line-height:1.7;font-family:ui-monospace,monospace;">
      <strong>Pass A</strong> GBuffer<br>
      <span style="opacity:.5">│</span> render<br>
      <span style="opacity:.5">│</span> <span style="color:#ef4444;font-weight:600">store → VRAM ✗</span><br>
      <span style="opacity:.5">└</span> done<br>
      <br>
      <strong>Pass B</strong> Lighting<br>
      <span style="opacity:.5">│</span> <span style="color:#ef4444;font-weight:600">load ← VRAM ✗</span><br>
      <span style="opacity:.5">│</span> render<br>
      <span style="opacity:.5">└</span> done
    </div>
    <div style="margin-top:.7em;padding-top:.6em;border-top:1px solid rgba(239,68,68,.12);font-size:.82em;opacity:.7">2 render passes, 1 unnecessary round-trip</div>
  </div>
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(34,197,94,.25);background:rgba(34,197,94,.03);padding:1em 1.1em;">
    <div style="font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:#22c55e;margin-bottom:.6em;">With merging</div>
    <div style="font-size:.88em;line-height:1.7;font-family:ui-monospace,monospace;">
      <strong>Pass A+B</strong> merged<br>
      <span style="opacity:.5">│</span> render A<br>
      <span style="opacity:.5">│</span> <span style="color:#22c55e;font-weight:600">B reads in-place ✓</span><br>
      <span style="opacity:.5">│</span> render B<br>
      <span style="opacity:.5">└</span> store once → VRAM
    </div>
    <div style="margin-top:.7em;padding-top:.6em;border-top:1px solid rgba(34,197,94,.15);font-size:.82em;color:#22c55e;font-weight:600">1 render pass — no intermediate memory traffic</div>
  </div>
</div>

<div class="fg-reveal" style="margin:1.2em 0;padding:.75em 1em;border-radius:8px;background:rgba(59,130,246,.04);border:1px solid rgba(59,130,246,.1);font-size:.88em;line-height:1.6;">
<strong>When can two passes merge?</strong> Three conditions, all required:<br>
<span style="display:inline-block;width:1.4em;text-align:center;font-weight:700;color:#3b82f6;">①</span> Same render target dimensions<br>
<span style="display:inline-block;width:1.4em;text-align:center;font-weight:700;color:#3b82f6;">②</span> Second pass reads the first's output at the <strong>current pixel only</strong> (no arbitrary UV sampling)<br>
<span style="display:inline-block;width:1.4em;text-align:center;font-weight:700;color:#3b82f6;">③</span> No external dependencies forcing a render pass break
</div>

Fewer render pass boundaries means fewer state changes, less barrier overhead, and the driver gets a larger scope to schedule work internally. D3D12 Render Pass Tier 2 hardware can eliminate intermediate stores for merged passes entirely — the GPU keeps data on-chip between subpasses instead of round-tripping through VRAM. Console GPUs benefit similarly, where the driver can batch state setup across fused passes.

### ⚡ Async Compute

Pass merging and barriers optimize work on a single GPU queue. But modern GPUs expose at least two: a **graphics queue** and a **compute queue**. If two passes have **no dependency path between them** in the DAG, the compiler can schedule them on different queues simultaneously.

#### 🔍 Finding parallelism

The compiler needs to answer one question for every pair of passes: **can these run at the same time?** Two passes can overlap only if neither depends on the other — directly or indirectly. A pass that writes the GBuffer can't overlap with lighting (which reads it), but it *can* overlap with SSAO if they share no resources.

The algorithm is called **reachability analysis** — for each pass, the compiler figures out every other pass it can eventually reach by following edges forward through the DAG. If pass A can reach pass B (or B can reach A), they're dependent. If neither can reach the other, they're **independent** — safe to run on separate queues.

#### 🚧 Minimizing fences

Cross-queue work needs **GPU fences** — one queue signals, the other waits. Each fence costs ~5–15 µs of dead GPU time. Move SSAO, volumetrics, and particle sim to compute and you create six fences — up to **90 µs of idle** that can erase the overlap gain. The compiler applies **transitive reduction** to collapse those down:

<div class="fg-grid-stagger" style="display:grid;grid-template-columns:1fr 1fr;gap:1em;margin:1.2em 0;">
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(239,68,68,.25);overflow:hidden;">
    <div style="padding:.55em .9em;background:rgba(239,68,68,.06);border-bottom:1px solid rgba(239,68,68,.12);font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:#ef4444;">Naive — 4 fences</div>
    <div style="padding:.8em .9em;font-family:ui-monospace,monospace;font-size:.82em;line-height:1.8;">
      <span style="opacity:.5;">Graphics:</span> [A] ──<span style="color:#ef4444;font-weight:600;">fence</span>──→ [C]<br>
      <span style="opacity:.3;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>└──<span style="color:#ef4444;font-weight:600;">fence</span>──→ [D]<br>
      <br>
      <span style="opacity:.5;">Compute:</span>&nbsp; [B] ──<span style="color:#ef4444;font-weight:600;">fence</span>──→ [C]<br>
      <span style="opacity:.3;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>└──<span style="color:#ef4444;font-weight:600;">fence</span>──→ [D]
    </div>
    <div style="padding:.5em .9em;border-top:1px solid rgba(239,68,68,.1);font-size:.78em;opacity:.7;">Every cross-queue edge gets its own fence</div>
  </div>
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(34,197,94,.25);overflow:hidden;">
    <div style="padding:.55em .9em;background:rgba(34,197,94,.06);border-bottom:1px solid rgba(34,197,94,.12);font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:#22c55e;">Reduced — 1 fence</div>
    <div style="padding:.8em .9em;font-family:ui-monospace,monospace;font-size:.82em;line-height:1.8;">
      <span style="opacity:.5;">Graphics:</span> [A] ─────────→ [C] → [D]<br>
      <span style="opacity:.3;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>↑<br>
      <span style="opacity:.5;">Compute:</span>&nbsp; [B] ──<span style="color:#22c55e;font-weight:600;">fence</span>──┘<br>
      <br>
      B's fence covers both C and D<br>
      <span style="opacity:.6;">(D is after C on graphics queue)</span>
    </div>
    <div style="padding:.5em .9em;border-top:1px solid rgba(34,197,94,.1);font-size:.78em;color:#22c55e;font-weight:600;">Redundant fences removed transitively</div>
  </div>
</div>

#### ⚖️ What makes overlap good or bad

Solving fences is the easy part — the compiler handles that. The harder question is whether overlapping two specific passes actually helps:

<div class="fg-grid-stagger" style="display:grid;grid-template-columns:1fr 1fr;gap:1em;margin:1.2em 0;">
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(34,197,94,.25);overflow:hidden;">
    <div style="padding:.6em .9em;background:rgba(34,197,94,.05);border-bottom:1px solid rgba(34,197,94,.12);font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:#22c55e;">✓ Complementary</div>
    <div style="padding:.8em .9em;font-size:.88em;line-height:1.6;">
      Graphics is <strong>ROP/rasterizer-bound</strong> (shadow rasterization, geometry-dense passes) while compute runs <strong>ALU-heavy</strong> shaders (SSAO, volumetrics). Different hardware units stay busy — real parallelism, measurable frame time reduction.
    </div>
  </div>
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(239,68,68,.25);overflow:hidden;">
    <div style="padding:.6em .9em;background:rgba(239,68,68,.05);border-bottom:1px solid rgba(239,68,68,.12);font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:#ef4444;">✗ Competing</div>
    <div style="padding:.8em .9em;font-size:.88em;line-height:1.6;">
      Both passes are <strong>bandwidth-bound</strong> or both <strong>ALU-heavy</strong> — they thrash each other's L2 cache and fight for CU time. The frame gets <em>slower</em> than running them sequentially. Common trap: overlapping two fullscreen post-effects.
    </div>
  </div>
</div>

<div class="fg-reveal" style="margin:.6em 0 1.2em;padding:.65em 1em;border-radius:8px;border:1px solid rgba(99,102,241,.12);background:rgba(99,102,241,.03);font-size:.85em;line-height:1.6;opacity:.8;">
NVIDIA uses dedicated async engines. AMD exposes more independent CUs for overlap. But on both: <strong>always profile per-GPU</strong> — the overlap that wins on one architecture can regress on another.
</div>

Try it yourself — move compute-eligible passes between queues and see how fence count and frame time change:

{{< interactive-async >}}

#### 🧭 Should this pass go async?

<div style="margin:1.2em 0;max-width:420px;font-size:.88em;">
  <div style="display:flex;align-items:center;gap:.6em;padding:.55em .8em;border-radius:8px 8px 0 0;background:rgba(99,102,241,.06);border:1.5px solid rgba(99,102,241,.15);border-bottom:none;">
    <span style="font-weight:700;">Compute-only?</span>
    <span style="margin-left:auto;font-size:.82em;color:#ef4444;">no → needs rasterization</span>
  </div>
  <div style="text-align:center;color:#22c55e;font-weight:700;font-size:.75em;line-height:1.8;">yes ↓</div>
  <div style="display:flex;align-items:center;gap:.6em;padding:.55em .8em;background:rgba(99,102,241,.06);border:1.5px solid rgba(99,102,241,.15);border-bottom:none;">
    <span style="font-weight:700;">Independent of graphics?</span>
    <span style="margin-left:auto;font-size:.82em;color:#ef4444;">no → shared resource</span>
  </div>
  <div style="text-align:center;color:#22c55e;font-weight:700;font-size:.75em;line-height:1.8;">yes ↓</div>
  <div style="display:flex;align-items:center;gap:.6em;padding:.55em .8em;background:rgba(99,102,241,.06);border:1.5px solid rgba(99,102,241,.15);border-bottom:none;">
    <span style="font-weight:700;">Complementary overlap?</span>
    <span style="margin-left:auto;font-size:.82em;color:#ef4444;">no → profile first</span>
  </div>
  <div style="text-align:center;color:#22c55e;font-weight:700;font-size:.75em;line-height:1.8;">yes ↓</div>
  <div style="display:flex;align-items:center;gap:.6em;padding:.55em .8em;background:rgba(99,102,241,.06);border:1.5px solid rgba(99,102,241,.15);">
    <span style="font-weight:700;">Enough work between fences?</span>
    <span style="margin-left:auto;font-size:.82em;color:#ef4444;">no → sync eats the gain</span>
  </div>
  <div style="text-align:center;color:#22c55e;font-weight:700;font-size:.75em;line-height:1.8;">yes ↓</div>
  <div style="padding:.55em .8em;border-radius:0 0 8px 8px;background:rgba(34,197,94,.08);border:1.5px solid rgba(34,197,94,.25);text-align:center;font-weight:800;color:#22c55e;">ASYNC COMPUTE ✓</div>
</div>
<div style="font-size:.82em;opacity:.6;margin-top:-.4em;">Good candidates: SSAO alongside ROP-bound geometry, volumetrics during shadow rasterization, particle sim during UI.</div>

### ✂️ Split Barriers

Async compute hides latency by overlapping work across *queues*. Split barriers achieve the same effect on a *single queue* — by spreading one resource transition across multiple passes instead of stalling on it.

A **regular barrier** does a cache flush, state change, and cache invalidate in one blocking command — the GPU finishes the source pass, stalls while the transition completes, then starts the next pass. Every microsecond of that stall is wasted.

A **split barrier** breaks the transition into two halves and spreads them apart:

<div style="margin:1.4em 0;font-size:.88em;">
  <div style="display:flex;align-items:stretch;gap:0;border-radius:10px;overflow:hidden;border:1.5px solid rgba(99,102,241,.15);">
    <div style="background:rgba(59,130,246,.08);padding:.8em 1em;border-right:3px solid #3b82f6;min-width:130px;text-align:center;">
      <div style="font-weight:800;font-size:.95em;">Source pass</div>
      <div style="font-size:.78em;opacity:.6;margin-top:.2em;">writes texture</div>
    </div>
    <div style="background:rgba(59,130,246,.15);padding:.5em .8em;display:flex;align-items:center;min-width:50px;border-right:1px dashed rgba(99,102,241,.3);">
      <div style="text-align:center;width:100%;">
        <div style="font-size:.7em;font-weight:700;color:#3b82f6;text-transform:uppercase;letter-spacing:.04em;">BEGIN</div>
        <div style="font-size:.68em;opacity:.5;">flush caches</div>
      </div>
    </div>
    <div style="flex:1;background:repeating-linear-gradient(90deg,rgba(99,102,241,.04) 0,rgba(99,102,241,.04) 50%,rgba(99,102,241,.08) 50%,rgba(99,102,241,.08) 100%);background-size:50% 100%;display:flex;align-items:stretch;min-width:200px;">
      <div style="flex:1;padding:.6em .7em;text-align:center;border-right:1px dashed rgba(99,102,241,.15);">
        <div style="font-weight:700;font-size:.85em;">Pass C</div>
        <div style="font-size:.72em;opacity:.5;">unrelated work</div>
      </div>
      <div style="flex:1;padding:.6em .7em;text-align:center;">
        <div style="font-weight:700;font-size:.85em;">Pass D</div>
        <div style="font-size:.72em;opacity:.5;">unrelated work</div>
      </div>
    </div>
    <div style="background:rgba(34,197,94,.15);padding:.5em .8em;display:flex;align-items:center;min-width:50px;border-left:1px dashed rgba(99,102,241,.3);">
      <div style="text-align:center;width:100%;">
        <div style="font-size:.7em;font-weight:700;color:#22c55e;text-transform:uppercase;letter-spacing:.04em;">END</div>
        <div style="font-size:.68em;opacity:.5;">invalidate</div>
      </div>
    </div>
    <div style="background:rgba(34,197,94,.08);padding:.8em 1em;border-left:3px solid #22c55e;min-width:130px;text-align:center;">
      <div style="font-weight:800;font-size:.95em;">Dest pass</div>
      <div style="font-size:.78em;opacity:.6;margin-top:.2em;">reads texture</div>
    </div>
  </div>
  <div style="display:flex;margin-top:.25em;">
    <div style="min-width:130px;"></div>
    <div style="min-width:50px;"></div>
    <div style="flex:1;min-width:200px;text-align:center;font-size:.78em;opacity:.6;">
      ↑ cache flush runs in background while these execute ↑
    </div>
    <div style="min-width:50px;"></div>
    <div style="min-width:130px;"></div>
  </div>
</div>

The passes between begin and end are the **overlap gap** — they execute while the cache flush happens in the background. The compiler places these automatically: begin immediately after the source pass, end immediately before the destination.

#### 📏 How much gap is enough?

<div class="fg-grid-stagger" style="display:grid;grid-template-columns:repeat(4,1fr);gap:.6em;margin:1.2em 0;">
  <div class="fg-hoverable" style="border-radius:8px;border:1.5px solid rgba(239,68,68,.2);background:rgba(239,68,68,.03);padding:.7em .8em;text-align:center;">
    <div style="font-weight:800;font-size:1.3em;color:#ef4444;">0</div>
    <div style="font-size:.8em;font-weight:600;margin:.25em 0;">passes</div>
    <div style="font-size:.78em;opacity:.7;">No gap — degenerates into a regular barrier with extra API cost</div>
  </div>
  <div class="fg-hoverable" style="border-radius:8px;border:1.5px solid rgba(234,179,8,.2);background:rgba(234,179,8,.03);padding:.7em .8em;text-align:center;">
    <div style="font-weight:800;font-size:1.3em;color:#eab308;">1</div>
    <div style="font-size:.8em;font-weight:600;margin:.25em 0;">pass</div>
    <div style="font-size:.78em;opacity:.7;">Marginal — might not cover the full flush latency</div>
  </div>
  <div class="fg-hoverable" style="border-radius:8px;border:1.5px solid rgba(34,197,94,.25);background:rgba(34,197,94,.03);padding:.7em .8em;text-align:center;">
    <div style="font-weight:800;font-size:1.3em;color:#22c55e;">2+</div>
    <div style="font-size:.8em;font-weight:600;margin:.25em 0;">passes</div>
    <div style="font-size:.78em;opacity:.7;">Cache flush fully hidden — measurable frame time reduction</div>
  </div>
  <div class="fg-hoverable" style="border-radius:8px;border:1.5px solid rgba(99,102,241,.2);background:rgba(99,102,241,.03);padding:.7em .8em;text-align:center;">
    <div style="font-weight:800;font-size:1.3em;color:#6366f1;">⚡</div>
    <div style="font-size:.8em;font-weight:600;margin:.25em 0;">cross-queue</div>
    <div style="font-size:.78em;opacity:.7;">Can't split across queues — use an async fence instead</div>
  </div>
</div>

Try it — drag the BEGIN marker left to widen the overlap gap and watch the stall disappear:

{{< interactive-split-barriers >}}

<div class="fg-reveal" style="margin:1.4em 0;padding:.85em 1.1em;border-radius:10px;background:linear-gradient(135deg,rgba(34,197,94,.06),rgba(59,130,246,.06));border:1px solid rgba(34,197,94,.15);font-size:.92em;line-height:1.65;">
That's all the theory. <a href="../frame-graph-build-it/" style="font-weight:600;">Part II</a> implements the core — barriers, culling, aliasing — in ~300 lines of C++. <a href="../frame-graph-production/" style="font-weight:600;">Part III</a> shows how production engines deploy all of these at scale.
</div>

---

## 🎛️ Putting It All Together

You've now seen every piece the compiler works with — topological sorting, pass culling, barrier insertion, async compute scheduling, memory aliasing, split barriers. In a simple 5-pass pipeline these feel manageable. In a production renderer? You're looking at **15–25 passes, 30+ resource edges, and dozens of implicit dependencies** — all inferred from `read()` and `write()` calls that no human can hold in their head at once.

<div class="fg-reveal" style="margin:1.2em 0;padding:.85em 1.1em;border-radius:10px;border:1.5px solid rgba(139,92,246,.2);background:linear-gradient(135deg,rgba(139,92,246,.05),transparent);font-size:.92em;line-height:1.65;">
<strong>This is the trade-off at the heart of every render graph.</strong> Dependencies become <em>implicit</em> — the graph infers ordering from data flow, which means you never declare "pass A must run before pass B." That's powerful: the compiler can reorder, cull, and parallelize freely. But it also means <strong>dependencies are hidden</strong>. Miss a <code>read()</code> call and the graph silently reorders two passes that shouldn't overlap. Add an assert and you'll catch the <em>symptom</em> — but not the missing edge that caused it.
</div>

The render graph isn't a safety net — it's a tool that requires discipline. You still need to build the graph in the right order, track what modifies what, and validate that every resource dependency is explicitly declared. **But here's the upside:** because every dependency flows through the graph, you can build tools to **visualize** the entire pipeline — something that's impossible when barriers and ordering are scattered across hand-written render code.

The explorer below is a production-scale graph. Toggle each compiler feature on and off to see exactly what it contributes. Click any pass to inspect its implicit dependencies — every edge was inferred, not hand-written.

{{< interactive-full-pipeline >}}

<div class="fg-reveal" style="margin:1.2em 0;padding:.85em 1.1em;border-radius:10px;border:1px solid rgba(139,92,246,.15);background:rgba(139,92,246,.04);font-size:.9em;line-height:1.65;">
<strong>Why this matters:</strong> With all features off, you're looking at what a renderer <em>without</em> a frame graph must manage by hand — every dependency, every barrier, every memory decision, across every pass. Turn them on one by one and watch the compiler do the work. That's the pitch: <strong>trade implicit dependencies for compiler-automated scheduling, synchronization, and memory management.</strong> The graph hides where dependencies are — but it also gives you the tooling to see <em>all of them at once</em>, which manual code never could.
</div>

---

<div style="margin:2em 0 0;padding:1em 1.2em;border-radius:10px;border:1px solid rgba(59,130,246,.2);background:rgba(59,130,246,.03);display:flex;justify-content:flex-end;">
  <a href="../frame-graph-build-it/" style="text-decoration:none;font-weight:700;font-size:.95em;">
    Next: Part II — Build It →
  </a>
</div>
