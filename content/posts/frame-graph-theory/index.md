---
title: "Frame Graph — Theory"
date: 2026-02-09
draft: true
description: "What a render graph is, what problems it solves, and why every major engine uses one."
tags: ["rendering", "frame-graph", "gpu", "architecture"]
categories: ["analysis"]
series: ["Rendering Architecture"]
showTableOfContents: false
---

{{< article-nav >}}

<div style="margin:0 0 1.5em;padding:.7em 1em;border-radius:8px;background:rgba(99,102,241,.04);border:1px solid rgba(99,102,241,.12);font-size:.88em;line-height:1.6;opacity:.85;">
📖 <strong>Part I of III.</strong>&ensp; <em>Theory</em> → <a href="/posts/frame-graph-build-it/">Build It</a> → <a href="/posts/frame-graph-production/">Production Engines</a>
</div>

*What a render graph is, what problems it solves, and why every major engine uses one.*

---

## 🎯 Why You Want One

<div style="margin:1.2em 0 1.5em;padding:1.3em 1.5em;border-radius:12px;border:1.5px solid rgba(99,102,241,.18);background:linear-gradient(135deg,rgba(99,102,241,.04),rgba(34,197,94,.03));">
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

Frostbite introduced it at GDC 2017. UE5 ships it as **RDG**. Unity has its own in SRP. Every major renderer uses one — this series shows you why, walks you through building your own in C++, and maps every piece to what ships in production engines.

<div style="margin:1.5em 0;border-radius:12px;overflow:hidden;border:1.5px solid rgba(99,102,241,.25);background:linear-gradient(135deg,rgba(99,102,241,.04),transparent);">
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

<div style="position:relative;margin:1.4em 0;padding-left:2.2em;border-left:3px solid var(--color-neutral-300,#d4d4d4);">

  <div style="margin-bottom:1.6em;">
    <div style="position:absolute;left:-0.8em;width:1.4em;height:1.4em;border-radius:50%;background:#22c55e;display:flex;align-items:center;justify-content:center;font-size:.7em;color:#fff;font-weight:700;">1</div>
    <div style="font-weight:800;font-size:1.05em;color:#22c55e;margin-bottom:.3em;">Month 1 — 3 passes, everything's fine</div>
    <div style="font-size:.92em;line-height:1.6;">
      Depth prepass → GBuffer → lighting. Two barriers, hand-placed. Two textures, both allocated at init. Code is clean, readable, correct.
    </div>
    <div style="margin-top:.4em;padding:.4em .8em;border-radius:6px;background:rgba(34,197,94,.06);font-size:.88em;font-style:italic;border-left:3px solid #22c55e;">
      At this scale, manual management works. You know every resource by name.
    </div>
  </div>

  <div style="margin-bottom:1.6em;">
    <div style="position:absolute;left:-0.8em;width:1.4em;height:1.4em;border-radius:50%;background:#f59e0b;display:flex;align-items:center;justify-content:center;font-size:.7em;color:#fff;font-weight:700;">6</div>
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
    <div style="position:absolute;left:-0.8em;width:1.4em;height:1.4em;border-radius:50%;background:#ef4444;display:flex;align-items:center;justify-content:center;font-size:.65em;color:#fff;font-weight:700;">18</div>
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
<div style="margin:.8em auto 1.2em;max-width:560px;">
  <div style="display:flex;align-items:stretch;gap:0;border-radius:10px;overflow:hidden;border:1.5px solid rgba(99,102,241,.2);">
    <div style="flex:1;padding:.7em .6em;text-align:center;background:rgba(59,130,246,.06);border-right:1px solid rgba(99,102,241,.12);">
      <div style="font-weight:800;font-size:.88em;letter-spacing:.04em;color:#3b82f6;">①&ensp;DECLARE</div>
      <div style="font-size:.75em;opacity:.6;margin-top:.2em;">passes &amp; dependencies</div>
    </div>
    <div style="flex:1;padding:.7em .6em;text-align:center;background:rgba(139,92,246,.06);border-right:1px solid rgba(99,102,241,.12);">
      <div style="font-weight:800;font-size:.88em;letter-spacing:.04em;color:#8b5cf6;">②&ensp;COMPILE</div>
      <div style="font-size:.75em;opacity:.6;margin-top:.2em;">order · aliases · barriers</div>
    </div>
    <div style="flex:1;padding:.7em .6em;text-align:center;background:rgba(34,197,94,.06);">
      <div style="font-weight:800;font-size:.88em;letter-spacing:.04em;color:#22c55e;">③&ensp;EXECUTE</div>
      <div style="font-size:.75em;opacity:.6;margin-top:.2em;">record GPU commands</div>
    </div>
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

