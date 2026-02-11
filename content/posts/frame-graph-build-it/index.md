---
title: "Frame Graph — Build It"
date: 2026-02-10
draft: true
description: "Three iterations from blank file to working frame graph with automatic barriers and memory aliasing."
tags: ["rendering", "frame-graph", "gpu", "architecture", "cpp"]
categories: ["analysis"]
series: ["Rendering Architecture"]
showTableOfContents: false
---

{{< article-nav >}}

<div style="margin:0 0 1.5em;padding:.7em 1em;border-radius:8px;background:rgba(99,102,241,.04);border:1px solid rgba(99,102,241,.12);font-size:.88em;line-height:1.6;opacity:.85;">
📖 <strong>Part II of III.</strong>&ensp; <a href="/posts/frame-graph-theory/">Theory</a> → <em>Build It</em> → <a href="/posts/frame-graph-production/">Production Engines</a>
</div>

*Three iterations from blank file to working frame graph with automatic barriers and memory aliasing. Each version builds on the last — by the end you'll have something you can drop into a real renderer.*

[Part I](/posts/frame-graph-theory/) covered what a frame graph is — the three-phase lifecycle (declare → compile → execute), the DAG, and why every major engine uses one. Now we implement it. Three C++ iterations, each adding a layer: v1 is scaffolding, v2 adds the dependency graph with automatic barriers and pass culling, v3 adds lifetime analysis and memory aliasing.

---

## 🏗️ API Design

We start from the API you *want* to write — a minimal `FrameGraph` that declares a depth prepass, GBuffer pass, and lighting pass in ~20 lines of C++.

### Design principles

<div style="margin:1em 0 1.4em;display:grid;grid-template-columns:repeat(3,1fr);gap:.8em;">
  <div style="padding:1em;border-radius:10px;border-top:3px solid #3b82f6;background:rgba(59,130,246,.04);">
    <div style="font-weight:800;font-size:.92em;margin-bottom:.4em;color:#3b82f6;">λ² &ensp;Two lambdas</div>
    <div style="font-size:.86em;line-height:1.6;opacity:.85;">
      <strong>Setup</strong> — runs at declaration. Declares reads &amp; writes. No GPU work.<br>
      <strong>Execute</strong> — runs later. Records GPU commands into a fully resolved environment.
    </div>
  </div>
  <div style="padding:1em;border-radius:10px;border-top:3px solid #8b5cf6;background:rgba(139,92,246,.04);">
    <div style="font-weight:800;font-size:.92em;margin-bottom:.4em;color:#8b5cf6;">📐 &ensp;Virtual resources</div>
    <div style="font-size:.86em;line-height:1.6;opacity:.85;">
      Requested by description (<code>{1920, 1080, RGBA8}</code>), not GPU handle. Virtual until the compiler maps them to memory.
    </div>
  </div>
  <div style="padding:1em;border-radius:10px;border-top:3px solid #22c55e;background:rgba(34,197,94,.04);">
    <div style="font-weight:800;font-size:.92em;margin-bottom:.4em;color:#22c55e;">♻️ &ensp;Owned lifetimes</div>
    <div style="font-size:.86em;line-height:1.6;opacity:.85;">
      The graph owns every transient resource from first use to last. You never call create or destroy.
    </div>
  </div>
</div>

These three ideas produce a natural pipeline — declare your intent, let the compiler optimize, then execute:

