---
title: "Frame Graph — Production Engines"
date: 2026-02-11
draft: true
description: "How UE5, Frostbite, and Unity implement frame graphs at scale — plus the upgrade roadmap from MVP to production."
tags: ["rendering", "frame-graph", "gpu", "architecture", "ue5"]
categories: ["analysis"]
series: ["Rendering Architecture"]
showTableOfContents: false
---

{{< article-nav >}}

<div style="margin:0 0 1.5em;padding:.7em 1em;border-radius:8px;background:rgba(99,102,241,.04);border:1px solid rgba(99,102,241,.12);font-size:.88em;line-height:1.6;opacity:.85;">
📖 <strong>Part III of III.</strong>&ensp; <a href="/posts/frame-graph-theory/">Theory</a> → <a href="/posts/frame-graph-build-it/">Build It</a> → <em>Production Engines</em>
</div>

<div style="border-left:4px solid #6366f1;background:linear-gradient(135deg,rgba(99,102,241,.06),transparent);border-radius:0 10px 10px 0;padding:1em 1.3em;margin:1em 0;font-size:.95em;font-style:italic;line-height:1.55">
How UE5, Frostbite, and Unity implement the same ideas at scale — what they added, what they compromised, and where they still differ.
</div>

[Part II](/posts/frame-graph-build-it/) left us with a working frame graph — automatic barriers, pass culling, and memory aliasing in ~300 lines of C++. That's a solid MVP, but production engines face problems we didn't: parallel command recording, subpass merging for mobile GPUs, async compute scheduling, and managing thousands of passes across legacy codebases. This article examines how three major engines solved those problems, then maps out the path from MVP to production.

---

## 🏭 Production Engines

### 🎮 UE5's Rendering Dependency Graph (RDG)

UE5's RDG is the frame graph you're most likely to work with. It was retrofitted onto a 25-year-old renderer, so every design choice reflects a tension: do this properly *and* don't break the 10,000 existing draw calls.

<div class="diagram-phases">
  <div class="dph-col" style="border-color:#3b82f6">
    <div class="dph-title" style="color:#3b82f6">Render thread (setup)</div>
    <div class="dph-body">
      <code>FRDGBuilder::AddPass(...)</code><br>
      <code>FRDGBuilder::AddPass(...)</code><br>
      <code>FRDGBuilder::AddPass(...)</code><br>
      <code>CreateTexture(...)</code><br>
      <code>RegisterExternalTexture(...)</code><br>
      <span style="opacity:.6">↓ accumulates full DAG<br>(passes, resources, edges)</span>
    </div>
  </div>
  <div style="display:flex;align-items:center;font-size:1.6em;color:#3b82f6;font-weight:700">→</div>
  <div class="dph-col" style="border-color:#22c55e">
    <div class="dph-title" style="color:#22c55e">Render thread (execute)</div>
    <div class="dph-body">
      <code>FRDGBuilder::Execute()</code><br>
      ├ compile<br>
      ├ allocate<br>
      ├ barriers<br>
      └ record cmds
    </div>
  </div>
</div>

**Pass declaration.** Each `AddPass` takes a parameter struct + execute lambda. The struct *is* the setup phase:

<div class="diagram-macro">
  <div class="dm-code">
    <span style="color:#8b5cf6">BEGIN_SHADER_PARAMETER_STRUCT(...)</span><br>
    &nbsp;&nbsp;SHADER_PARAMETER_RDG_TEXTURE(Input)<br>
    &nbsp;&nbsp;RENDER_TARGET_BINDING_SLOT(Output)<br>
    <span style="color:#8b5cf6">END_SHADER_PARAMETER_STRUCT()</span>
  </div>
  <div class="dm-arrow">→</div>
  <div class="dm-result">
    <span style="color:#22c55e">read edge</span> ←<br>
    <span style="color:#ef4444">write edge</span> ← &nbsp;→ DAG
  </div>
</div>
<div style="font-size:.78em;opacity:.6;margin-top:-.3em">Macro generates metadata → RDG extracts dependency edges. No separate setup lambda needed.</div>

**Pass flags & resource types:**

