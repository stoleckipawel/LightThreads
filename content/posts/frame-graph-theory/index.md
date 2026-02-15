---
title: "Frame Graph — Theory"
date: 2026-02-09
draft: false
description: "The theory behind frame graphs — how a DAG of passes and resources gives the compiler enough information to automate scheduling, barriers, and memory aliasing."
tags: ["rendering", "frame-graph", "gpu", "architecture"]
categories: ["analysis"]
series: ["Rendering Architecture"]
showTableOfContents: false
---

{{< article-nav >}}

<div style="margin:0 0 1.5em;padding:.7em 1em;border-radius:8px;background:rgba(var(--ds-indigo-rgb),.04);border:1px solid rgba(var(--ds-indigo-rgb),.12);font-size:.88em;line-height:1.6;opacity:.85;">
📖 <strong>Part I of III.</strong>&ensp; <em>Theory</em> → <a href="../frame-graph-build-it/">Build It</a> → <a href="../frame-graph-production/">Production Engines</a>
</div>

---

## 🎯 Why You Want One

<div class="fg-reveal" style="margin:1.2em 0 1.5em;padding:1.3em 1.5em;border-radius:12px;border:1.5px solid rgba(var(--ds-indigo-rgb),.18);background:linear-gradient(135deg,rgba(var(--ds-indigo-rgb),.04),rgba(var(--ds-success-rgb),.03));">
  <div style="display:grid;grid-template-columns:1fr auto 1fr;gap:.3em .8em;align-items:center;font-size:1em;line-height:1.6;">
    <span style="text-decoration:line-through;opacity:.4;text-align:right;">Passes run in whatever order you wrote them.</span>
    <span style="opacity:.45;display:flex;align-items:center;justify-content:center;"><svg viewBox="0 0 32 20" width="24" height="15" fill="none"><line x1="3" y1="10" x2="20" y2="10" stroke="currentColor" stroke-width="2" stroke-linecap="round" opacity=".15"/><line x1="3" y1="10" x2="20" y2="10" class="flow flow-sm flow-current" style="animation-duration:1.4s"/><polyline points="17,4 28,10 17,16" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none" opacity=".35"/></svg></span>
    <strong>Sorted by dependencies.</strong>
    <span style="text-decoration:line-through;opacity:.4;text-align:right;">Every GPU sync point placed by hand.</span>
    <span style="opacity:.45;display:flex;align-items:center;justify-content:center;"><svg viewBox="0 0 32 20" width="24" height="15" fill="none"><line x1="3" y1="10" x2="20" y2="10" stroke="currentColor" stroke-width="2" stroke-linecap="round" opacity=".15"/><line x1="3" y1="10" x2="20" y2="10" class="flow flow-sm flow-current"/><polyline points="17,4 28,10 17,16" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none" opacity=".35"/></svg></span>
    <strong>Barriers inserted for you.</strong>
    <span style="text-decoration:line-through;opacity:.4;text-align:right;">Each pass allocates its own memory — 900 MB gone.</span>
    <span style="opacity:.45;display:flex;align-items:center;justify-content:center;"><svg viewBox="0 0 32 20" width="24" height="15" fill="none"><line x1="3" y1="10" x2="20" y2="10" stroke="currentColor" stroke-width="2" stroke-linecap="round" opacity=".15"/><line x1="3" y1="10" x2="20" y2="10" class="flow flow-sm flow-current" style="animation-duration:1.6s"/><polyline points="17,4 28,10 17,16" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none" opacity=".35"/></svg></span>
    <strong style="color:var(--ds-success);">Resources shared safely — ~450 MB back.</strong>
  </div>
  <div style="margin-top:.8em;padding-top:.7em;border-top:1px solid rgba(var(--ds-indigo-rgb),.1);font-size:.88em;opacity:.7;line-height:1.5;text-align:center;">
    You describe <em>what</em> each pass needs — the graph figures out the <em>how</em>.
  </div>
</div>

Behind every smooth frame is a brutal scheduling problem — which passes can run in parallel, which buffers can reuse the same memory, and which barriers are actually necessary. Frame graphs solve it: declare what each pass reads and writes, and the graph handles the rest. This series breaks down the theory, builds a real implementation in C++, and shows how the same ideas scale to production engines like UE5's RDG.

<div class="fg-reveal" style="margin:1.5em 0;border-radius:12px;overflow:hidden;border:1.5px solid rgba(var(--ds-indigo-rgb),.25);background:linear-gradient(135deg,rgba(var(--ds-indigo-rgb),.04),transparent);">
  <div style="display:grid;grid-template-columns:repeat(3,1fr);gap:0;">
    <div style="padding:1em;text-align:center;border-right:1px solid rgba(var(--ds-indigo-rgb),.12);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.12);">
      <div style="font-size:1.6em;margin-bottom:.15em;">�</div>
      <div style="font-weight:800;font-size:.95em;">Learn Theory</div>
      <div style="font-size:.82em;opacity:.7;line-height:1.4;margin-top:.2em;">What a frame graph is, why every engine uses one, and how each piece works</div>
    </div>
    <div style="padding:1em;text-align:center;border-right:1px solid rgba(var(--ds-indigo-rgb),.12);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.12);">
      <div style="font-size:1.6em;margin-bottom:.15em;">🔨</div>
      <div style="font-weight:800;font-size:.95em;">Build MVP</div>
      <div style="font-size:.82em;opacity:.7;line-height:1.4;margin-top:.2em;">Working C++ frame graph, from scratch to prototype in ~300 lines</div>
    </div>
    <div style="padding:1em;text-align:center;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.12);">
      <div style="font-size:1.6em;margin-bottom:.15em;">🗺️</div>
      <div style="font-weight:800;font-size:.95em;">Map to UE5</div>
      <div style="font-size:.82em;opacity:.7;line-height:1.4;margin-top:.2em;">Every piece maps to RDG — read the source with confidence</div>
    </div>
  </div>
</div>

---

## 🔥 The Problem