<div style="margin:1.2em 0;padding:1.1em 1.3em;border-radius:10px;border:1.5px dashed rgba(99,102,241,.3);background:rgba(99,102,241,.04);display:flex;align-items:center;gap:1.2em;flex-wrap:wrap;">
  <div style="flex:1;min-width:180px;">
    <div style="font-size:1.15em;font-weight:800;margin:.1em 0;">Handle #3</div>
    <div style="font-size:.82em;opacity:.6;">1920×1080 · RGBA8 · render target</div>
  </div>
  <div style="flex-shrink:0;padding:.4em .8em;border-radius:6px;background:rgba(245,158,11,.1);color:#f59e0b;font-weight:700;font-size:.8em;">description only — no GPU memory yet</div>
</div>
<div style="text-align:center;font-size:.82em;opacity:.6;margin-top:-.2em;">
  Resources stay virtual at this stage — just a description and a handle. Memory comes later.
</div>

### Transient vs. imported

When you declare a resource, the graph needs to know one thing: **does it live inside this frame, or does it come from outside?**

<div style="display:grid;grid-template-columns:1fr 1fr;gap:1em;margin:1.2em 0;">
  <div style="border-radius:10px;border:1.5px solid rgba(59,130,246,.3);overflow:hidden;">
    <div style="padding:.6em .9em;font-weight:800;font-size:.95em;background:rgba(59,130,246,.08);border-bottom:1px solid rgba(59,130,246,.15);color:#3b82f6;">⚡ Transient</div>
    <div style="padding:.7em .9em;font-size:.88em;line-height:1.7;">
      <strong>Lifetime:</strong> single frame<br>
      <strong>Declared as:</strong> description (size, format)<br>
      <strong>GPU memory:</strong> allocated at compile time — <em>virtual until then</em><br>
      <strong>Aliasable:</strong> <span style="color:#22c55e;font-weight:700;">Yes</span> — non-overlapping lifetimes share physical memory<br>
      <strong>Examples:</strong> GBuffer MRTs, SSAO scratch, bloom scratch
    </div>
  </div>
  <div style="border-radius:10px;border:1.5px solid rgba(139,92,246,.3);overflow:hidden;">
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
    </div>
    <div style="text-align:center;font-size:.82em;opacity:.6;margin-top:.3em">Still CPU — producing data structures for the execute phase</div>
  </div>
</div>

<!-- Visual: the virtual handle from Declare is now resolved -->
<div style="margin:1.2em 0;display:flex;align-items:center;gap:1em;flex-wrap:wrap;">
  <div style="flex:1;min-width:180px;padding:1em 1.2em;border-radius:10px;border:1.5px dashed rgba(99,102,241,.3);background:rgba(99,102,241,.04);">
    <div style="font-size:1.1em;font-weight:800;">Handle #3</div>
    <div style="font-size:.8em;opacity:.5;">1920×1080 · RGBA8</div>
    <div style="margin-top:.4em;font-size:.75em;padding:.25em .6em;border-radius:6px;background:rgba(245,158,11,.1);color:#f59e0b;font-weight:700;display:inline-block;">virtual</div>
  </div>
  <div style="font-size:1.4em;opacity:.3;flex-shrink:0;">→</div>
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

The plan is ready — now the GPU gets involved. This phase walks the compiled pass order, applies barriers, and calls your execute lambdas.