<div style="display:flex;gap:1em;flex-wrap:wrap;margin:1em 0">
  <div style="flex:1;min-width:260px;border:1px solid rgba(59,130,246,.25);border-radius:10px;overflow:hidden">
    <div style="background:linear-gradient(135deg,rgba(59,130,246,.12),rgba(59,130,246,.05));padding:.6em 1em;font-weight:700;font-size:.9em;color:#3b82f6;border-bottom:1px solid rgba(59,130,246,.15)">Pass Flags</div>
    <div style="padding:.6em 1em;font-size:.85em;line-height:1.8">
      <code>ERDGPassFlags::Raster</code> — Graphics queue, render targets<br>
      <code>ERDGPassFlags::Compute</code> — Graphics queue, compute dispatch<br>
      <code>ERDGPassFlags::AsyncCompute</code> — Async compute queue<br>
      <code>ERDGPassFlags::NeverCull</code> — Exempt from dead-pass culling<br>
      <code>ERDGPassFlags::Copy</code> — Copy queue operations<br>
      <code>ERDGPassFlags::SkipRenderPass</code> — Raster pass that manages its own render pass
    </div>
  </div>
  <div style="flex:1;min-width:260px;border:1px solid rgba(139,92,246,.25);border-radius:10px;overflow:hidden">
    <div style="background:linear-gradient(135deg,rgba(139,92,246,.12),rgba(139,92,246,.05));padding:.6em 1em;font-weight:700;font-size:.9em;color:#8b5cf6;border-bottom:1px solid rgba(139,92,246,.15)">Resource Types</div>
    <div style="padding:.6em 1em;font-size:.85em;line-height:1.8">
      <code>FRDGTexture</code> / <code>FRDGTextureRef</code> — Render targets, SRVs, UAVs<br>
      <code>FRDGBuffer</code> / <code>FRDGBufferRef</code> — Structured, vertex/index, indirect args<br>
      <code>FRDGUniformBuffer</code> — Uniform/constant buffer references<br>
      Created via <code>CreateTexture()</code> (transient) or <code>RegisterExternalTexture()</code> (imported)
    </div>
  </div>
</div>
<div style="font-size:.82em;opacity:.6;margin-top:-.3em">Both go through the same aliasing and barrier system.</div>

**Key systems — how they map to our MVP:**

<div class="diagram-ftable">
<table>
  <tr><th>Feature</th><th>Our MVP</th><th>UE5 RDG</th></tr>
  <tr><td><strong>Transient alloc</strong></td><td>free-list scan per frame</td><td>pooled allocator amortized across frames (<code>FRDGTransientResourceAllocator</code>)</td></tr>
  <tr><td><strong>Barriers</strong></td><td>one-at-a-time</td><td>batched + split begin/end via <code>FRDGBarrierBatchBegin</code>/<code>End</code></td></tr>
  <tr><td><strong>Pass culling</strong></td><td>backward walk from output</td><td>refcount-based + skip allocation for culled passes</td></tr>
  <tr><td><strong>Cmd recording</strong></td><td>single thread</td><td>parallel <code>FRHICommandList</code> — one per pass group, merged at submit</td></tr>
  <tr><td><strong>Rebuild</strong></td><td>dynamic</td><td>hybrid (cached topology, invalidated on change)</td></tr>
</table>
</div>

**Navigating the RDG source.** The key entry points when reading UE5's RDG code:

<div class="diagram-steps">
  <div class="ds-step">
    <div class="ds-num">1</div>
    <div><code>RenderGraphBuilder.h</code> — <code>FRDGBuilder</code> is the graph object. <code>AddPass()</code>, <code>CreateTexture()</code>, <code>Execute()</code> are all here. Start reading here.</div>
  </div>
  <div class="ds-step">
    <div class="ds-num">2</div>
    <div><code>RenderGraphPass.h</code> — <code>FRDGPass</code> stores the parameter struct, execute lambda, and pass flags. The macro-generated metadata lives on the parameter struct.</div>
  </div>
  <div class="ds-step">
    <div class="ds-num">3</div>
    <div><code>RenderGraphResources.h</code> — <code>FRDGTexture</code>, <code>FRDGBuffer</code>, and their SRV/UAV views. Tracks current state for barrier emission. Check <code>FRDGResource::GetRHI()</code> to see when virtual becomes physical.</div>
  </div>
  <div class="ds-step">
    <div class="ds-num">4</div>
    <div><code>RenderGraphPrivate.h</code> — The compile phase: topological sort, pass culling, barrier batching, async compute fence insertion. The core algorithms live here.</div>
  </div>