<div class="fg-reveal" style="position:relative;margin:1.4em 0;padding-left:2.2em;border-left:3px solid var(--color-neutral-300,#d4d4d4);">

  <div style="margin-bottom:1.6em;">
    <div class="fg-dot-bounce" style="position:absolute;left:-0.8em;width:1.4em;height:1.4em;border-radius:50%;background:var(--ds-success);display:flex;align-items:center;justify-content:center;font-size:.7em;color:#fff;font-weight:700;">1</div>
    <div style="font-weight:800;font-size:1.05em;color:var(--ds-success);margin-bottom:.3em;">Month 1 — 3 passes, everything's fine</div>
    <div style="font-size:.92em;line-height:1.6;">
      Depth prepass → GBuffer → lighting. Two barriers, hand-placed. Two textures, both allocated at init. Code is clean, readable, correct.
    </div>
    <div style="margin-top:.4em;padding:.4em .8em;border-radius:6px;background:rgba(var(--ds-success-rgb),.06);font-size:.88em;font-style:italic;border-left:3px solid var(--ds-success);">
      At this scale, manual management works. You know every resource by name.
    </div>
  </div>

  <div style="margin-bottom:1.6em;">
    <div class="fg-dot-bounce" style="position:absolute;left:-0.8em;width:1.4em;height:1.4em;border-radius:50%;background:var(--ds-warn);display:flex;align-items:center;justify-content:center;font-size:.7em;color:#fff;font-weight:700;">6</div>
    <div style="font-weight:800;font-size:1.05em;color:var(--ds-warn);margin-bottom:.3em;">Month 6 — 12 passes, cracks appear</div>
    <div style="font-size:.92em;line-height:1.6;">
      Same renderer, now with SSAO, SSR, bloom, TAA, shadow cascades. Three things going wrong simultaneously:
    </div>
    <div style="margin-top:.5em;display:grid;gap:.4em;">
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(var(--ds-warn-rgb),.2);background:rgba(var(--ds-warn-rgb),.04);font-size:.88em;line-height:1.5;">
        <strong>Invisible dependencies</strong> — someone adds SSAO but doesn't realize GBuffer needs an updated barrier. Visual artifacts on fresh build.
      </div>
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(var(--ds-warn-rgb),.2);background:rgba(var(--ds-warn-rgb),.04);font-size:.88em;line-height:1.5;">
        <strong>Wasted memory</strong> — SSAO and bloom textures never overlap, but aliasing them means auditing every pass that might touch them. Nobody does it.
      </div>
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(var(--ds-warn-rgb),.2);background:rgba(var(--ds-warn-rgb),.04);font-size:.88em;line-height:1.5;">
        <strong>Silent reordering</strong> — two branches touch the render loop. Git merges cleanly, but the shadow pass ends up after lighting. Subtly wrong output ships unnoticed.
      </div>
    </div>
    <div style="margin-top:.5em;padding:.4em .8em;border-radius:6px;background:rgba(var(--ds-warn-rgb),.06);font-size:.88em;font-style:italic;border-left:3px solid var(--ds-warn);">
      No single change broke it. The accumulation broke it.
    </div>
  </div>

  <div>
    <div class="fg-dot-bounce" style="position:absolute;left:-0.8em;width:1.4em;height:1.4em;border-radius:50%;background:var(--ds-danger);display:flex;align-items:center;justify-content:center;font-size:.65em;color:#fff;font-weight:700;">18</div>
    <div style="font-weight:800;font-size:1.05em;color:var(--ds-danger);margin-bottom:.3em;">Month 18 — 25 passes, nobody touches it</div>
    <div style="font-size:.92em;line-height:1.6;margin-bottom:.5em;">The renderer works, but:</div>
    <div style="display:grid;gap:.4em;">
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(var(--ds-danger-rgb),.2);background:rgba(var(--ds-danger-rgb),.04);font-size:.88em;line-height:1.5;">
        <strong>900 MB VRAM.</strong> Profiling shows 400 MB is aliasable — but the lifetime analysis would take a week and break the next time anyone adds a pass.
      </div>
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(var(--ds-danger-rgb),.2);background:rgba(var(--ds-danger-rgb),.04);font-size:.88em;line-height:1.5;">
        <strong>47 barrier calls.</strong> Three are redundant, two are missing, one is in the wrong queue. Nobody knows which.
      </div>
      <div style="padding:.5em .8em;border-radius:6px;border:1px solid rgba(var(--ds-danger-rgb),.2);background:rgba(var(--ds-danger-rgb),.04);font-size:.88em;line-height:1.5;">
        <strong>2 days to add a new pass.</strong> 30 minutes for the shader, the rest to figure out where to slot it and what barriers it needs.
      </div>
    </div>
    <div style="margin-top:.5em;padding:.4em .8em;border-radius:6px;background:rgba(var(--ds-danger-rgb),.06);font-size:.88em;font-style:italic;border-left:3px solid var(--ds-danger);">
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
  <div style="color:var(--ds-success);font-weight:700">✓ manageable</div>
  <div style="color:var(--ds-warn);font-weight:700">⚠ fragile</div>
  <div style="color:var(--ds-danger);font-weight:700">✗ untouchable</div>
</div>

The pattern is always the same: manual resource management works at small scale and fails at compound scale. Not because engineers are sloppy — because *no human tracks 25 lifetimes and 47 transitions in their head every sprint*. You need a system that sees the whole frame at once.

---

## 💡 The Core Idea

A frame graph is a **directed acyclic graph (DAG)** — each node is a render pass, each edge is a resource one pass hands to the next. Here's what a typical deferred frame looks like:

<!-- DAG flow diagram — Frostbite-style -->
<div style="margin:1.6em 0 .5em;text-align:center;">
<svg viewBox="0 0 1050 210" width="100%" style="max-width:1050px;display:block;margin:0 auto;font-family:inherit;" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <filter id="gB" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur in="SourceGraphic" stdDeviation="6" result="blur"/><feColorMatrix in="blur" type="matrix" values="0 0 0 0 0.23  0 0 0 0 0.51  0 0 0 0 0.96  0 0 0 0.35 0"/><feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge></filter>
    <filter id="gO" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur in="SourceGraphic" stdDeviation="6" result="blur"/><feColorMatrix in="blur" type="matrix" values="0 0 0 0 0.96  0 0 0 0 0.62  0 0 0 0 0.04  0 0 0 0.35 0"/><feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge></filter>
    <filter id="gG" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur in="SourceGraphic" stdDeviation="6" result="blur"/><feColorMatrix in="blur" type="matrix" values="0 0 0 0 0.13  0 0 0 0 0.77  0 0 0 0 0.37  0 0 0 0.35 0"/><feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge></filter>
    <filter id="gR" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur in="SourceGraphic" stdDeviation="5" result="blur"/><feColorMatrix in="blur" type="matrix" values="0 0 0 0 0.94  0 0 0 0 0.27  0 0 0 0 0.27  0 0 0 0.35 0"/><feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge></filter>
    <linearGradient id="grB" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stop-color="#60a5fa"/><stop offset="100%" stop-color="#2563eb"/></linearGradient>
    <linearGradient id="grO" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stop-color="#fbbf24"/><stop offset="100%" stop-color="#d97706"/></linearGradient>
    <linearGradient id="grG" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stop-color="#4ade80"/><stop offset="100%" stop-color="#16a34a"/></linearGradient>
    <linearGradient id="grR" x1="0" y1="0" x2="0" y2="1"><stop offset="0%" stop-color="#f87171"/><stop offset="100%" stop-color="#dc2626"/></linearGradient>
    <marker id="ah" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M1,1 L9,5 L1,9" fill="none" stroke="rgba(255,255,255,.35)" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></marker>
  </defs>
  <!-- base edges -->
  <path d="M115,100 L155,100 L155,42 L195,42" fill="none" stroke="rgba(255,255,255,.12)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" marker-end="url(#ah)"/>
  <path d="M115,120 L155,120 L155,160 L200,160" fill="none" stroke="rgba(255,255,255,.12)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" marker-end="url(#ah)"/>
  <path d="M300,160 L380,160" fill="none" stroke="rgba(255,255,255,.12)" stroke-width="2" stroke-linecap="round" marker-end="url(#ah)"/>
  <path d="M320,42 L520,42 L520,96 L548,96" fill="none" stroke="rgba(255,255,255,.12)" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" marker-end="url(#ah)"/>
  <path d="M462,160 L548,118" fill="none" stroke="rgba(255,255,255,.12)" stroke-width="2" stroke-linecap="round" marker-end="url(#ah)"/>
  <path d="M300,176 C370,205 480,205 548,125" fill="none" stroke="rgba(255,255,255,.12)" stroke-width="2" stroke-linecap="round" marker-end="url(#ah)"/>
  <path d="M650,106 L720,106" fill="none" stroke="rgba(255,255,255,.12)" stroke-width="2" stroke-linecap="round" marker-end="url(#ah)"/>
  <path d="M830,106 L910,106" fill="none" stroke="rgba(255,255,255,.12)" stroke-width="2" stroke-linecap="round" marker-end="url(#ah)"/>
  <!-- flow particles (CSS animated — classes from custom.css flow system) -->
  <path class="flow flow-lg flow-d1" d="M115,100 L155,100 L155,42 L195,42"/>
  <path class="flow flow-lg flow-d2" d="M115,120 L155,120 L155,160 L200,160"/>
  <path class="flow flow-lg flow-d3" d="M300,160 L380,160"/>
  <path class="flow flow-lg flow-d4" d="M320,42 L520,42 L520,96 L548,96"/>
  <path class="flow flow-lg flow-d5" d="M462,160 L548,118"/>
  <path class="flow flow-lg flow-d6" d="M300,176 C370,205 480,205 548,125"/>
  <path class="flow flow-lg flow-d7" d="M650,106 L720,106"/>
  <path class="flow flow-lg flow-d8" d="M830,106 L910,106"/>
  <!-- nodes -->
  <rect x="10" y="86" width="105" height="44" rx="22" fill="url(#grB)" filter="url(#gB)"/>
  <text x="62" y="113" text-anchor="middle" fill="#fff" font-weight="700" font-size="12" letter-spacing=".5">Z-Prepass</text>
  <rect x="195" y="20" width="125" height="44" rx="22" fill="url(#grB)" filter="url(#gB)"/>
  <text x="257" y="47" text-anchor="middle" fill="#fff" font-weight="700" font-size="12" letter-spacing=".5">Shadows</text>
  <rect x="200" y="138" width="100" height="44" rx="22" fill="url(#grB)" filter="url(#gB)"/>
  <text x="250" y="165" text-anchor="middle" fill="#fff" font-weight="700" font-size="12" letter-spacing=".5">GBuffer</text>
  <rect x="380" y="138" width="82" height="44" rx="22" fill="url(#grO)" filter="url(#gO)"/>
  <text x="421" y="165" text-anchor="middle" fill="#fff" font-weight="700" font-size="12" letter-spacing=".5">SSAO</text>
  <rect x="548" y="82" width="102" height="50" rx="25" fill="url(#grO)" filter="url(#gO)"/>
  <text x="599" y="112" text-anchor="middle" fill="#fff" font-weight="700" font-size="12" letter-spacing=".5">Lighting</text>
  <rect x="720" y="84" width="110" height="44" rx="22" fill="url(#grG)" filter="url(#gG)"/>
  <text x="775" y="111" text-anchor="middle" fill="#fff" font-weight="700" font-size="12" letter-spacing=".5">PostProcess</text>
  <rect x="910" y="86" width="70" height="40" rx="20" fill="url(#grR)" filter="url(#gR)"/>
  <text x="945" y="111" text-anchor="middle" fill="#fff" font-weight="600" font-size="11" letter-spacing=".3">Present</text>
  <!-- edge labels -->
  <text x="155" y="68" text-anchor="middle" fill="rgba(255,255,255,.4)" font-size="9.5" font-style="italic" letter-spacing=".3">depth</text>
  <text x="155" y="145" text-anchor="middle" fill="rgba(255,255,255,.4)" font-size="9.5" font-style="italic" letter-spacing=".3">depth</text>
  <text x="340" y="152" text-anchor="middle" fill="rgba(255,255,255,.4)" font-size="9.5" font-style="italic" letter-spacing=".3">normals</text>
  <text x="420" y="34" text-anchor="middle" fill="rgba(255,255,255,.4)" font-size="9.5" font-style="italic" letter-spacing=".3">shadow map</text>
  <text x="505" y="130" text-anchor="middle" fill="rgba(255,255,255,.4)" font-size="9.5" font-style="italic" letter-spacing=".3">AO</text>
  <text x="420" y="205" text-anchor="middle" fill="rgba(255,255,255,.4)" font-size="9.5" font-style="italic" letter-spacing=".3">GBuffer MRTs</text>
  <text x="685" y="98" text-anchor="middle" fill="rgba(255,255,255,.4)" font-size="9.5" font-style="italic" letter-spacing=".3">HDR</text>
  <text x="870" y="98" text-anchor="middle" fill="rgba(255,255,255,.4)" font-size="9.5" font-style="italic" letter-spacing=".3">LDR</text>
</svg>
<div style="margin-top:.4em;">
  <span style="display:inline-block;font-size:.72em;opacity:.4;letter-spacing:.03em;padding:.25em .7em;">nodes = render passes &nbsp;·&nbsp; edges = resource dependencies &nbsp;·&nbsp; forks = GPU parallelism</span>
</div>
</div>

You don't execute this graph directly. Every frame goes through three steps — first you **declare** all the passes and what they read/write, then the system **compiles** an optimized plan (ordering, memory, barriers), and finally it **executes** the result:

<!-- 3-step lifecycle — distinct style from the DAG above -->
<div class="fg-reveal" style="margin:.8em auto 1.2em;max-width:560px;">
  <div class="fg-lifecycle" style="display:flex;align-items:stretch;gap:0;border-radius:10px;overflow:hidden;border:1.5px solid rgba(var(--ds-indigo-rgb),.2);">
    <a href="#-the-declare-step" aria-label="Jump to Declare section" style="flex:1;padding:.7em .6em;text-align:center;background:rgba(var(--ds-info-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.12);text-decoration:none;color:inherit;transition:background .2s ease;cursor:pointer;" onmouseover="this.style.background='rgba(var(--ds-info-rgb),.14)'" onmouseout="this.style.background='rgba(var(--ds-info-rgb),.06)'">
      <div style="font-weight:800;font-size:.88em;letter-spacing:.04em;color:var(--ds-info);">①&ensp;DECLARE</div>
      <div style="font-size:.75em;opacity:.6;margin-top:.2em;">passes &amp; dependencies</div>
    </a>
    <span style="display:flex;align-items:center;flex-shrink:0;"><svg viewBox="0 0 28 20" width="20" height="14" fill="none"><line x1="2" y1="10" x2="17" y2="10" stroke="currentColor" stroke-width="2" stroke-linecap="round" opacity=".15"/><line x1="2" y1="10" x2="17" y2="10" class="flow flow-sm flow-current" style="animation-duration:1.4s"/><polyline points="15,4 24,10 15,16" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none" opacity=".35"/></svg></span>
    <a href="#-the-compile-step" aria-label="Jump to Compile section" style="flex:1;padding:.7em .6em;text-align:center;background:rgba(var(--ds-code-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.12);text-decoration:none;color:inherit;transition:background .2s ease;cursor:pointer;" onmouseover="this.style.background='rgba(var(--ds-code-rgb),.14)'" onmouseout="this.style.background='rgba(var(--ds-code-rgb),.06)'">
      <div style="font-weight:800;font-size:.88em;letter-spacing:.04em;color:var(--ds-code);">②&ensp;COMPILE</div>
      <div style="font-size:.75em;opacity:.6;margin-top:.2em;">order · aliases · barriers</div>
    </a>
    <span style="display:flex;align-items:center;flex-shrink:0;"><svg viewBox="0 0 28 20" width="20" height="14" fill="none"><line x1="2" y1="10" x2="17" y2="10" stroke="currentColor" stroke-width="2" stroke-linecap="round" opacity=".15"/><line x1="2" y1="10" x2="17" y2="10" class="flow flow-sm flow-current"/><polyline points="15,4 24,10 15,16" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" fill="none" opacity=".35"/></svg></span>
    <a href="#-the-execute-step" aria-label="Jump to Execute section" style="flex:1;padding:.7em .6em;text-align:center;background:rgba(var(--ds-success-rgb),.06);text-decoration:none;color:inherit;transition:background .2s ease;cursor:pointer;" onmouseover="this.style.background='rgba(var(--ds-success-rgb),.14)'" onmouseout="this.style.background='rgba(var(--ds-success-rgb),.06)'">
      <div style="font-weight:800;font-size:.88em;letter-spacing:.04em;color:var(--ds-success);">③&ensp;EXECUTE</div>
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
        <ul><li><code>addPass(setup, execute)</code></li></ul>
      </div>
      <div class="dp-stage">
        <div class="dp-title">DECLARE RESOURCES</div>
        <ul><li><code>create({1920,1080, RGBA8})</code></li></ul>
      </div>
      <div class="dp-stage">
        <div class="dp-title">WIRE DEPENDENCIES</div>
        <ul><li><code>read(h)</code> / <code>write(h)</code></li></ul>
      </div>
    </div>
    <div style="text-align:center;font-size:.82em;opacity:.6;margin-top:.3em">CPU only — the GPU is idle during this phase</div>
  </div>
</div>

<div class="fg-reveal" style="margin:1.2em 0;padding:1.1em 1.3em;border-radius:10px;border:1.5px dashed rgba(var(--ds-indigo-rgb),.3);background:rgba(var(--ds-indigo-rgb),.04);display:flex;align-items:center;gap:1.2em;flex-wrap:wrap;">
  <div style="flex:1;min-width:180px;">
    <div style="font-size:1.15em;font-weight:800;margin:.1em 0;">Handle #3</div>
    <div style="font-size:.82em;opacity:.6;">1920×1080 · RGBA8 · render target</div>
  </div>
  <div style="flex-shrink:0;padding:.4em .8em;border-radius:6px;background:rgba(var(--ds-warn-rgb),.1);color:var(--ds-warn);font-weight:700;font-size:.8em;">description only — no GPU memory yet</div>
</div>
<div style="text-align:center;font-size:.82em;opacity:.6;margin-top:-.2em;">
  Resources stay virtual at this stage — just a description and a handle. Memory comes later.
</div>

### 📦 Transient vs. imported

When you declare a resource, the graph needs to know one thing: **does it live inside this frame, or does it come from outside?**

<div class="fg-grid-stagger" style="display:grid;grid-template-columns:1fr 1fr;gap:1em;margin:1.2em 0;">
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(var(--ds-info-rgb),.3);overflow:hidden;">
    <div style="padding:.6em .9em;font-weight:800;font-size:.95em;background:rgba(var(--ds-info-rgb),.08);border-bottom:1px solid rgba(var(--ds-info-rgb),.15);color:var(--ds-info);">⚡ Transient</div>
    <div style="padding:.7em .9em;font-size:.88em;line-height:1.7;">
      <strong>Lifetime:</strong> single frame<br>
      <strong>Declared as:</strong> description (size, format)<br>
      <strong>GPU memory:</strong> allocated and aliased at compile<br>
      <strong>Aliasable:</strong> <span style="color:var(--ds-success);font-weight:700;">Yes</span> — non-overlapping lifetimes share physical memory<br>
      <strong>Examples:</strong> GBuffer MRTs, SSAO scratch, bloom scratch
    </div>
  </div>
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(var(--ds-code-rgb),.3);overflow:hidden;">
    <div style="padding:.6em .9em;font-weight:800;font-size:.95em;background:rgba(var(--ds-code-rgb),.08);border-bottom:1px solid rgba(var(--ds-code-rgb),.15);color:var(--ds-code);">📌 Imported</div>
    <div style="padding:.7em .9em;font-size:.88em;line-height:1.7;">
      <strong>Lifetime:</strong> across frames<br>
      <strong>Declared as:</strong> existing GPU handle<br>
      <strong>GPU memory:</strong> already allocated externally<br>
      <strong>Aliasable:</strong> <span style="color:var(--ds-danger);font-weight:700;">No</span> — lifetime extends beyond the frame<br>
      <strong>Examples:</strong> backbuffer, TAA history, shadow atlas, blue noise LUT
    </div>
  </div>
</div>

---

## ⚙️ The Compile Step

The declared DAG goes in; an optimized execution plan comes out — all on the CPU, in microseconds.

<div style="margin:1.2em 0;border-radius:12px;overflow:hidden;border:1.5px solid rgba(var(--ds-code-rgb),.25);">
  <!-- INPUT -->
  <div style="padding:.7em 1.1em;background:rgba(var(--ds-code-rgb),.08);border-bottom:1px solid rgba(var(--ds-code-rgb),.15);display:flex;align-items:center;gap:.8em;">
    <span style="font-weight:800;font-size:.85em;color:var(--ds-code);text-transform:uppercase;letter-spacing:.04em;">📥 In</span>
    <span style="font-size:.88em;opacity:.8;">declared passes + virtual resources + read/write edges</span>
  </div>
  <!-- PIPELINE -->
  <div style="padding:.8em 1.3em;background:rgba(var(--ds-code-rgb),.03);">
    <div style="display:grid;grid-template-columns:auto 1fr;gap:.35em 1em;align-items:center;font-size:.88em;">
      <span style="font-weight:700;color:var(--ds-code);">①</span><span><strong>Sort</strong> passes into dependency order</span>
      <span style="font-weight:700;color:var(--ds-code);">②</span><span><strong>Cull</strong> passes whose outputs are never read</span>
      <span style="font-weight:700;color:var(--ds-code);">③</span><span><strong>Allocate</strong> — alias memory so non-overlapping lifetimes share physical blocks</span>
      <span style="font-weight:700;color:var(--ds-code);">④</span><span><strong>Barrier</strong> — insert transitions at every resource state change</span>
      <span style="font-weight:700;color:var(--ds-code);">⑤</span><span><strong>Bind</strong> — attach physical memory, creating or reusing from a pool</span>
    </div>
  </div>
  <!-- OUTPUT -->
  <div style="padding:.7em 1.1em;background:rgba(var(--ds-success-rgb),.06);border-top:1px solid rgba(var(--ds-success-rgb),.15);display:flex;align-items:center;gap:.8em;">
    <span style="font-weight:800;font-size:.85em;color:var(--ds-success);text-transform:uppercase;letter-spacing:.04em;">📤 Out</span>
    <span style="font-size:.88em;opacity:.8;">ordered passes · aliased memory · barrier list · physical bindings</span>
  </div>
</div>

### Sorting and culling

**Sorting** is a topological sort over the dependency edges, producing a linear order that respects every read-before-write constraint.

**Culling** walks backward from the final outputs and removes any pass whose results are never read. Dead-code elimination for GPU work — entire passes vanish without a feature flag.

### Allocation and aliasing

The sorted order tells the compiler exactly when each resource is first written and last read — its **lifetime**. Two resources that are never alive at the same time can share the same physical memory.

<div style="margin:1em 0;border-radius:12px;overflow:hidden;border:1.5px solid rgba(var(--ds-code-rgb),.2);">
  <!-- Timeline -->
  <div style="padding:.8em 1.2em;">
    <div style="display:grid;grid-template-columns:100px repeat(6,1fr);gap:2px;font-size:.72em;opacity:.45;margin-bottom:.3em;">
      <div></div>
      <div style="text-align:center;">Pass 1</div>
      <div style="text-align:center;">Pass 2</div>
      <div style="text-align:center;">Pass 3</div>
      <div style="text-align:center;">Pass 4</div>
      <div style="text-align:center;">Pass 5</div>
      <div style="text-align:center;">Pass 6</div>
    </div>
    <div style="display:grid;grid-template-columns:100px repeat(6,1fr);gap:2px;margin-bottom:3px;">
      <div style="font-size:.8em;font-weight:700;display:flex;align-items:center;">GBuffer</div>
      <div style="background:rgba(var(--ds-code-rgb),.2);border-radius:4px 0 0 4px;height:24px;"></div>
      <div style="background:rgba(var(--ds-code-rgb),.2);height:24px;"></div>
      <div style="background:rgba(var(--ds-code-rgb),.2);border-radius:0 4px 4px 0;height:24px;"></div>
      <div style="height:24px;"></div>
      <div style="height:24px;"></div>
      <div style="height:24px;"></div>
    </div>
    <div style="display:grid;grid-template-columns:100px repeat(6,1fr);gap:2px;margin-bottom:.5em;">
      <div style="font-size:.8em;font-weight:700;display:flex;align-items:center;">Bloom</div>
      <div style="height:24px;"></div>
      <div style="height:24px;"></div>
      <div style="height:24px;"></div>
      <div style="background:rgba(var(--ds-code-rgb),.2);border-radius:4px 0 0 4px;height:24px;"></div>
      <div style="background:rgba(var(--ds-code-rgb),.2);height:24px;"></div>
      <div style="background:rgba(var(--ds-code-rgb),.2);border-radius:0 4px 4px 0;height:24px;"></div>
    </div>
    <div style="border-top:1px solid rgba(var(--ds-code-rgb),.1);padding-top:.5em;display:flex;align-items:center;gap:.8em;">
      <span style="font-size:.8em;opacity:.5;">No overlap →</span>
      <span style="padding:.25em .65em;border-radius:6px;background:rgba(var(--ds-success-rgb),.1);color:var(--ds-success);font-weight:700;font-size:.82em;">same heap, two resources</span>
    </div>
  </div>

  <!-- How it works -->
  <div style="padding:.7em 1.2em;font-size:.88em;line-height:1.7;border-top:1px solid rgba(var(--ds-code-rgb),.08);">
    The graph allocates a large <code>ID3D12Heap</code> (or <code>VkDeviceMemory</code>) and <strong>places</strong> multiple resources at different offsets within it. This is the single biggest VRAM win the graph provides.
  </div>

  <!-- Pitfalls -->
  <div style="padding:.7em 1.2em;background:rgba(234,179,8,.04);border-top:1px solid rgba(234,179,8,.12);">
    <div style="font-size:.78em;font-weight:700;text-transform:uppercase;letter-spacing:.03em;color:#ca8a04;margin-bottom:.4em;">⚠ Pitfalls</div>
    <div style="display:grid;grid-template-columns:auto 1fr;gap:.2em .8em;font-size:.85em;line-height:1.6;">
      <span style="font-weight:700;opacity:.7;">Garbage</span><span>Aliased memory has stale contents — first use must be a full clear or overwrite</span>
      <span style="font-weight:700;opacity:.7;">Transient only</span><span>Imported resources live across frames — only single-frame transients qualify</span>
      <span style="font-weight:700;opacity:.7;">Sync</span><span>The old resource must finish all GPU access before the new one touches the same memory</span>
    </div>
  </div>

  <!-- Production tricks -->
  <div style="padding:.7em 1.2em;background:rgba(var(--ds-code-rgb),.03);border-top:1px solid rgba(var(--ds-code-rgb),.08);">
    <div style="font-size:.78em;font-weight:700;text-transform:uppercase;letter-spacing:.03em;opacity:.45;margin-bottom:.4em;">Production optimizations</div>
    <div style="display:grid;grid-template-columns:auto 1fr;gap:.2em .8em;font-size:.85em;line-height:1.6;">
      <span style="font-weight:700;opacity:.7;">🪣 Bucketing</span><span>Round sizes to power-of-two (4, 8, 16 MB…) — fewer distinct sizes means heaps are reusable across resources</span>
      <span style="font-weight:700;opacity:.7;">♻️ Pooling</span><span>Keep heaps across frames. Next frame's <code>compile()</code> pulls from the pool — allocation cost drops to near zero</span>
    </div>
  </div>

  <!-- Part III link -->
  <div style="padding:.45em 1.2em;background:rgba(var(--ds-code-rgb),.06);border-top:1px solid rgba(var(--ds-code-rgb),.1);font-size:.8em;opacity:.6;text-align:center;">
    <a href="../frame-graph-production/">Part III</a> covers how UE5 and Frostbite implement these strategies.
  </div>
</div>

### Barriers

The compiler knows each resource's state at every point — render target, shader read, copy source — and inserts a barrier at every transition. Hand-written barriers are one of the most common sources of GPU bugs; the graph makes them automatic and correct by construction.

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

<div class="fg-reveal" style="margin:1.2em 0;padding:1em 1.2em;border-radius:10px;border:1.5px solid rgba(var(--ds-success-rgb),.2);background:rgba(var(--ds-success-rgb),.04);font-size:.92em;line-height:1.6;">
  Each execute lambda sees a <strong>fully resolved environment</strong> — barriers already placed, memory already allocated, resources ready to bind. The lambda just records draw calls, dispatches, and copies. All the intelligence lives in the compile step.
</div>

---

## 🔄 Rebuild Strategies

How often should the graph recompile? Three approaches, each a valid tradeoff:

<div class="fg-grid-stagger" style="display:grid;grid-template-columns:repeat(3,1fr);gap:1em;margin:1.2em 0;">
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid var(--ds-success);overflow:hidden;">
    <div style="padding:.5em .8em;background:rgba(var(--ds-success-rgb),.1);font-weight:800;font-size:.95em;border-bottom:1px solid rgba(var(--ds-success-rgb),.2);">
      🔄 Dynamic
    </div>
    <div style="padding:.7em .8em;font-size:.88em;line-height:1.6;">
      Rebuild every frame.<br>
      <strong>Cost:</strong> microseconds<br>
      <strong>Flex:</strong> full — passes appear/disappear freely<br>
      <span style="opacity:.6;font-size:.9em;">Used by: Frostbite</span>
    </div>
  </div>
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid var(--ds-info);overflow:hidden;">
    <div style="padding:.5em .8em;background:rgba(var(--ds-info-rgb),.1);font-weight:800;font-size:.95em;border-bottom:1px solid rgba(var(--ds-info-rgb),.2);">
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

<div class="fg-compare" style="margin:1.2em 0;display:grid;grid-template-columns:1fr 1fr;gap:0;border-radius:10px;overflow:hidden;border:2px solid rgba(var(--ds-indigo-rgb),.25);box-shadow:0 2px 8px rgba(0,0,0,.08);">
  <div style="padding:.6em 1em;font-weight:800;font-size:.95em;background:rgba(var(--ds-danger-rgb),.1);border-bottom:1.5px solid rgba(var(--ds-indigo-rgb),.15);border-right:1.5px solid rgba(var(--ds-indigo-rgb),.15);color:var(--ds-danger);">❌ Without Graph</div>
  <div style="padding:.6em 1em;font-weight:800;font-size:.95em;background:rgba(var(--ds-success-rgb),.1);border-bottom:1.5px solid rgba(var(--ds-indigo-rgb),.15);color:var(--ds-success);">✅ With Graph</div>

  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1.5px solid rgba(var(--ds-indigo-rgb),.15);background:rgba(var(--ds-danger-rgb),.02);">
    <strong>Memory aliasing</strong><br><span style="opacity:.65">Opt-in, fragile, rarely done</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);background:rgba(var(--ds-success-rgb),.02);">
    <strong>Memory aliasing</strong><br>Automatic — compiler sees all lifetimes. <strong style="color:var(--ds-success);">30–50% VRAM saved.</strong>
  </div>

  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1.5px solid rgba(var(--ds-indigo-rgb),.15);background:rgba(var(--ds-danger-rgb),.02);">
    <strong>Lifetimes</strong><br><span style="opacity:.65">Manual create/destroy, leaked or over-retained</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);background:rgba(var(--ds-success-rgb),.02);">
    <strong>Lifetimes</strong><br>Scoped to first..last use. Zero waste.
  </div>

  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1.5px solid rgba(var(--ds-indigo-rgb),.15);background:rgba(var(--ds-danger-rgb),.02);">
    <strong>Barriers</strong><br><span style="opacity:.65">Manual, per-pass</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);background:rgba(var(--ds-success-rgb),.02);">
    <strong>Barriers</strong><br>Automatic from declared read/write
  </div>

  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1.5px solid rgba(var(--ds-indigo-rgb),.15);background:rgba(var(--ds-danger-rgb),.02);">
    <strong>Pass reordering</strong><br><span style="opacity:.65">Breaks silently</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);background:rgba(var(--ds-success-rgb),.02);">
    <strong>Pass reordering</strong><br>Safe — compiler respects dependencies
  </div>

  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1.5px solid rgba(var(--ds-indigo-rgb),.15);background:rgba(var(--ds-danger-rgb),.02);">
    <strong>Pass culling</strong><br><span style="opacity:.65">Manual ifdef / flag checks</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);background:rgba(var(--ds-success-rgb),.02);">
    <strong>Pass culling</strong><br>Automatic — unused outputs = dead pass
  </div>

  <div style="padding:.55em .8em;font-size:.88em;border-right:1.5px solid rgba(var(--ds-indigo-rgb),.15);background:rgba(var(--ds-danger-rgb),.02);">
    <strong>Async compute</strong><br><span style="opacity:.65">Manual queue sync</span>
  </div>
  <div style="padding:.55em .8em;font-size:.88em;background:rgba(var(--ds-success-rgb),.02);">
    <strong>Async compute</strong><br>Compiler schedules across queues
  </div>