<div class="diagram-box">
  <div class="db-title">▶️ EXECUTE — recording GPU commands</div>
  <div class="db-body">
    <div class="diagram-pipeline">
      <div class="dp-stage">
        <div class="dp-title">BACK RESOURCES</div>
        <ul><li>create or reuse physical memory</li><li>apply the alias map</li></ul>
      </div>
      <div class="dp-stage">
        <div class="dp-title">RUN PASSES</div>
        <ul><li>for each pass in compiled order:</li><li>insert barriers → call <code>execute()</code></li></ul>
      </div>
      <div class="dp-stage">
        <div class="dp-title">CLEANUP</div>
        <ul><li>release transients (or pool them)</li><li>reset the frame allocator</li></ul>
      </div>
    </div>
    <div style="text-align:center;font-size:.82em;opacity:.6;margin-top:.3em">The only phase that touches the GPU API</div>
  </div>
</div>

<div style="margin:1.2em 0;padding:1em 1.2em;border-radius:10px;border:1.5px solid rgba(34,197,94,.2);background:rgba(34,197,94,.04);font-size:.92em;line-height:1.6;">
  Each execute lambda sees a <strong>fully resolved environment</strong> — barriers already placed, memory already allocated, resources ready to bind. The lambda just records draw calls, dispatches, and copies. All the intelligence lives in the compile step.
</div>

---

## 🔄 Rebuild Strategies

How often should the graph recompile? Three approaches, each a valid tradeoff:

<div style="display:grid;grid-template-columns:repeat(3,1fr);gap:1em;margin:1.2em 0;">
  <div style="border-radius:10px;border:1.5px solid #22c55e;overflow:hidden;">
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
  <div style="border-radius:10px;border:1.5px solid #3b82f6;overflow:hidden;">
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
  <div style="border-radius:10px;border:1.5px solid var(--color-neutral-400,#9ca3af);overflow:hidden;">
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

<div style="margin:1.2em 0;display:grid;grid-template-columns:1fr 1fr;gap:0;border-radius:10px;overflow:hidden;border:2px solid rgba(99,102,241,.25);box-shadow:0 2px 8px rgba(0,0,0,.08);">
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

<div style="margin:1.2em 0;padding:.8em 1em;border-radius:8px;background:linear-gradient(135deg,rgba(34,197,94,.06),rgba(59,130,246,.06));border:1px solid rgba(34,197,94,.2);font-size:.92em;line-height:1.6;">
🏭 <strong>Not theoretical.</strong> Frostbite reported <strong>50% VRAM reduction</strong> from aliasing at GDC 2017. UE5's RDG ships the same optimization today — every <code>FRDGTexture</code> marked as transient goes through the same aliasing pipeline we build in <a href="/posts/frame-graph-build-it/">Part II</a>.
</div>

---

## 🔬 Advanced Features

The core graph — scheduling, barriers, aliasing — covers most of what a render graph does. Three more techniques build on the same DAG to handle remaining production requirements. [Part II](/posts/frame-graph-build-it/) implements the core; [Part III](/posts/frame-graph-production/) shows how engines deploy all of these at scale.

### 🔗 Pass Merging

Tile-based GPUs (Mali, Adreno, Apple Silicon) render into small on-chip tile memory. Between separate render passes, the GPU **flushes** tiles to main memory and **reloads** them for the next pass — expensive bandwidth you'd rather avoid.

<div class="diagram-tiles">
  <div class="dt-col">
    <div class="dt-col-title">Without merging (tile-based GPU)</div>
    <div class="dt-col-body">
      <strong>Pass A (GBuffer)</strong><br>
      ├ render to tile<br>
      ├ <span class="dt-cost-bad">flush tile → main memory ✗ slow</span><br>
      └ done<br><br>
      <strong>Pass B (Lighting)</strong><br>
      ├ <span class="dt-cost-bad">load from main memory ✗ slow</span><br>
      ├ render to tile<br>
      └ done
    </div>
  </div>
  <div class="dt-col" style="border-color:#22c55e">
    <div class="dt-col-title" style="color:#22c55e">With merging</div>
    <div class="dt-col-body">
      <strong>Pass A+B (merged subpass)</strong><br>
      ├ render A to tile<br>
      ├ <span class="dt-cost-good">B reads tile directly — free!</span><br>
      └ flush once → main memory<br><br>
      <strong>Saves:</strong> 1 flush + 1 load per merged pair<br>
      = <span class="dt-cost-good">massive bandwidth savings on mobile</span>
    </div>
  </div>