</div>

<div style="display:flex;align-items:flex-start;gap:.8em;border:1px solid rgba(34,197,94,.2);border-radius:10px;padding:1em 1.2em;margin:1em 0;background:linear-gradient(135deg,rgba(34,197,94,.05),transparent)">
  <span style="font-size:1.4em;line-height:1">🔍</span>
  <div style="font-size:.9em;line-height:1.55"><strong>RDG Insights.</strong> Enable via the Unreal editor to visualize the full pass graph, resource lifetimes, and barrier placement. Use <code>r.RDG.Debug</code> CVars for validation: <code>r.RDG.Debug.FlushGPU</code> serializes execution for debugging, <code>r.RDG.Debug.ExtendResourceLifetimes</code> disables aliasing to isolate corruption bugs. The frame is data — export it, diff it, analyze offline.</div>
</div>

**The legacy boundary.** The biggest practical challenge with RDG isn't the graph itself — it's the seam between RDG-managed passes and legacy `FRHICommandList` code. At this boundary:

- Barriers must be inserted manually (RDG can't see what the legacy code does)
- Resources must be "extracted" from RDG via `ConvertToExternalTexture()` before legacy code can use them
- Re-importing back into RDG requires `RegisterExternalTexture()` with correct state tracking

This boundary is shrinking every release as Epic migrates more passes to RDG, but in practice you'll still hit it when integrating third-party plugins or older rendering features.

**What RDG gets wrong (or leaves on the table):**

<div class="diagram-limits">
  <div class="dl-title">RDG Limitations</div>
  <div class="dl-item"><span class="dl-x">✗</span> <strong>Incomplete migration</strong> — Legacy FRHICommandList ←→ RDG boundary = manual barriers at the seam</div>
  <div class="dl-item"><span class="dl-x">✗</span> <strong>Macro-heavy API</strong> — BEGIN_SHADER_PARAMETER_STRUCT → opaque, no debugger stepping, fights dynamic composition</div>
  <div class="dl-item"><span class="dl-x">✗</span> <strong>Transient-only aliasing</strong> — Imported resources never aliased, even when lifetime is fully known within the frame</div>
  <div class="dl-item"><span class="dl-x">✗</span> <strong>No automatic subpass merging</strong> — Delegated to RHI — graph can't optimize tile-based GPUs directly</div>
  <div class="dl-item"><span class="dl-x">✗</span> <strong>Async compute is opt-in</strong> — Manual ERDGPassFlags::AsyncCompute tagging. Compiler trusts, doesn't discover.</div>
  <div class="dl-item"><span class="dl-x">✗</span> <strong>No cross-pass resource versioning</strong> — Unlike our MVP's read/write versioning, RDG uses a flat dependency model. Parallel reads are implicit.</div>
</div>

### ❄️ Where Frostbite started

Frostbite's frame graph (O'Donnell & Barczak, GDC 2017: *"FrameGraph: Extensible Rendering Architecture in Frostbite"*) is where the modern render graph concept originates.

<div class="diagram-innovations">
  <div class="di-title">Frostbite innovations that shaped every later engine</div>
  <div class="di-row"><div class="di-label">Transient resources</div><div>First production aliasing. 50% VRAM saved on Battlefield 1.</div></div>
  <div class="di-row"><div class="di-label">Split barriers</div><div>begin/end placement for GPU overlap. UE5 adopted this.</div></div>
  <div class="di-row"><div class="di-label">Graph export</div><div>DOT-format debug. Every engine since has built equivalent.</div></div>
  <div class="di-row"><div class="di-label">Dynamic rebuild</div><div>Full rebuild every frame. "Compile cost so low, caching adds complexity for nothing."</div></div>
</div>

**Frostbite vs UE5 — design spectrum:**