<!-- Timeline: declaration → compile → execution -->
<div class="diagram-phases">
  <div class="dph-col" style="border-color:#3b82f6">
    <div class="dph-title" style="color:#3b82f6">① Declaration <span style="font-weight:400;font-size:.75em;opacity:.7;">CPU</span></div>
    <div class="dph-body">
      <code>addPass(setup, execute)</code><br>
      ├ setup lambda runs<br>
      &nbsp;&nbsp;• declare reads / writes<br>
      &nbsp;&nbsp;• request resources<br>
      └ <strong>no GPU work, no allocation</strong>
      <div style="margin-top:.6em;padding:.35em .6em;border-radius:5px;background:rgba(59,130,246,.08);font-size:.82em;line-height:1.4;border:1px solid rgba(59,130,246,.12);">
        Resources are <strong>virtual</strong> — just a description + handle index. Zero bytes allocated.
      </div>
    </div>
  </div>
  <div class="dph-col" style="border-color:#8b5cf6">
    <div class="dph-title" style="color:#8b5cf6">② Compile <span style="font-weight:400;font-size:.75em;opacity:.7;">CPU</span></div>
    <div class="dph-body">
      ├ <strong>sort</strong> — topo order (Kahn's)<br>
      ├ <strong>cull</strong> — remove dead passes<br>
      ├ <strong>alias</strong> — map virtual → physical<br>
      └ <strong>barrier</strong> — emit transitions
      <div style="margin-top:.6em;padding:.35em .6em;border-radius:5px;background:rgba(139,92,246,.08);font-size:.82em;line-height:1.4;border:1px solid rgba(139,92,246,.12);">
        Memory <strong>allocated &amp; reused</strong> here — non-overlapping lifetimes share the same heap.
      </div>
    </div>
  </div>
  <div class="dph-col" style="border-color:#22c55e">
    <div class="dph-title" style="color:#22c55e">③ Execute <span style="font-weight:400;font-size:.75em;opacity:.7;">GPU</span></div>
    <div class="dph-body">
      for each pass in sorted order:<br>
      ├ insert pre-computed barriers<br>
      └ call execute lambda<br>
      &nbsp;&nbsp;→ draw / dispatch / copy
      <div style="margin-top:.6em;padding:.35em .6em;border-radius:5px;background:rgba(34,197,94,.08);font-size:.82em;line-height:1.4;border:1px solid rgba(34,197,94,.12);">
        Lambdas see a <strong>fully resolved</strong> environment — memory bound, barriers placed, resources ready.
      </div>
    </div>
  </div>
</div>

### UE5 mapping

If you've worked with UE5's RDG, our API maps directly:

<div style="margin:.8em 0;font-size:.9em;line-height:1.8;font-family:ui-monospace,monospace;">
  <span style="opacity:.5;">ours</span> <code>addPass(setup, execute)</code> &ensp;→&ensp; <span style="opacity:.5;">UE5</span> <code>FRDGBuilder::AddPass</code><br>
  <span style="opacity:.5;">ours</span> <code>ResourceHandle</code> &ensp;→&ensp; <span style="opacity:.5;">UE5</span> <code>FRDGTextureRef</code> / <code>FRDGBufferRef</code><br>
  <span style="opacity:.5;">ours</span> <code>setup lambda</code> &ensp;→&ensp; <span style="opacity:.5;">UE5</span> <code>BEGIN_SHADER_PARAMETER_STRUCT</code><br>
  <span style="opacity:.5;">ours</span> <code>execute lambda</code> &ensp;→&ensp; <span style="opacity:.5;">UE5</span> <code>execute lambda</code> <span style="font-family:inherit;opacity:.5;">(same concept)</span>
</div>

<div style="margin:1em 0;display:grid;grid-template-columns:1fr 1fr;gap:0;border-radius:8px;overflow:hidden;border:1px solid rgba(245,158,11,.25);font-size:.85em;">
  <div style="padding:.6em .8em;background:rgba(245,158,11,.06);border-right:1px solid rgba(245,158,11,.15);border-bottom:1px solid rgba(245,158,11,.15);font-weight:700;color:#f59e0b;">UE5 macros</div>
  <div style="padding:.6em .8em;background:rgba(59,130,246,.06);border-bottom:1px solid rgba(245,158,11,.15);font-weight:700;color:#3b82f6;">Our two-lambda API</div>
  <div style="padding:.5em .8em;border-right:1px solid rgba(245,158,11,.15);line-height:1.5;"><span style="color:#22c55e">✓</span> Auto dependency extraction — can't forget a read/write</div>
  <div style="padding:.5em .8em;line-height:1.5;"><span style="color:#22c55e">✓</span> Transparent — step through in any debugger</div>
  <div style="padding:.5em .8em;border-right:1px solid rgba(245,158,11,.15);line-height:1.5;"><span style="color:#ef4444">✗</span> Harder to debug &amp; compose dynamically</div>
  <div style="padding:.5em .8em;line-height:1.5;"><span style="color:#ef4444">✗</span> Manual — you wire the edges yourself</div>
</div>

### Putting it together

Here's how the final API reads — three passes, ~20 lines:

```cpp
FrameGraph fg;
auto depth = fg.createResource({1920, 1080, Format::D32F});
auto gbufA = fg.createResource({1920, 1080, Format::RGBA8});
auto gbufN = fg.createResource({1920, 1080, Format::RGBA8});
auto hdr   = fg.createResource({1920, 1080, Format::RGBA16F});

fg.addPass("DepthPrepass",
    [&]() { fg.write(0, depth); },
    [&](/*cmd*/) { /* draw scene depth-only */ });

fg.addPass("GBuffer",
    [&]() { fg.read(1, depth); fg.write(1, gbufA); fg.write(1, gbufN); },
    [&](/*cmd*/) { /* draw scene to GBuffer MRTs */ });

fg.addPass("Lighting",
    [&]() { fg.read(2, gbufA); fg.read(2, gbufN); fg.write(2, hdr); },
    [&](/*cmd*/) { /* fullscreen lighting pass */ });

fg.execute();  // → topo-sort, cull, alias, barrier, run
```

Three passes, declared as lambdas. The graph handles the rest — ordering, barriers, memory. We build this step by step below.

---

## 🧱 MVP v1 — Declare & Execute

**Data structures:**

<div class="diagram-struct">
  <div class="dst-title">FrameGraph <span class="dst-comment">(UE5: FRDGBuilder)</span></div>
  <div class="dst-section">
    <strong>passes[]</strong> → <strong>RenderPass</strong> <span class="dst-comment">(UE5: FRDGPass)</span><br>
    &nbsp;&nbsp;• name<br>
    &nbsp;&nbsp;• setup() <span class="dst-comment">← build the DAG</span><br>
    &nbsp;&nbsp;• execute() <span class="dst-comment">← record GPU cmds</span>
  </div>
  <div class="dst-section">
    <strong>resources[]</strong> → <strong>ResourceDesc</strong> <span class="dst-comment">(UE5: FRDGTextureDesc)</span><br>
    &nbsp;&nbsp;• width, height, format<br>
    &nbsp;&nbsp;• virtual — no GPU handle yet
  </div>
  <div class="dst-section">
    <strong>ResourceHandle</strong> = index into resources[]<br>
    <span class="dst-comment">(UE5: FRDGTextureRef / FRDGBufferRef)</span>
  </div>
  <div class="dst-section">
    <span class="dst-comment">← linear allocator: all frame-scoped, free at frame end</span>
  </div>
</div>

**Flow:** Declare passes in order → execute in order. No dependency tracking yet. Resources are created eagerly.

{{< include-code file="frame_graph_v1.h" lang="cpp" compact="true" >}}
{{< include-code file="example_v1.cpp" lang="cpp" compile="true" deps="frame_graph_v1.h" compact="true" >}}

Compiles and runs — the execute lambdas are stubs, but the scaffolding is real. Every piece we add in v2 and v3 goes into this same `FrameGraph` class.

<div style="display:grid;grid-template-columns:1fr 1fr;gap:.8em;margin:1em 0;">
  <div style="padding:.7em 1em;border-radius:8px;border-left:4px solid #22c55e;background:rgba(34,197,94,.05);font-size:.9em;line-height:1.5;">
    <strong style="color:#22c55e;">✓ What it proves</strong><br>
    The lambda-based pass declaration pattern works. You can already compose passes without manual barrier calls (even though barriers are no-ops here).
  </div>
  <div style="padding:.7em 1em;border-radius:8px;border-left:4px solid #ef4444;background:rgba(239,68,68,.05);font-size:.9em;line-height:1.5;">
    <strong style="color:#ef4444;">✗ What it lacks</strong><br>
    Executes passes in declaration order, creates every resource upfront. Correct but wasteful. Version 2 adds the graph.
  </div>
</div>

---

## 🔗 MVP v2 — Dependencies & Barriers

<div style="margin:1em 0;padding:.7em 1em;border-radius:8px;border-left:4px solid #3b82f6;background:rgba(59,130,246,.04);font-size:.92em;line-height:1.6;">
🎯 <strong>Goal:</strong> Automatic pass ordering, dead-pass culling, and barrier insertion — the core value of a render graph.
</div>

Four pieces, each feeding the next:

<div style="margin:.8em 0 1.2em;display:grid;grid-template-columns:repeat(4,1fr);gap:0;border-radius:10px;overflow:hidden;border:1.5px solid rgba(99,102,241,.2);">
  <a href="#v2-versioning" style="padding:.7em .6em .5em;background:rgba(59,130,246,.05);border-right:1px solid rgba(99,102,241,.12);text-decoration:none;text-align:center;transition:background .15s;" onmouseover="this.style.background='rgba(59,130,246,.12)'" onmouseout="this.style.background='rgba(59,130,246,.05)'">
    <div style="font-size:1.2em;margin-bottom:.15em;">🔀</div>
    <div style="font-weight:800;font-size:.85em;color:#3b82f6;">Versioning</div>
    <div style="font-size:.72em;opacity:.6;margin-top:.15em;line-height:1.3;">reads/writes → edges</div>
  </a>
  <a href="#v2-toposort" style="padding:.7em .6em .5em;background:rgba(139,92,246,.05);border-right:1px solid rgba(99,102,241,.12);text-decoration:none;text-align:center;transition:background .15s;" onmouseover="this.style.background='rgba(139,92,246,.12)'" onmouseout="this.style.background='rgba(139,92,246,.05)'">
    <div style="font-size:1.2em;margin-bottom:.15em;">📦</div>
    <div style="font-weight:800;font-size:.85em;color:#8b5cf6;">Topo Sort</div>
    <div style="font-size:.72em;opacity:.6;margin-top:.15em;line-height:1.3;">edges → execution order</div>
  </a>
  <a href="#v2-culling" style="padding:.7em .6em .5em;background:rgba(245,158,11,.05);border-right:1px solid rgba(99,102,241,.12);text-decoration:none;text-align:center;transition:background .15s;" onmouseover="this.style.background='rgba(245,158,11,.12)'" onmouseout="this.style.background='rgba(245,158,11,.05)'">
    <div style="font-size:1.2em;margin-bottom:.15em;">✂️</div>
    <div style="font-weight:800;font-size:.85em;color:#f59e0b;">Pass Culling</div>
    <div style="font-size:.72em;opacity:.6;margin-top:.15em;line-height:1.3;">kill unreachable passes</div>
  </a>
  <a href="#v2-barriers" style="padding:.7em .6em .5em;background:rgba(239,68,68,.05);text-decoration:none;text-align:center;transition:background .15s;" onmouseover="this.style.background='rgba(239,68,68,.12)'" onmouseout="this.style.background='rgba(239,68,68,.05)'">
    <div style="font-size:1.2em;margin-bottom:.15em;">🚧</div>
    <div style="font-weight:800;font-size:.85em;color:#ef4444;">Barriers</div>
    <div style="font-size:.72em;opacity:.6;margin-top:.15em;line-height:1.3;">emit GPU transitions</div>
  </a>
</div>

<span id="v2-versioning"></span>

### 🔀 Resource versioning & the dependency graph

A resource can be written by pass A, read by pass B, then written *again* by pass C. To keep edges correct, each write creates a new **version** of the resource. Pass B's read depends on version 1 (A's write), not version 2 (C's write). Without versioning, the dependency graph would be ambiguous — this is the "rename on write" pattern.

<div class="diagram-version">
  <div class="dv-row">
    <span class="dv-pass">Pass A</span>
    <span class="dv-action">writes</span>
    <span class="dv-res">GBuffer v1</span>
    <span class="dv-edge">──→</span>
    <span class="dv-pass" style="background:#8b5cf6">Pass B</span>
    <span class="dv-action">reads</span>
    <span class="dv-res">GBuffer v1</span>
  </div>
  <div class="dv-row" style="margin-top:.3em">
    <span class="dv-pass">Pass C</span>
    <span class="dv-action">writes</span>
    <span class="dv-res">GBuffer v2</span>
    <span class="dv-edge">──→</span>
    <span class="dv-pass" style="background:#8b5cf6">Pass D</span>
    <span class="dv-action">reads</span>
    <span class="dv-res">GBuffer v2</span>
  </div>
  <div style="font-size:.78em;opacity:.6;margin-top:.5em;line-height:1.6">
    B depends on A (v1), D depends on C (v2).<br>
    B does <strong>NOT</strong> depend on C — versioning keeps them separate.
  </div>
</div>

Each resource version tracks who wrote it and who reads it. On write, create a new version and record the pass. On read, add a dependency edge from the writer. The dependency graph is an adjacency list — for 25 passes you'll typically have 30–50 edges.

---

<span id="v2-toposort"></span>

### � Topological sort (Kahn's algorithm)

The algorithm counts incoming edges (in-degree) for every pass. Passes with zero in-degree have no unsatisfied dependencies — they're ready to run. Step through the interactive demo to see how the queue drains:

{{< interactive-toposort >}}

Runs in O(V + E). Kahn's is preferred over DFS-based topo-sort because cycle detection falls out naturally — if the sorted output is shorter than the pass count, a cycle exists.

---

<span id="v2-culling"></span>

### ✂️ Pass culling

<div style="display:grid;grid-template-columns:auto 1fr;gap:.6em .9em;align-items:start;margin:.8em 0 1.2em;padding:.8em 1em;border-radius:10px;background:linear-gradient(135deg,rgba(245,158,11,.06),transparent);border:1px solid rgba(245,158,11,.18);font-size:.9em;line-height:1.6;">
  <span style="font-size:1.3em;line-height:1;">🔙</span>
  <span><strong>Algorithm:</strong> Walk backwards from the final output (present / backbuffer). Mark every reachable pass as <em>alive</em>.</span>
  <span style="font-size:1.3em;line-height:1;">💀</span>
  <span><strong>Result:</strong> Any unmarked pass is dead — removed along with all its resource declarations. No <code>#ifdef</code>, no flag.</span>
  <span style="font-size:1.3em;line-height:1;">⏱️</span>
  <span><strong>Cost:</strong> O(V + E) — one linear walk over the graph.</span>
</div>

Disable edges in the interactive DAG, then compile both C++ variants below to see culling happen for real:

{{< interactive-dag >}}

{{< compile-compare fileA="example_v2_ssao_alive.cpp" fileB="example_v2_ssao_dead.cpp" labelA="SSAO Connected (alive)" labelB="SSAO Disconnected (culled)" deps="frame_graph_v2.h" >}}

---

<span id="v2-barriers"></span>

### 🚧 Barrier insertion

Walk the sorted order. For each pass, check each resource against a state table tracking its current pipeline stage, access flags, and image layout. If usage changed, emit a barrier. Every one of these is a barrier your graph inserts automatically:

<div class="barrier-zoo-grid">
  <div class="bz-header">Barrier zoo — the transitions a real frame actually needs</div>
  <div class="bz-cards">
    <div class="bz-card">
      <div class="bz-card-head"><span class="bz-num">1</span> Render Target → Shader Read</div>
      <div class="bz-desc">GBuffer writes albedo → Lighting samples it</div>
      <div class="bz-tag bz-common">most common</div>
      <div class="bz-api"><span class="bz-vk">VK</span> COLOR_ATTACHMENT_OUTPUT → FRAGMENT_SHADER</div>
      <div class="bz-api"><span class="bz-dx">DX</span> RENDER_TARGET → PIXEL_SHADER_RESOURCE</div>
    </div>
    <div class="bz-card">
      <div class="bz-card-head"><span class="bz-num">2</span> Depth Write → Depth Read</div>
      <div class="bz-desc">Shadow pass writes depth → Lighting reads as texture</div>
      <div class="bz-tag bz-shadow">shadow sampling</div>
      <div class="bz-api"><span class="bz-vk">VK</span> LATE_FRAGMENT_TESTS → FRAGMENT_SHADER</div>
      <div class="bz-api"><span class="bz-dx">DX</span> DEPTH_WRITE → PIXEL_SHADER_RESOURCE</div>
    </div>
    <div class="bz-card">
      <div class="bz-card-head"><span class="bz-num">3</span> UAV Write → UAV Read</div>
      <div class="bz-desc">Bloom downsample mip N → reads it for mip N+1</div>
      <div class="bz-tag bz-compute">compute ping-pong</div>
      <div class="bz-api"><span class="bz-vk">VK</span> COMPUTE_SHADER (W) → COMPUTE_SHADER (R)</div>
      <div class="bz-api"><span class="bz-dx">DX</span> UAV barrier (flush compute caches)</div>
    </div>
    <div class="bz-card">
      <div class="bz-card-head"><span class="bz-num">4</span> Shader Read → Render Target</div>
      <div class="bz-desc">Lighting sampled HDR → Tonemap writes to it</div>
      <div class="bz-tag bz-reuse">resource reuse</div>
      <div class="bz-api"><span class="bz-vk">VK</span> FRAGMENT_SHADER → COLOR_ATTACHMENT_OUTPUT</div>
      <div class="bz-api"><span class="bz-dx">DX</span> PIXEL_SHADER_RESOURCE → RENDER_TARGET</div>
    </div>
    <div class="bz-card">
      <div class="bz-card-head"><span class="bz-num">5</span> Render Target → Present</div>
      <div class="bz-desc">Final composite → swapchain present</div>
      <div class="bz-tag bz-every">every frame</div>
      <div class="bz-api"><span class="bz-vk">VK</span> COLOR_ATTACHMENT_OUTPUT → BOTTOM_OF_PIPE</div>
      <div class="bz-api"><span class="bz-dx">DX</span> RENDER_TARGET → PRESENT</div>
    </div>
    <div class="bz-card">
      <div class="bz-card-head"><span class="bz-num">6</span> Aliasing Barrier</div>
      <div class="bz-desc">GBuffer dies → HDR reuses same physical memory</div>
      <div class="bz-tag bz-alias">memory aliasing</div>
      <div class="bz-api"><span class="bz-dx">DX</span> RESOURCE_BARRIER_TYPE_ALIASING</div>
      <div class="bz-api"><span class="bz-vk">VK</span> image layout UNDEFINED (discard)</div>
    </div>
  </div>
</div>

{{< interactive-barriers >}}

A 25-pass frame needs 30–50 of these. Miss one: corruption or device lost. Add a redundant one: GPU stall for nothing. The graph sees every read/write edge and emits the *exact* set.

---

### Putting it together — v1 → v2 diff

We need four new pieces: (1) resource versioning with read/write tracking, (2) adjacency list for the DAG, (3) topological sort, (4) pass culling, and (5) barrier insertion. Additions marked with `// NEW v2` in the source:

{{< code-diff title="v1 → v2 — Key structural changes" >}}
@@ RenderPass struct @@
 struct RenderPass {
     std::string name;
     std::function<void()>             setup;
     std::function<void(/*cmd list*/)> execute;
+    std::vector<ResourceHandle> reads;     // NEW v2
+    std::vector<ResourceHandle> writes;    // NEW v2
+    std::vector<uint32_t> dependsOn;       // NEW v2
+    std::vector<uint32_t> successors;      // NEW v2
+    uint32_t inDegree = 0;                 // NEW v2
+    bool     alive    = false;             // NEW v2
 };

@@ FrameGraph class — new methods @@
+    void read(uint32_t passIdx, ResourceHandle h);  // link to resource version
+    void write(uint32_t passIdx, ResourceHandle h);  // create new version

@@ FrameGraph::execute() @@
-    // v1: just run every pass in declaration order.
-    for (auto& pass : passes_)
-        pass.execute();
+    // v2: build edges, topo-sort, cull, then run in sorted order.
+    buildEdges();
+    auto sorted = topoSort();   // Kahn's algorithm — O(V+E)
+    cull(sorted);               // backward walk from output
+    for (uint32_t idx : sorted) {
+        if (!passes_[idx].alive) continue;  // skip dead
+        insertBarriers(idx);                // auto barriers
+        passes_[idx].execute();
+    }

@@ New internal data @@
-    std::vector<ResourceDesc>  resources_;
+    std::vector<ResourceEntry> entries_;  // now with versioning
{{< /code-diff >}}

Full updated source:

{{< include-code file="frame_graph_v2.h" lang="cpp" compact="true" >}}
{{< include-code file="example_v2.cpp" lang="cpp" compile="true" deps="frame_graph_v2.h" compact="true" >}}

That's three of the four intro promises delivered — automatic ordering, barrier insertion, and dead-pass culling. The only piece missing: resources still live for the entire frame. Version 3 fixes that with lifetime analysis and memory aliasing.

UE5's RDG does the same thing. When you call `FRDGBuilder::AddPass`, RDG builds the dependency graph from your declared reads/writes, topologically sorts it, culls dead passes, and inserts barriers — all before recording a single GPU command. The migration is incomplete, though — large parts of UE5's renderer still use legacy `FRHICommandList` calls outside the graph, requiring manual barriers at the RDG boundary. More on that in [Part III](/posts/frame-graph-production/).

---

## 💾 MVP v3 — Lifetimes & Aliasing

V2 gives us ordering, culling, and barriers — but every transient resource lives for the entire frame. A 1080p deferred pipeline allocates ~52 MB of transient textures that are each used for only 2–3 passes. If their lifetimes don't overlap, they can share physical memory. That's aliasing, and it typically saves 30–50% VRAM.

The algorithm has three steps. First, **scan lifetimes**: walk the sorted pass list and record each transient resource's `firstUsePass` and `lastUsePass` (imported resources are excluded — they're externally owned). Second, **track refcounts**: increment at first use, decrement at last use; when a resource's refcount hits zero, its physical memory becomes available. Third, **free-list scan**: sort resources by first-use, then greedily try to fit each one into an existing physical block that's compatible (same memory type, large enough, available after the previous user finished). Fit → reuse. No fit → allocate a new block. This is greedy interval-coloring — the same approach Frostbite described at GDC 2017.