</div>

Two adjacent passes can merge when they share the **same render target dimensions**, the second only reads the first's output at the **current pixel** (not arbitrary UVs), and there are **no external dependencies** forcing a render pass break. The graph compiler walks the sorted pass list, checks these three conditions, and groups compatible passes into one API render pass with multiple subpasses.

<div style="overflow-x:auto;margin:1em 0">
<table style="width:100%;border-collapse:collapse;font-size:.88em;border-radius:10px;overflow:hidden">
  <thead>
    <tr style="background:linear-gradient(135deg,rgba(59,130,246,.1),rgba(139,92,246,.08))">
      <th style="padding:.65em 1em;text-align:left;border-bottom:2px solid rgba(59,130,246,.15)">API</th>
      <th style="padding:.65em 1em;text-align:left;border-bottom:2px solid rgba(59,130,246,.15)">Merged group becomes</th>
      <th style="padding:.65em 1em;text-align:left;border-bottom:2px solid rgba(59,130,246,.15)">Intermediate data</th>
    </tr>
  </thead>
  <tbody>
    <tr><td style="padding:.5em 1em;font-weight:600;color:#ef4444">Vulkan</td><td style="padding:.5em 1em">Single <code>VkRenderPass</code> + N <code>VkSubpassDescription</code></td><td style="padding:.5em 1em">Subpass inputs (tile-local)</td></tr>
    <tr style="background:rgba(127,127,127,.04)"><td style="padding:.5em 1em;font-weight:600;color:#6b7280">Metal</td><td style="padding:.5em 1em">One <code>MTLRenderPassDescriptor</code>, <code>storeAction = .dontCare</code></td><td style="padding:.5em 1em"><code>loadAction = .load</code></td></tr>
    <tr><td style="padding:.5em 1em;font-weight:600;color:#3b82f6">D3D12</td><td style="padding:.5em 1em"><code>BeginRenderPass</code>/<code>EndRenderPass</code> (Tier 1/2)</td><td style="padding:.5em 1em">No direct subpass — via render pass tiers</td></tr>
  </tbody>
</table>
</div>

Desktop GPUs (NVIDIA, AMD) don't use tile-based rendering, so merging has minimal benefit there. This optimization matters most on mobile and Switch.

### 🛡️ Aliasing Pitfalls

Memory aliasing is powerful but has sharp edges you need to know about before implementing it:

<div class="diagram-warn">
  <div class="dw-title">⚠ Aliasing pitfalls</div>
  <div class="dw-row"><div class="dw-label">Format compat</div><div>depth/stencil metadata may conflict with color targets on same VkMemory → skip aliasing for depth formats</div></div>
  <div class="dw-row"><div class="dw-label">Initialization</div><div>reused memory = garbage contents → first use <strong>MUST</strong> be a full clear or fullscreen write</div></div>
  <div class="dw-row"><div class="dw-label">Imported res</div><div>survive across frames — <strong>never alias</strong>. Only transient resources qualify.</div></div>
</div>

Production engines also refine the aliasing implementation: **placed resources** (binding at offsets within `ID3D12Heap` or `VkDeviceMemory` heaps), **power-of-two bucketing** (reducing fragmentation), and **cross-frame pooling** (amortizing allocation cost to near zero). [Part III](/posts/frame-graph-production/) covers how each engine handles these.

### ⚡ Async Compute

Modern GPUs expose multiple hardware queues — at minimum a graphics queue and a compute queue. If two passes have **no dependency path between them** in the DAG, they can execute concurrently on different queues.

The compiler discovers these opportunities through **reachability analysis**. Walk the DAG in reverse topological order; each pass's bitset = union of its successors' bitsets + the successors themselves. Two passes are independent iff neither can reach the other — one bitwise AND per query.