</div>

<div class="fg-reveal" style="margin:1.2em 0;padding:.8em 1em;border-radius:8px;background:linear-gradient(135deg,rgba(var(--ds-success-rgb),.06),rgba(var(--ds-info-rgb),.06));border:1px solid rgba(var(--ds-success-rgb),.2);font-size:.92em;line-height:1.6;">
🏭 <strong>Not theoretical.</strong> Frostbite reported <strong>50% VRAM reduction</strong> from aliasing at GDC 2017. UE5's RDG ships the same optimization today — every <code>FRDGTexture</code> marked as transient goes through the same aliasing pipeline we build in <a href="../frame-graph-build-it/">Part II</a>.
</div>

---

## 🔬 Advanced Features

The core graph handles scheduling, barriers, and aliasing — but the same DAG enables the compiler to go further. It can merge adjacent render passes to eliminate redundant state changes, schedule independent work across GPU queues, and split barrier transitions to hide cache-flush latency. [Part II](/posts/frame-graph-build-it/) builds the core; [Part III](/posts/frame-graph-production/) shows how production engines deploy all of these.

### 🔗 Pass Merging

<div class="fg-reveal" style="margin:0 0 1.2em;padding:.85em 1.1em;border-radius:10px;border:1px solid rgba(var(--ds-indigo-rgb),.15);background:linear-gradient(135deg,rgba(var(--ds-indigo-rgb),.04),transparent);font-size:.92em;line-height:1.65;">
Every render pass boundary has a cost — the GPU resolves attachments, flushes caches, stores intermediate results to memory, and sets up state for the next pass. When two adjacent passes share the same render targets, that boundary is pure overhead. <strong>Pass merging</strong> fuses compatible passes into a single API render pass, eliminating the round-trip entirely.
</div>