<div class="diagram-spectrum">
  <div class="ds-labels"><span>More aggressive</span><span>More conservative</span></div>
  <div class="ds-bar"></div>
  <div class="ds-cards">
    <div class="ds-card" style="border-color:#22c55e">
      <div class="ds-name" style="color:#22c55e">Frostbite</div>
      <span style="color:#22c55e">✓</span> fully dynamic<br>
      <span style="color:#22c55e">✓</span> alias everything<br>
      <span style="color:#22c55e">✓</span> subpass merging<br>
      <span style="color:#22c55e">✓</span> auto async<br>
      <span style="color:#ef4444">✗</span> no legacy support<br>
      <span style="color:#ef4444">✗</span> closed engine
    </div>
    <div class="ds-card" style="border-color:#3b82f6">
      <div class="ds-name" style="color:#3b82f6">UE5 RDG</div>
      <span style="color:#22c55e">✓</span> hybrid/cached<br>
      <span style="color:#22c55e">✓</span> transient only<br>
      <span style="color:#ef4444">✗</span> RHI-delegated<br>
      <span style="color:#ef4444">✗</span> opt-in async<br>
      <span style="color:#22c55e">✓</span> legacy compat<br>
      <span style="color:#22c55e">✓</span> 3P game code
    </div>
  </div>
  <div style="font-size:.78em;opacity:.6;margin-top:.5em">Frostbite controls the full engine. UE5 must support 25 years of existing code.</div>
</div>

### 🔧 Other implementations

<div style="border:1px solid rgba(34,197,94,.2);border-radius:10px;padding:1em 1.2em;margin:1em 0;background:linear-gradient(135deg,rgba(34,197,94,.05),transparent)">
  <div style="font-weight:700;color:#22c55e;margin-bottom:.3em">Unity — SRP Render Graph</div>
  <div style="font-size:.9em;line-height:1.55">Shipped as part of the Scriptable Render Pipeline. Handles pass culling and transient resource aliasing in URP/HDRP backends. Async compute support varies by platform. Designed for portability across mobile and desktop, so it avoids the more aggressive GPU-specific optimizations.</div>
</div>

### 📊 Comparison

<div style="overflow-x:auto;margin:1em 0">
<table style="width:100%;border-collapse:collapse;border-radius:10px;overflow:hidden;font-size:.9em">
  <thead>
    <tr style="background:linear-gradient(135deg,rgba(99,102,241,.1),rgba(59,130,246,.08))">
      <th style="padding:.7em 1em;text-align:left;border-bottom:2px solid rgba(99,102,241,.2)">Feature</th>
      <th style="padding:.7em 1em;text-align:center;border-bottom:2px solid rgba(59,130,246,.2);color:#3b82f6">UE5 RDG</th>
      <th style="padding:.7em 1em;text-align:center;border-bottom:2px solid rgba(34,197,94,.2);color:#22c55e">Frostbite</th>
      <th style="padding:.7em 1em;text-align:center;border-bottom:2px solid rgba(139,92,246,.2);color:#8b5cf6">Unity SRP</th>
    </tr>
  </thead>
  <tbody>
    <tr><td style="padding:.5em 1em;font-weight:600">Rebuild strategy</td><td style="padding:.5em 1em;text-align:center">hybrid (cached)</td><td style="padding:.5em 1em;text-align:center">dynamic</td><td style="padding:.5em 1em;text-align:center">dynamic</td></tr>
    <tr style="background:rgba(127,127,127,.04)"><td style="padding:.5em 1em;font-weight:600">Pass culling</td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span> auto</td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span> refcount</td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span> auto</td></tr>
    <tr><td style="padding:.5em 1em;font-weight:600">Memory aliasing</td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span> transient</td><td style="padding:.5em 1em;text-align:center;font-weight:600;color:#22c55e">✓ full</td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span> transient</td></tr>
    <tr style="background:rgba(127,127,127,.04)"><td style="padding:.5em 1em;font-weight:600">Async compute</td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span> flag-based</td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span></td><td style="padding:.5em 1em;text-align:center;opacity:.6">varies</td></tr>
    <tr><td style="padding:.5em 1em;font-weight:600">Split barriers</td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span></td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span></td><td style="padding:.5em 1em;text-align:center"><span style="color:#ef4444">✗</span></td></tr>
    <tr style="background:rgba(127,127,127,.04)"><td style="padding:.5em 1em;font-weight:600">Parallel recording</td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span></td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span></td><td style="padding:.5em 1em;text-align:center;opacity:.6">limited</td></tr>
    <tr><td style="padding:.5em 1em;font-weight:600">Buffer tracking</td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span></td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span></td><td style="padding:.5em 1em;text-align:center"><span style="color:#22c55e">✓</span></td></tr>
  </tbody>