Here's a concrete example. Six transient resources in a 1080p deferred pipeline:

<div style="overflow-x:auto;margin:1em 0;">
<table style="width:100%;border-collapse:collapse;font-size:.88em;border-radius:8px;overflow:hidden;">
  <thead>
    <tr style="background:rgba(139,92,246,.1);">
      <th style="padding:.5em .8em;text-align:left;font-weight:700;border-bottom:2px solid rgba(139,92,246,.2);">Virtual Resource</th>
      <th style="padding:.5em .8em;text-align:center;font-weight:700;border-bottom:2px solid rgba(139,92,246,.2);">Format</th>
      <th style="padding:.5em .8em;text-align:center;font-weight:700;border-bottom:2px solid rgba(139,92,246,.2);">Size</th>
      <th style="padding:.5em .8em;text-align:center;font-weight:700;border-bottom:2px solid rgba(139,92,246,.2);">Lifetime</th>
      <th style="padding:.5em .8em;text-align:center;font-weight:700;border-bottom:2px solid rgba(139,92,246,.2);">Shares with</th>
    </tr>
  </thead>
  <tbody>
    <tr style="background:rgba(59,130,246,.06);">
      <td style="padding:.4em .8em;border-bottom:1px solid rgba(127,127,127,.1);">GBuffer Albedo</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;border-bottom:1px solid rgba(127,127,127,.1);">RGBA8</td>
      <td style="padding:.4em .8em;text-align:center;border-bottom:1px solid rgba(127,127,127,.1);">8 MB</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;border-bottom:1px solid rgba(127,127,127,.1);">P2–P4</td>
      <td style="padding:.4em .8em;text-align:center;color:#3b82f6;font-weight:600;border-bottom:1px solid rgba(127,127,127,.1);">HDR Lighting ↓</td>
    </tr>
    <tr style="background:rgba(139,92,246,.06);">
      <td style="padding:.4em .8em;border-bottom:1px solid rgba(127,127,127,.1);">GBuffer Normals</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;border-bottom:1px solid rgba(127,127,127,.1);">RGB10A2</td>
      <td style="padding:.4em .8em;text-align:center;border-bottom:1px solid rgba(127,127,127,.1);">8 MB</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;border-bottom:1px solid rgba(127,127,127,.1);">P2–P4</td>
      <td style="padding:.4em .8em;text-align:center;color:#8b5cf6;font-weight:600;border-bottom:1px solid rgba(127,127,127,.1);">Bloom Scratch ↓</td>
    </tr>
    <tr style="background:rgba(34,197,94,.06);">
      <td style="padding:.4em .8em;border-bottom:1px solid rgba(127,127,127,.1);">SSAO Scratch</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;border-bottom:1px solid rgba(127,127,127,.1);">R8</td>
      <td style="padding:.4em .8em;text-align:center;border-bottom:1px solid rgba(127,127,127,.1);">2 MB</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;border-bottom:1px solid rgba(127,127,127,.1);">P3–P4</td>
      <td style="padding:.4em .8em;text-align:center;color:#22c55e;font-weight:600;border-bottom:1px solid rgba(127,127,127,.1);">SSAO Result ↓</td>
    </tr>
    <tr style="background:rgba(34,197,94,.06);">
      <td style="padding:.4em .8em;border-bottom:1px solid rgba(127,127,127,.1);">SSAO Result</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;border-bottom:1px solid rgba(127,127,127,.1);">R8</td>
      <td style="padding:.4em .8em;text-align:center;border-bottom:1px solid rgba(127,127,127,.1);">2 MB</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;border-bottom:1px solid rgba(127,127,127,.1);">P4–P5</td>
      <td style="padding:.4em .8em;text-align:center;color:#22c55e;font-weight:600;border-bottom:1px solid rgba(127,127,127,.1);">SSAO Scratch ↑</td>
    </tr>
    <tr style="background:rgba(59,130,246,.06);">
      <td style="padding:.4em .8em;border-bottom:1px solid rgba(127,127,127,.1);">HDR Lighting</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;border-bottom:1px solid rgba(127,127,127,.1);">RGBA16F</td>
      <td style="padding:.4em .8em;text-align:center;border-bottom:1px solid rgba(127,127,127,.1);">16 MB</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;border-bottom:1px solid rgba(127,127,127,.1);">P5–P6</td>
      <td style="padding:.4em .8em;text-align:center;color:#3b82f6;font-weight:600;border-bottom:1px solid rgba(127,127,127,.1);">GBuffer Albedo ↑</td>
    </tr>
    <tr style="background:rgba(139,92,246,.06);">
      <td style="padding:.4em .8em;">Bloom Scratch</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;">RGBA16F</td>
      <td style="padding:.4em .8em;text-align:center;">16 MB</td>
      <td style="padding:.4em .8em;text-align:center;font-family:monospace;">P6–P7</td>
      <td style="padding:.4em .8em;text-align:center;color:#8b5cf6;font-weight:600;">GBuffer Normals ↑</td>
    </tr>
  </tbody>