<div class="fg-grid-stagger" style="display:grid;grid-template-columns:1fr 1fr;gap:1em;margin:1.2em 0">
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(var(--ds-danger-rgb),.2);background:rgba(var(--ds-danger-rgb),.03);padding:1em 1.1em;">
    <div style="font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:var(--ds-danger);margin-bottom:.6em;">Without merging</div>
    <div style="font-size:.88em;line-height:1.7;font-family:ui-monospace,monospace;">
      <strong>Pass A</strong> GBuffer<br>
      <span style="opacity:.5">│</span> render<br>
      <span style="opacity:.5">│</span> <span style="color:var(--ds-danger);font-weight:600">store → VRAM ✗</span><br>
      <span style="opacity:.5">└</span> done<br>
      <br>
      <strong>Pass B</strong> Lighting<br>
      <span style="opacity:.5">│</span> <span style="color:var(--ds-danger);font-weight:600">load ← VRAM ✗</span><br>
      <span style="opacity:.5">│</span> render<br>
      <span style="opacity:.5">└</span> done
    </div>
    <div style="margin-top:.7em;padding-top:.6em;border-top:1px solid rgba(var(--ds-danger-rgb),.12);font-size:.82em;opacity:.7">2 render passes, 1 unnecessary round-trip</div>
  </div>
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(var(--ds-success-rgb),.25);background:rgba(var(--ds-success-rgb),.03);padding:1em 1.1em;">
    <div style="font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:var(--ds-success);margin-bottom:.6em;">With merging</div>
    <div style="font-size:.88em;line-height:1.7;font-family:ui-monospace,monospace;">
      <strong>Pass A+B</strong> merged<br>
      <span style="opacity:.5">│</span> render A<br>
      <span style="opacity:.5">│</span> <span style="color:var(--ds-success);font-weight:600">B reads in-place ✓</span><br>
      <span style="opacity:.5">│</span> render B<br>
      <span style="opacity:.5">└</span> store once → VRAM
    </div>
    <div style="margin-top:.7em;padding-top:.6em;border-top:1px solid rgba(var(--ds-success-rgb),.15);font-size:.82em;color:var(--ds-success);font-weight:600">1 render pass — no intermediate memory traffic</div>
  </div>