</table>
</div>

---

## � How Each Engine Scales It

[Part I](/posts/frame-graph-theory/) covered all the theory — pass merging, async compute, split barriers, and aliasing pitfalls. Here’s how each production engine actually implements those features, and where they diverge.

### 💾 Memory aliasing at scale

All three engines use the same core algorithm from [Part II](/posts/frame-graph-build-it/) — lifetime scanning + free-list allocation. The production differences:

<div class="diagram-ftable">
<table>
  <tr><th>Refinement</th><th>UE5 RDG</th><th>Frostbite</th><th>Unity SRP</th></tr>
  <tr><td><strong>Placed resources</strong></td><td><code>FRDGTransientResourceAllocator</code> binds into <code>ID3D12Heap</code> offsets</td><td>Heap sub-allocation from the start</td><td>Platform-dependent backend</td></tr>
  <tr><td><strong>Size bucketing</strong></td><td>Power-of-two in transient allocator</td><td>Custom bin sizes</td><td>Per-platform</td></tr>
  <tr><td><strong>Cross-frame pooling</strong></td><td>Persistent pool, peak-N-frames sizing</td><td>Aggressive pooling</td><td>Pool per render pipeline</td></tr>
  <tr><td><strong>Imported aliasing</strong></td><td><span style="color:#ef4444">✗</span> transient only</td><td><span style="color:#22c55e">✓</span> any known lifetime</td><td><span style="color:#ef4444">✗</span> transient only</td></tr>
</table>
</div>

### 🔗 Pass merging

- **Frostbite** merges automatically in the graph compiler — the original design treated it as first-class.
- **UE5 RDG** delegates to the RHI layer. Pass authors never see it; the graph itself doesn't know about subpasses.
- **Unity SRP** handles it per-platform in the backend, optimizing for mobile tile architectures.

Desktop GPUs gain almost nothing from merging. Only worth the complexity if you ship on mobile or Switch.

### ⚡ Async compute

| Engine | Approach | Discovery |
|--------|----------|-----------|
| **UE5** | Opt-in via `ERDGPassFlags::AsyncCompute` per pass | Manual — compiler trusts the flag, handles fence insertion + cross-queue sync |
| **Frostbite** | Graph compiler discovers independent subgraphs | Automatic — reachability analysis built into the compiler |
| **Unity** | Varies by platform | Limited — more conservative to maintain portability |

**Hardware reality:** NVIDIA uses separate async engines. AMD exposes more independent CUs. Some GPUs just time-slice — always profile to confirm real overlap. Vulkan requires explicit queue family ownership transfer; D3D12 uses `ID3D12Fence`. Both are expensive — only worth it if overlap wins exceed transfer cost.

### ✂️ Split barriers

Both Frostbite and UE5 support split barriers. UE5 batches them via `FRDGBarrierBatchBegin`/`FRDGBarrierBatchEnd`. Frostbite's compiler places begin/end automatically based on pass gaps.

Diminishing returns on desktop — modern drivers hide barrier latency internally. Biggest wins on mobile GPUs and expensive layout transitions (depth → shader-read). Add last, and only if profiling shows barrier stalls.

---

## 🏁 Closing

A render graph is not always the right answer. If your project has a fixed pipeline with 3–4 passes that will never change, the overhead of a graph compiler is wasted complexity. But the moment your renderer needs to *grow* — new passes, new platforms, new debug tools — the graph pays for itself in the first week.

Across these three articles, we covered the full arc: [Part I](/posts/frame-graph-theory/) laid out all the theory — the declare/compile/execute lifecycle, pass merging, async compute, and split barriers. [Part II](/posts/frame-graph-build-it/) turned the core into working C++ — automatic barriers, pass culling, and memory aliasing. And this article mapped those ideas onto what ships in UE5, Frostbite, and Unity, showing how each engine implements the same concepts at production scale.