<div class="diagram-bitset">
<table>
  <tr><th>Pass</th><th>Depth</th><th>GBuf</th><th>SSAO</th><th>Light</th><th>Tone</th><th>Reaches</th></tr>
  <tr><td><strong>Depth</strong></td><td class="bit-0">0</td><td class="bit-1">1</td><td class="bit-1">1</td><td class="bit-1">1</td><td class="bit-1">1</td><td>everything</td></tr>
  <tr><td><strong>GBuf</strong></td><td class="bit-0">0</td><td class="bit-0">0</td><td class="bit-1">1</td><td class="bit-1">1</td><td class="bit-1">1</td><td>SSAO, Light, Tone</td></tr>
  <tr><td><strong>SSAO</strong></td><td class="bit-0">0</td><td class="bit-0">0</td><td class="bit-0">0</td><td class="bit-1">1</td><td class="bit-1">1</td><td>Light, Tone</td></tr>
  <tr><td><strong>Light</strong></td><td class="bit-0">0</td><td class="bit-0">0</td><td class="bit-0">0</td><td class="bit-0">0</td><td class="bit-1">1</td><td>Tonemap</td></tr>
  <tr><td><strong>Tone</strong></td><td class="bit-0">0</td><td class="bit-0">0</td><td class="bit-0">0</td><td class="bit-0">0</td><td class="bit-0">0</td><td>—</td></tr>
</table>
</div>

<div class="diagram-card dc-success" style="margin:.5em 0">
  <strong>Can Shadows overlap SSAO?</strong> Neither can reach the other → <strong>independent → different queues</strong>
</div>

When a dependency edge crosses queue boundaries, a GPU fence (signal + wait) synchronizes them. **Transitive reduction** minimizes fence count: if a later fence already covers an earlier one's guarantee transitively, the earlier fence is redundant. Walk the DAG edges — if source is on queue A and destination on queue B, insert a fence. Then remove any fence whose guarantee is already covered by another fence later on the same queue.

<div class="diagram-tiles">
  <div class="dt-col">
    <div class="dt-col-title"><span class="dt-cost-bad">Without transitive reduction</span></div>
    <div class="dt-col-body" style="font-family:ui-monospace,monospace;font-size:.9em">
      Graphics: [A] ──fence──→ [C]<br>
      &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;└──fence──→ [D]<br><br>
      Compute: &nbsp;[B] ──fence──→ [C]<br>
      &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;└──fence──→ [D]<br><br>
      <span class="dt-cost-bad">4 fences</span>
    </div>
  </div>
  <div class="dt-col" style="border-color:#22c55e">
    <div class="dt-col-title"><span class="dt-cost-good">With transitive reduction</span></div>
    <div class="dt-col-body" style="font-family:ui-monospace,monospace;font-size:.9em">
      Graphics: [A] ──────────→ [C]<br>
      &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;↑<br>
      Compute: &nbsp;[B] ──fence──┘<br><br>
      B's fence covers both C and D<br>
      (D is after C on graphics queue)<br><br>
      <span class="dt-cost-good">1 fence</span>
    </div>
  </div>
</div>

Try it yourself — drag compute-eligible passes between queues and see fence costs update in real time:

{{< interactive-async >}}

**Should a pass go async?** Compute-only, lasting ≥ 0.5 ms, and independent from the graphics tail. Below that threshold, fence overhead (~5–15 µs) eats the savings.