</div>

<div class="fg-reveal" style="margin:1.2em 0;padding:.75em 1em;border-radius:8px;background:rgba(var(--ds-info-rgb),.04);border:1px solid rgba(var(--ds-info-rgb),.1);font-size:.88em;line-height:1.6;">
<strong>When can two passes merge?</strong> Three conditions, all required:<br>
<span style="display:inline-block;width:1.4em;text-align:center;font-weight:700;color:var(--ds-info);">①</span> Same render target dimensions<br>
<span style="display:inline-block;width:1.4em;text-align:center;font-weight:700;color:var(--ds-info);">②</span> Second pass reads the first's output at the <strong>current pixel only</strong> (no arbitrary UV sampling)<br>
<span style="display:inline-block;width:1.4em;text-align:center;font-weight:700;color:var(--ds-info);">③</span> No external dependencies forcing a render pass break
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
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(var(--ds-danger-rgb),.25);overflow:hidden;">
    <div style="padding:.55em .9em;background:rgba(var(--ds-danger-rgb),.06);border-bottom:1px solid rgba(var(--ds-danger-rgb),.12);font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:var(--ds-danger);">Naive — 4 fences</div>
    <div style="padding:.8em .9em;font-family:ui-monospace,monospace;font-size:.82em;line-height:1.8;">
      <span style="opacity:.5;">Graphics:</span> [A] ──<span style="color:var(--ds-danger);font-weight:600;">fence</span>──→ [C]<br>
      <span style="opacity:.3;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>└──<span style="color:var(--ds-danger);font-weight:600;">fence</span>──→ [D]<br>
      <br>
      <span style="opacity:.5;">Compute:</span>&nbsp; [B] ──<span style="color:var(--ds-danger);font-weight:600;">fence</span>──→ [C]<br>
      <span style="opacity:.3;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>└──<span style="color:var(--ds-danger);font-weight:600;">fence</span>──→ [D]
    </div>
    <div style="padding:.5em .9em;border-top:1px solid rgba(var(--ds-danger-rgb),.1);font-size:.78em;opacity:.7;">Every cross-queue edge gets its own fence</div>
  </div>
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(var(--ds-success-rgb),.25);overflow:hidden;">
    <div style="padding:.55em .9em;background:rgba(var(--ds-success-rgb),.06);border-bottom:1px solid rgba(var(--ds-success-rgb),.12);font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:var(--ds-success);">Reduced — 1 fence</div>
    <div style="padding:.8em .9em;font-family:ui-monospace,monospace;font-size:.82em;line-height:1.8;">
      <span style="opacity:.5;">Graphics:</span> [A] ─────────→ [C] → [D]<br>
      <span style="opacity:.3;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</span>↑<br>
      <span style="opacity:.5;">Compute:</span>&nbsp; [B] ──<span style="color:var(--ds-success);font-weight:600;">fence</span>──┘<br>
      <br>
      B's fence covers both C and D<br>
      <span style="opacity:.6;">(D is after C on graphics queue)</span>
    </div>
    <div style="padding:.5em .9em;border-top:1px solid rgba(var(--ds-success-rgb),.1);font-size:.78em;color:var(--ds-success);font-weight:600;">Redundant fences removed transitively</div>
  </div>