You can now open `RenderGraphBuilder.h` in UE5 and *read* it, not reverse-engineer it. You know what `FRDGBuilder::AddPass` builds, how the transient allocator aliases memory, why `ERDGPassFlags::AsyncCompute` exists, and where the RDG boundary with legacy code still leaks.

The point isn't that every project needs a render graph. The point is that if you understand how they work, you'll make a better decision about whether *yours* does.

---

## 📚 Resources

Further reading, ordered from "start here" to deep dives.

<div class="diagram-nav">
  <div class="dn-col">
    <div class="dn-title" style="color:#22c55e">Start here</div>
    Wijiler video (15 min)<br>
    Loggini overview<br>
    GPUOpen render graphs
  </div>
  <div class="dn-col">
    <div class="dn-title" style="color:#3b82f6">Go deeper</div>
    Frostbite GDC talk<br>
    themaister blog<br>
    D3D12 barriers doc
  </div>
  <div class="dn-col">
    <div class="dn-title" style="color:#8b5cf6">Go deepest</div>
    UE5 RDG source<br>
    Vulkan sync blog<br>
    AMD RPS SDK
  </div>
</div>

**Quick visual intro (start here)** — **[Rendergraphs & High Level Rendering in Modern Graphics APIs — Wijiler (YouTube)](https://www.youtube.com/watch?v=FBYg64QKjFo)**
~15-minute video covering what render graphs are and how they fit into modern graphics APIs. Best starting point if you prefer video over text.

**Render graphs overview** — **[Render Graphs — GPUOpen](https://gpuopen.com/learn/render-graphs/)**
AMD's overview of render graph concepts and their RPS SDK. Covers declare/compile/execute, barriers, aliasing with D3D12 and Vulkan backends.

**The original talk that started it all** — **[FrameGraph: Extensible Rendering Architecture in Frostbite (GDC 2017)](https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in)**
Yuriy O'Donnell's GDC 2017 presentation — where the modern frame graph concept was introduced. If you read one thing, make it this.

**Render Graphs with D3D12 examples** — **[Render Graphs — Riccardo Loggini](https://logins.github.io/graphics/2021/05/31/RenderGraphs.html)**
Practical walkthrough with D3D12 placed resources. Covers setup/compile/execute phases with concrete code and transient memory aliasing.

**Render graphs and Vulkan — a deep dive** — **[themaister](https://themaister.net/blog/2017/08/15/render-graphs-and-vulkan-a-deep-dive/)**
Complete Vulkan render graph implementation in Granite. Covers subpass merging, barrier placement with VkEvent, async compute, and render target aliasing.

**UE5 Render Dependency Graph — official docs** — **[Render Dependency Graph in Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/render-dependency-graph-in-unreal-engine/)**
Epic's official RDG documentation. Covers `FRDGBuilder`, pass declaration, transient allocation, async compute, and RDG Insights debugging tools.

**Vulkan synchronization explained** — **[Understanding Vulkan Synchronization — Khronos Blog](https://www.khronos.org/blog/understanding-vulkan-synchronization)**
Khronos Group's guide to Vulkan sync primitives: pipeline barriers, events, semaphores, fences, and timeline semaphores.

**D3D12 resource barriers reference** — **[Using Resource Barriers — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)**
Microsoft's reference on D3D12 transition, aliasing, UAV, and split barriers. The exact API calls a D3D12 frame graph backend needs to emit.

**AMD Render Pipeline Shaders SDK (open source)** — **[RenderPipelineShaders — GitHub](https://github.com/GPUOpen-LibrariesAndSDKs/RenderPipelineShaders)**
AMD's open-source render graph framework (MIT). Automatic barriers, transient aliasing, RPSL language extension for HLSL. D3D12 + Vulkan.

---

<div style="margin:2em 0 0;padding:1em 1.2em;border-radius:10px;border:1px solid rgba(99,102,241,.2);background:rgba(99,102,241,.03);display:flex;justify-content:flex-start;">
  <a href="/posts/frame-graph-build-it/" style="text-decoration:none;font-weight:700;font-size:.95em;">
    ← Previous: Part II — Build It
  </a>
</div>