<div class="diagram-tree">
  <div class="dt-node"><strong>Should this pass go async?</strong></div>
  <div class="dt-branch">
    <strong>Is it compute-only?</strong>
    <span class="dt-no"> — no →</span> <span class="dt-result dt-fail">can't</span> <span style="opacity:.6">(needs rasterization)</span>
    <div class="dt-branch">
      <span class="dt-yes">yes ↓</span><br>
      <strong>Duration > 0.5ms?</strong>
      <span class="dt-no"> — no →</span> <span class="dt-result dt-fail">don't bother</span> <span style="opacity:.6">(fence overhead ≈ 5–15µs eats the savings)</span>
      <div class="dt-branch">
        <span class="dt-yes">yes ↓</span><br>
        <strong>Independent from graphics tail?</strong>
        <span class="dt-no"> — no →</span> <span class="dt-result dt-fail">can't</span> <span style="opacity:.6">(DAG dependency)</span>
        <div class="dt-branch">
          <span class="dt-yes">yes ↓</span><br>
          <span class="dt-result dt-pass">ASYNC COMPUTE ✓</span><br>
          <span style="font-size:.85em;opacity:.7">Good candidates: SSAO, volumetrics, particle sim, light clustering</span>
        </div>
      </div>
    </div>
  </div>
</div>

### ✂️ Split Barriers

A regular barrier stalls the pipeline — the GPU finishes all prior work, transitions the resource, then resumes. A **split barrier** separates this into two halves:

<div class="diagram-steps">
  <div class="ds-step">
    <div class="ds-num" style="background:#3b82f6">3</div>
    <div><strong>Source pass finishes</strong> ← <span style="color:#3b82f6;font-weight:600">begin barrier</span> (flush caches)</div>
  </div>
  <div class="ds-step">
    <div class="ds-num" style="background:#6b7280">4</div>
    <div>Unrelated pass &ensp; <span style="opacity:.5">↕ GPU freely executes these</span></div>
  </div>
  <div class="ds-step">
    <div class="ds-num" style="background:#6b7280">5</div>
    <div>Unrelated pass</div>
  </div>
  <div class="ds-step">
    <div class="ds-num" style="background:#22c55e">6</div>
    <div><strong>Destination pass starts</strong> ← <span style="color:#22c55e;font-weight:600">end barrier</span> (invalidate caches)</div>
  </div>
</div>

The gap between begin and end is free overlap — unrelated passes execute without stalling on this transition. Placement is straightforward: begin goes right after the source pass finishes, end goes right before the destination pass starts.

Drag the BEGIN marker in the interactive tool below to see how the overlap gap changes:

{{< interactive-split-barriers >}}

Worth it when the gap spans **2+ passes**. If begin and end are adjacent, a split barrier degenerates into a regular barrier with extra API overhead. Vulkan uses `vkCmdSetEvent2` / `vkCmdWaitEvents2`; D3D12 uses `BARRIER_FLAG_BEGIN_ONLY` / `BARRIER_FLAG_END_ONLY`.

<div class="diagram-ftable">
<table>
  <tr><th>Gap size</th><th>Action</th><th>Why</th></tr>
  <tr><td><strong>0 passes</strong></td><td>regular barrier</td><td>begin/end adjacent → no benefit</td></tr>
  <tr><td><strong>1 pass</strong></td><td>maybe</td><td>marginal overlap</td></tr>
  <tr><td><strong>2+ passes</strong></td><td>split</td><td>measurable GPU overlap</td></tr>
  <tr><td><strong>cross-queue</strong></td><td>fence instead</td><td>can't split across queues</td></tr>
</table>
</div>

<div style="margin:1.2em 0;padding:.8em 1em;border-radius:8px;background:linear-gradient(135deg,rgba(34,197,94,.06),rgba(59,130,246,.06));border:1px solid rgba(34,197,94,.2);font-size:.92em;line-height:1.6;">
<span style="opacity:.7;font-size:.9em;">That's all the theory. <a href="/posts/frame-graph-build-it/">Part II</a> implements the core (barriers, culling, aliasing) in ~300 lines of C++. <a href="/posts/frame-graph-production/">Part III</a> shows how production engines deploy all of these at scale.</span>
</div>

---

<div style="margin:2em 0 0;padding:1em 1.2em;border-radius:10px;border:1px solid rgba(59,130,246,.2);background:rgba(59,130,246,.03);display:flex;justify-content:flex-end;">
  <a href="/posts/frame-graph-build-it/" style="text-decoration:none;font-weight:700;font-size:.95em;">
    Next: Part II — Build It →
  </a>
</div>