</div>

#### ⚖️ What makes overlap good or bad

Solving fences is the easy part — the compiler handles that. The harder question is whether overlapping two specific passes actually helps:

<div class="fg-grid-stagger" style="display:grid;grid-template-columns:1fr 1fr;gap:1em;margin:1.2em 0;">
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(var(--ds-success-rgb),.25);overflow:hidden;">
    <div style="padding:.6em .9em;background:rgba(var(--ds-success-rgb),.05);border-bottom:1px solid rgba(var(--ds-success-rgb),.12);font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:var(--ds-success);">✓ Complementary</div>
    <div style="padding:.8em .9em;font-size:.88em;line-height:1.6;">
      Graphics is <strong>ROP/rasterizer-bound</strong> (shadow rasterization, geometry-dense passes) while compute runs <strong>ALU-heavy</strong> shaders (SSAO, volumetrics). Different hardware units stay busy — real parallelism, measurable frame time reduction.
    </div>
  </div>
  <div class="fg-hoverable" style="border-radius:10px;border:1.5px solid rgba(var(--ds-danger-rgb),.25);overflow:hidden;">
    <div style="padding:.6em .9em;background:rgba(var(--ds-danger-rgb),.05);border-bottom:1px solid rgba(var(--ds-danger-rgb),.12);font-weight:800;font-size:.85em;text-transform:uppercase;letter-spacing:.04em;color:var(--ds-danger);">✗ Competing</div>
    <div style="padding:.8em .9em;font-size:.88em;line-height:1.6;">
      Both passes are <strong>bandwidth-bound</strong> or both <strong>ALU-heavy</strong> — they thrash each other's L2 cache and fight for CU time. The frame gets <em>slower</em> than running them sequentially. Common trap: overlapping two fullscreen post-effects.
    </div>
  </div>
</div>

#### 🧭 Should this pass go async?

<div style="margin:1.5em 0;display:flex;align-items:stretch;gap:0;font-size:.88em;">
  <div style="flex:1;min-width:0;display:flex;flex-direction:column;align-items:center;">
    <div style="width:100%;flex:1;display:flex;align-items:center;justify-content:center;padding:.7em .6em;border-radius:8px;background:rgba(var(--ds-indigo-rgb),.06);border:1.5px solid rgba(var(--ds-indigo-rgb),.15);text-align:center;font-weight:700;">Is Compute Shader?</div>
    <div style="margin-top:.4em;font-size:.78em;color:var(--ds-danger);text-align:center;line-height:1.35;opacity:.85;">✗ requires raster pipeline</div>
  </div>
  <div style="display:flex;align-items:center;padding:0 .2em;flex-shrink:0;"><svg viewBox="0 0 44 28" width="44" height="28" fill="none"><line x1="4" y1="14" x2="28" y2="14" stroke="var(--ds-success)" stroke-width="2" stroke-linecap="round" opacity=".15"/><line x1="4" y1="14" x2="28" y2="14" class="flow flow-md flow-green" style="animation-duration:1.8s"/><polyline points="24,5 38,14 24,23" stroke="var(--ds-success)" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" fill="none" opacity=".35"/></svg></div>
  <div style="flex:1;min-width:0;display:flex;flex-direction:column;align-items:center;">
    <div style="width:100%;flex:1;display:flex;align-items:center;justify-content:center;padding:.7em .6em;border-radius:8px;background:rgba(var(--ds-indigo-rgb),.06);border:1.5px solid rgba(var(--ds-indigo-rgb),.15);text-align:center;font-weight:700;">Zero Resource Contention with Graphics?</div>
    <div style="margin-top:.4em;font-size:.78em;color:var(--ds-danger);text-align:center;line-height:1.35;opacity:.85;">✗ data hazard with graphics</div>
  </div>
  <div style="display:flex;align-items:center;padding:0 .2em;flex-shrink:0;"><svg viewBox="0 0 44 28" width="44" height="28" fill="none"><line x1="4" y1="14" x2="28" y2="14" stroke="var(--ds-success)" stroke-width="2" stroke-linecap="round" opacity=".15"/><line x1="4" y1="14" x2="28" y2="14" class="flow flow-md flow-green"/><polyline points="24,5 38,14 24,23" stroke="var(--ds-success)" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" fill="none" opacity=".35"/></svg></div>
  <div style="flex:1;min-width:0;display:flex;flex-direction:column;align-items:center;">
    <div style="width:100%;flex:1;display:flex;align-items:center;justify-content:center;padding:.7em .6em;border-radius:8px;background:rgba(var(--ds-indigo-rgb),.06);border:1.5px solid rgba(var(--ds-indigo-rgb),.15);text-align:center;font-weight:700;">Has Complementary Resource Usage?</div>
    <div style="margin-top:.4em;font-size:.78em;color:var(--ds-danger);text-align:center;line-height:1.35;opacity:.85;">✗ same HW units — no overlap</div>
  </div>
  <div style="display:flex;align-items:center;padding:0 .2em;flex-shrink:0;"><svg viewBox="0 0 44 28" width="44" height="28" fill="none"><line x1="4" y1="14" x2="28" y2="14" stroke="var(--ds-success)" stroke-width="2" stroke-linecap="round" opacity=".15"/><line x1="4" y1="14" x2="28" y2="14" class="flow flow-md flow-green" style="animation-duration:2.2s"/><polyline points="24,5 38,14 24,23" stroke="var(--ds-success)" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" fill="none" opacity=".35"/></svg></div>
  <div style="flex:1;min-width:0;display:flex;flex-direction:column;align-items:center;">
    <div style="width:100%;flex:1;display:flex;align-items:center;justify-content:center;padding:.7em .6em;border-radius:8px;background:rgba(var(--ds-indigo-rgb),.06);border:1.5px solid rgba(var(--ds-indigo-rgb),.15);text-align:center;font-weight:700;">Has Enough Work Between Fences?</div>
    <div style="margin-top:.4em;font-size:.78em;color:var(--ds-danger);text-align:center;line-height:1.35;opacity:.85;">✗ sync cost exceeds gain</div>
  </div>
  <div style="display:flex;align-items:center;padding:0 .2em;flex-shrink:0;"><svg viewBox="0 0 44 28" width="44" height="28" fill="none"><line x1="4" y1="14" x2="28" y2="14" stroke="var(--ds-success)" stroke-width="2" stroke-linecap="round" opacity=".15"/><line x1="4" y1="14" x2="28" y2="14" class="flow flow-md flow-green" style="animation-duration:1.7s"/><polyline points="24,5 38,14 24,23" stroke="var(--ds-success)" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" fill="none" opacity=".35"/></svg></div>
  <div style="flex:1;min-width:0;display:flex;flex-direction:column;align-items:center;">
    <div style="width:100%;flex:1;display:flex;align-items:center;justify-content:center;padding:.7em .6em;border-radius:8px;background:rgba(var(--ds-success-rgb),.08);border:1.5px solid rgba(var(--ds-success-rgb),.25);text-align:center;font-weight:800;color:var(--ds-success);">ASYNC COMPUTE ✓</div>
  </div>
</div>
<div style="font-size:.82em;opacity:.6;margin-top:-.2em;text-align:center;">Good candidates: SSAO alongside ROP-bound geometry, volumetrics during shadow rasterization, particle sim during UI.</div>

Try it yourself — move compute-eligible passes between queues and see how fence count and frame time change:

{{< interactive-async >}}

### ✂️ Split Barriers

Async compute hides latency by overlapping work across *queues*. Split barriers achieve the same effect on a *single queue* — by spreading one resource transition across multiple passes instead of stalling on it.