</table>
</div>

<div style="display:flex;align-items:center;gap:1em;margin:1em 0;padding:.6em 1em;border-radius:8px;background:linear-gradient(90deg,rgba(239,68,68,.06),rgba(34,197,94,.06));">
  <div style="text-align:center;line-height:1.3;">
    <div style="font-size:.75em;opacity:.6;text-transform:uppercase;letter-spacing:.05em;">Without aliasing</div>
    <div style="font-size:1.4em;font-weight:800;color:#ef4444;">52 MB</div>
  </div>
  <div style="font-size:1.5em;opacity:.3;">→</div>
  <div style="text-align:center;line-height:1.3;">
    <div style="font-size:.75em;opacity:.6;text-transform:uppercase;letter-spacing:.05em;">With aliasing</div>
    <div style="font-size:1.4em;font-weight:800;color:#22c55e;">36 MB</div>
  </div>
  <div style="margin-left:auto;font-size:.85em;line-height:1.4;opacity:.8;">
    3 physical blocks shared across 6 virtual resources.<br>
    <strong style="color:#22c55e;">31% saved</strong> — in complex pipelines: 40–50%.
  </div>
</div>

This requires **placed resources** at the API level — GPU memory allocated from a heap, with resources bound to offsets within it. In D3D12, that means `ID3D12Heap` + `CreatePlacedResource`. In Vulkan, `VkDeviceMemory` + `vkBindImageMemory` at different offsets. Without placed resources (i.e., `CreateCommittedResource` or Vulkan dedicated allocations), each resource gets its own memory and aliasing is impossible — which is why the graph's allocator works with heaps.

Drag the interactive timeline below to see how resources share physical blocks as their lifetimes end:

{{< interactive-aliasing >}}

---

### Putting it together — v2 → v3 diff

Two additions to the `FrameGraph` class: (1) a lifetime scan that records each transient resource's first and last use in the sorted pass order, and (2) a greedy free-list allocator that reuses physical blocks when lifetimes don't overlap.

{{< code-diff title="v2 → v3 — Key additions for lifetime analysis & aliasing" >}}
@@ New structs @@
+struct PhysicalBlock {              // physical memory slot
+    uint32_t sizeBytes  = 0;
+    Format   format     = Format::RGBA8;
+    uint32_t availAfter = 0;        // free after this pass
+};
+
+struct Lifetime {                   // per-resource timing
+    uint32_t firstUse = UINT32_MAX;
+    uint32_t lastUse  = 0;
+    bool     isTransient = true;
+};

@@ FrameGraph::execute() @@
     auto sorted = topoSort();
     cull(sorted);
+    auto lifetimes = scanLifetimes(sorted);     // NEW v3
+    auto mapping   = aliasResources(lifetimes); // NEW v3
     // ... existing barrier + execute loop ...

@@ scanLifetimes() — walk sorted passes, record first/last use @@
+    for (uint32_t order = 0; order < sorted.size(); order++) {
+        for (auto& h : passes_[sorted[order]].reads) {
+            life[h.index].firstUse = min(life[h.index].firstUse, order);
+            life[h.index].lastUse  = max(life[h.index].lastUse,  order);
+        }
+        // ... same for writes ...
+    }