A **regular barrier** does a cache flush, state change, and cache invalidate in one blocking command — the GPU finishes the source pass, stalls while the transition completes, then starts the next pass. Every microsecond of that stall is wasted.

A **split barrier** breaks the transition into two halves and spreads them apart:

<div style="margin:1.4em 0;font-size:.88em;">
  <div style="display:flex;align-items:stretch;gap:0;border-radius:10px;overflow:hidden;border:1.5px solid rgba(var(--ds-indigo-rgb),.15);">
    <div style="background:rgba(var(--ds-info-rgb),.08);padding:.8em 1em;border-right:3px solid var(--ds-info);min-width:130px;text-align:center;">
      <div style="font-weight:800;font-size:.95em;">Source pass</div>
      <div style="font-size:.78em;opacity:.6;margin-top:.2em;">writes texture</div>
    </div>
    <div style="background:rgba(var(--ds-info-rgb),.15);padding:.5em .8em;display:flex;align-items:center;min-width:50px;border-right:1px dashed rgba(var(--ds-indigo-rgb),.3);">
      <div style="text-align:center;width:100%;">
        <div style="font-size:.7em;font-weight:700;color:var(--ds-info);text-transform:uppercase;letter-spacing:.04em;">BEGIN</div>
        <div style="font-size:.68em;opacity:.5;">flush caches</div>
      </div>
    </div>
    <div style="flex:1;background:repeating-linear-gradient(90deg,rgba(var(--ds-indigo-rgb),.04) 0,rgba(var(--ds-indigo-rgb),.04) 50%,rgba(var(--ds-indigo-rgb),.08) 50%,rgba(var(--ds-indigo-rgb),.08) 100%);background-size:50% 100%;display:flex;align-items:stretch;min-width:200px;">
      <div style="flex:1;padding:.6em .7em;text-align:center;border-right:1px dashed rgba(var(--ds-indigo-rgb),.15);">
        <div style="font-weight:700;font-size:.85em;">Pass C</div>
        <div style="font-size:.72em;opacity:.5;">unrelated work</div>
      </div>
      <div style="flex:1;padding:.6em .7em;text-align:center;">
        <div style="font-weight:700;font-size:.85em;">Pass D</div>
        <div style="font-size:.72em;opacity:.5;">unrelated work</div>
      </div>
    </div>
    <div style="background:rgba(var(--ds-success-rgb),.15);padding:.5em .8em;display:flex;align-items:center;min-width:50px;border-left:1px dashed rgba(var(--ds-indigo-rgb),.3);">
      <div style="text-align:center;width:100%;">
        <div style="font-size:.7em;font-weight:700;color:var(--ds-success);text-transform:uppercase;letter-spacing:.04em;">END</div>
        <div style="font-size:.68em;opacity:.5;">invalidate</div>
      </div>
    </div>
    <div style="background:rgba(var(--ds-success-rgb),.08);padding:.8em 1em;border-left:3px solid var(--ds-success);min-width:130px;text-align:center;">
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
  <div class="fg-hoverable" style="border-radius:8px;border:1.5px solid rgba(var(--ds-danger-rgb),.2);background:rgba(var(--ds-danger-rgb),.03);padding:.7em .8em;text-align:center;">
    <div style="font-weight:800;font-size:1.3em;color:var(--ds-danger);">0</div>
    <div style="font-size:.8em;font-weight:600;margin:.25em 0;">passes</div>
    <div style="font-size:.78em;opacity:.7;">No gap — degenerates into a regular barrier with extra API cost</div>
  </div>
  <div class="fg-hoverable" style="border-radius:8px;border:1.5px solid rgba(234,179,8,.2);background:rgba(234,179,8,.03);padding:.7em .8em;text-align:center;">
    <div style="font-weight:800;font-size:1.3em;color:#eab308;">1</div>
    <div style="font-size:.8em;font-weight:600;margin:.25em 0;">pass</div>
    <div style="font-size:.78em;opacity:.7;">Marginal — might not cover the full flush latency</div>
  </div>
  <div class="fg-hoverable" style="border-radius:8px;border:1.5px solid rgba(var(--ds-success-rgb),.25);background:rgba(var(--ds-success-rgb),.03);padding:.7em .8em;text-align:center;">
    <div style="font-weight:800;font-size:1.3em;color:var(--ds-success);">2+</div>
    <div style="font-size:.8em;font-weight:600;margin:.25em 0;">passes</div>
    <div style="font-size:.78em;opacity:.7;">Cache flush fully hidden — measurable frame time reduction</div>
  </div>
  <div class="fg-hoverable" style="border-radius:8px;border:1.5px solid rgba(var(--ds-indigo-rgb),.2);background:rgba(var(--ds-indigo-rgb),.03);padding:.7em .8em;text-align:center;">
    <div style="font-weight:800;font-size:1.3em;color:var(--ds-indigo);">⚡</div>
    <div style="font-size:.8em;font-weight:600;margin:.25em 0;">cross-queue</div>
    <div style="font-size:.78em;opacity:.7;">Can't split across queues — use an async fence instead</div>
  </div>
</div>

Try it — drag the BEGIN marker left to widen the overlap gap and watch the stall disappear:

{{< interactive-split-barriers >}}

<div class="fg-reveal" style="margin:1.4em 0;padding:.85em 1.1em;border-radius:10px;background:linear-gradient(135deg,rgba(var(--ds-success-rgb),.06),rgba(var(--ds-info-rgb),.06));border:1px solid rgba(var(--ds-success-rgb),.15);font-size:.92em;line-height:1.65;">
That's all the theory. <a href="../frame-graph-build-it/" style="font-weight:600;">Part II</a> implements the core — barriers, culling, aliasing — in ~300 lines of C++. <a href="../frame-graph-production/" style="font-weight:600;">Part III</a> shows how production engines deploy all of these at scale.
</div>

---

## 🎛️ Putting It All Together

You've now seen every piece the compiler works with — topological sorting, pass culling, barrier insertion, async compute scheduling, memory aliasing, split barriers. In a simple 5-pass pipeline these feel manageable. In a production renderer? You're looking at **15–25 passes, 30+ resource edges, and dozens of implicit dependencies** — all inferred from `read()` and `write()` calls that no human can hold in their head at once.

<div class="fg-reveal" style="margin:1.2em 0;padding:.85em 1.1em;border-radius:10px;border:1.5px solid rgba(var(--ds-code-rgb),.2);background:linear-gradient(135deg,rgba(var(--ds-code-rgb),.05),transparent);font-size:.92em;line-height:1.65;">
<strong>This is the trade-off at the heart of every render graph.</strong> Dependencies become <em>implicit</em> — the graph infers ordering from data flow, which means you never declare "pass A must run before pass B." That's powerful: the compiler can reorder, cull, and parallelize freely. But it also means <strong>dependencies are hidden</strong>. Miss a <code>read()</code> call and the graph silently reorders two passes that shouldn't overlap. Add an assert and you'll catch the <em>symptom</em> — but not the missing edge that caused it.
</div>

Since the frame graph is a DAG, every dependency is explicitly encoded in the structure. That means you can build tools to **visualize** the entire pipeline — every pass, every resource edge, every implicit ordering decision — something that's impossible when barriers and ordering are scattered across hand-written render code.

The explorer below is a production-scale graph. Toggle each compiler feature on and off to see exactly what it contributes. Click any pass to inspect its dependencies — every edge was inferred from `read()` and `write()` calls, not hand-written.

{{< interactive-full-pipeline >}}

---

<div style="margin:2em 0 0;padding:1em 1.2em;border-radius:10px;border:1px solid rgba(var(--ds-info-rgb),.2);background:rgba(var(--ds-info-rgb),.03);display:flex;justify-content:flex-end;">
  <a href="../frame-graph-build-it/" style="text-decoration:none;font-weight:700;font-size:.95em;">
    Next: Part II — Build It →
  </a>
</div>