@@ aliasResources() — greedy free-list scan @@
+    // sort resources by firstUse, then scan free list:
+    for (uint32_t resIdx : indices) {
+        for (uint32_t b = 0; b < freeList.size(); b++) {
+            if (freeList[b].availAfter < firstUse && sizeOK) {
+                mapping[resIdx] = b;  // reuse!
+                break;
+            }
+        }
+        if (!reused) freeList.push_back(newBlock); // allocate
+    }
{{< /code-diff >}}

Complete v3 source — all v2 code plus lifetime analysis and aliasing:

{{< include-code file="frame_graph_v3.h" lang="cpp" compact="true" >}}
{{< include-code file="example_v3.cpp" lang="cpp" compile="true" deps="frame_graph_v3.h" compact="true" >}}

~70 new lines on top of v2. Aliasing runs once per frame in O(R log R) — sort, then linear scan of the free list. Sub-microsecond for 15 transient resources.

That's the full value prop — automatic memory aliasing *and* automatic barriers from a single `FrameGraph` class. Feature-equivalent to Frostbite's 2017 GDC demo (minus async compute). UE5's transient resource allocator does the same thing: any `FRDGTexture` created through `FRDGBuilder::CreateTexture` (vs `RegisterExternalTexture`) is transient and eligible for aliasing, using the same lifetime analysis and free-list scan we just built. One difference: UE5 only aliases transient resources — imported resources are never aliased, even with fully known lifetimes. Frostbite was more aggressive here.

Still missing from our implementation: async compute, split barriers, pass merging, and parallel recording. These are production features — covered in [Part III](/posts/frame-graph-production/). But first — let's see what the completed MVP actually does with two real pipeline topologies.

---

## 🖥️ A Real Frame

**Deferred Pipeline**

Depth prepass → GBuffer → SSAO → Lighting → Tonemap → Present

<div class="diagram-flow" style="justify-content:center;flex-wrap:wrap">
  <div class="df-step df-primary">Depth<span class="df-sub">depth (T)</span></div>
  <div class="df-arrow"></div>
  <div class="df-step df-primary">GBuf<span class="df-sub">albedo (T) · norm (T)</span></div>
  <div class="df-arrow"></div>
  <div class="df-step df-primary">SSAO<span class="df-sub">scratch (T) · result (T)</span></div>
  <div class="df-arrow"></div>
  <div class="df-step df-primary">Lighting<span class="df-sub">HDR (T)</span></div>
  <div class="df-arrow"></div>
  <div class="df-step">Tonemap</div>
  <div class="df-arrow"></div>
  <div class="df-step df-success">Present<span class="df-sub">backbuffer (imported)</span></div>
</div>
<div style="text-align:center;font-size:.75em;opacity:.5;margin-top:-.3em">(T) = transient — aliased by graph &nbsp;&nbsp;&nbsp; (imported) = owned externally</div>

Everything marked (T) is transient — the graph owns its memory and aliases it. The backbuffer is imported — the graph tracks its barriers but doesn't own its memory. Same distinction we covered in [Part I](/posts/frame-graph-theory/).

**Forward Pipeline**

<div class="diagram-flow" style="justify-content:center;flex-wrap:wrap">
  <div class="df-step df-primary">Depth<span class="df-sub">depth (T)</span></div>
  <div class="df-arrow"></div>
  <div class="df-step df-primary">Forward + MSAA<span class="df-sub">color MSAA (T)</span></div>
  <div class="df-arrow"></div>
  <div class="df-step df-primary">Resolve<span class="df-sub">color (T)</span></div>
  <div class="df-arrow"></div>
  <div class="df-step df-primary">PostProc<span class="df-sub">HDR (T)</span></div>
  <div class="df-arrow"></div>
  <div class="df-step df-success">Present<span class="df-sub">backbuffer (imported)</span></div>
</div>
<div style="text-align:center;font-size:.75em;opacity:.5;margin-top:-.3em">Fewer passes, fewer transient resources → less aliasing opportunity. Same API, same automatic barriers.</div>

**Side-by-side**

<div style="overflow-x:auto;margin:1em 0">
<table style="width:100%;border-collapse:collapse;border-radius:10px;overflow:hidden;font-size:.92em">
  <thead>
    <tr style="background:linear-gradient(135deg,rgba(59,130,246,.12),rgba(139,92,246,.1))">
      <th style="padding:.7em 1em;text-align:left;border-bottom:2px solid rgba(59,130,246,.2)">Aspect</th>
      <th style="padding:.7em 1em;text-align:center;border-bottom:2px solid rgba(59,130,246,.2);color:#3b82f6">Deferred</th>
      <th style="padding:.7em 1em;text-align:center;border-bottom:2px solid rgba(139,92,246,.2);color:#8b5cf6">Forward</th>
    </tr>
  </thead>
  <tbody>
    <tr><td style="padding:.5em 1em">Passes</td><td style="padding:.5em 1em;text-align:center">6</td><td style="padding:.5em 1em;text-align:center">5</td></tr>
    <tr style="background:rgba(127,127,127,.04)"><td style="padding:.5em 1em">Peak VRAM (no aliasing)</td><td style="padding:.5em 1em;text-align:center">X MB</td><td style="padding:.5em 1em;text-align:center">Y MB</td></tr>
    <tr><td style="padding:.5em 1em">Peak VRAM (with aliasing)</td><td style="padding:.5em 1em;text-align:center">0.6X MB</td><td style="padding:.5em 1em;text-align:center">0.75Y MB</td></tr>
    <tr style="background:linear-gradient(90deg,rgba(34,197,94,.08),rgba(34,197,94,.04))"><td style="padding:.5em 1em;font-weight:700">VRAM saved by aliasing</td><td style="padding:.5em 1em;text-align:center;font-weight:700;color:#22c55e;font-size:1.1em">40%</td><td style="padding:.5em 1em;text-align:center;font-weight:700;color:#22c55e;font-size:1.1em">25%</td></tr>
    <tr><td style="padding:.5em 1em">Barriers auto-inserted</td><td style="padding:.5em 1em;text-align:center">8</td><td style="padding:.5em 1em;text-align:center">5</td></tr>
  </tbody>
</table>
</div>

**What about CPU cost?** Every phase is linear-time:

<div style="overflow-x:auto;margin:1em 0">
<table style="width:100%;border-collapse:collapse;font-size:.9em">
  <thead>
    <tr>
      <th style="padding:.6em 1em;text-align:left;border-bottom:2px solid rgba(34,197,94,.3);color:#22c55e">Phase</th>
      <th style="padding:.6em 1em;text-align:center;border-bottom:2px solid rgba(34,197,94,.3)">Complexity</th>
      <th style="padding:.6em 1em;text-align:left;border-bottom:2px solid rgba(34,197,94,.3)">Notes</th>
    </tr>
  </thead>
  <tbody>
    <tr><td style="padding:.45em 1em;font-weight:600">Topological sort</td><td style="padding:.45em 1em;text-align:center;font-family:ui-monospace,monospace;color:#22c55e">O(V + E)</td><td style="padding:.45em 1em;font-size:.9em;opacity:.8">Kahn's algorithm — passes + edges</td></tr>
    <tr style="background:rgba(127,127,127,.04)"><td style="padding:.45em 1em;font-weight:600">Pass culling</td><td style="padding:.45em 1em;text-align:center;font-family:ui-monospace,monospace;color:#22c55e">O(V + E)</td><td style="padding:.45em 1em;font-size:.9em;opacity:.8">Backward reachability from output</td></tr>
    <tr><td style="padding:.45em 1em;font-weight:600">Lifetime scan</td><td style="padding:.45em 1em;text-align:center;font-family:ui-monospace,monospace;color:#22c55e">O(V)</td><td style="padding:.45em 1em;font-size:.9em;opacity:.8">Single pass over sorted list</td></tr>
    <tr style="background:rgba(127,127,127,.04)"><td style="padding:.45em 1em;font-weight:600">Aliasing</td><td style="padding:.45em 1em;text-align:center;font-family:ui-monospace,monospace;color:#22c55e">O(R log R)</td><td style="padding:.45em 1em;font-size:.9em;opacity:.8">Sort by first-use, then O(R) free-list scan</td></tr>
    <tr><td style="padding:.45em 1em;font-weight:600">Barrier insertion</td><td style="padding:.45em 1em;text-align:center;font-family:ui-monospace,monospace;color:#22c55e">O(V)</td><td style="padding:.45em 1em;font-size:.9em;opacity:.8">Linear scan with state lookup</td></tr>
  </tbody>
</table>
</div>

<div style="font-size:.88em;line-height:1.5;opacity:.75;margin:-.3em 0 1em 0">Where V = passes (~25), E = dependency edges (~50), R = transient resources (~15). Everything is linear or near-linear. All data fits in L1 cache — the entire compile is well under 0.1 ms.</div>

The graph doesn't care about your rendering *strategy*. It cares about your *dependencies*. Deferred or forward, the same `FrameGraph` class handles both — different topology, same automatic barriers and aliasing. That's the whole point.

---

<div style="margin:2em 0 0;padding:1em 1.2em;border-radius:10px;border:1px solid rgba(99,102,241,.2);background:rgba(99,102,241,.03);display:flex;justify-content:space-between;align-items:center;">
  <a href="/posts/frame-graph-theory/" style="text-decoration:none;font-weight:700;font-size:.95em;">
    ← Previous: Part I — Theory
  </a>
  <a href="/posts/frame-graph-production/" style="text-decoration:none;font-weight:700;font-size:.95em;">
    Next: Part III — Production Engines →
  </a>
</div>
