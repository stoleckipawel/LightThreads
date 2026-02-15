---
title: "Frame Graph — Build It"
date: 2026-02-10
draft: false
description: "Three iterations from blank file to working frame graph with automatic barriers and memory aliasing."
tags: ["rendering", "frame-graph", "gpu", "architecture", "cpp"]
categories: ["analysis"]
series: ["Rendering Architecture"]
showTableOfContents: false
---

{{< article-nav >}}

<div style="margin:0 0 1.5em;padding:.7em 1em;border-radius:8px;background:rgba(var(--ds-indigo-rgb),.04);border:1px solid rgba(var(--ds-indigo-rgb),.12);font-size:.88em;line-height:1.6;opacity:.85;">
📖 <strong>Part II of III.</strong>&ensp; <a href="../frame-graph-theory/">Theory</a> → <em>Build It</em> → <a href="../frame-graph-production/">Production Engines</a>
</div>

*Three iterations from blank file to working frame graph with automatic barriers and memory aliasing. Each version builds on the last — by the end you'll have something you can drop into a real renderer.*

[Part I](/posts/frame-graph-theory/) covered what a frame graph is — the three-phase lifecycle (declare → compile → execute), the DAG, and why every major engine uses one. Now we implement it. Three C++ iterations, each adding a layer: v1 is scaffolding, v2 adds the dependency graph with automatic barriers and pass culling, v3 adds lifetime analysis and memory aliasing.

---

## 🏗️ API Design

We start from the API you *want* to write — a minimal `FrameGraph` that declares a depth prepass, GBuffer pass, and lighting pass in ~20 lines of C++.

### 🎯 Design principles

<div style="margin:1em 0 1.4em;display:grid;grid-template-columns:repeat(3,1fr);gap:.8em;">
  <div style="padding:1em;border-radius:10px;border-top:3px solid var(--ds-info);background:rgba(var(--ds-info-rgb),.04);">
    <div style="font-weight:800;font-size:.92em;margin-bottom:.4em;color:var(--ds-info);">λ² &ensp;Two lambdas</div>
    <div style="font-size:.86em;line-height:1.6;opacity:.85;">
      <strong>Setup</strong> — runs at declaration. Declares reads &amp; writes. No GPU work.<br>
      <strong>Execute</strong> — runs later. Records GPU commands into a fully resolved environment.
    </div>
  </div>
  <div style="padding:1em;border-radius:10px;border-top:3px solid var(--ds-code);background:rgba(var(--ds-code-rgb),.04);">
    <div style="font-weight:800;font-size:.92em;margin-bottom:.4em;color:var(--ds-code);">📐 &ensp;Virtual resources</div>
    <div style="font-size:.86em;line-height:1.6;opacity:.85;">
      Requested by description (<code>{1920, 1080, RGBA8}</code>), not GPU handle. Virtual until the compiler maps them to memory.
    </div>
  </div>
  <div style="padding:1em;border-radius:10px;border-top:3px solid var(--ds-success);background:rgba(var(--ds-success-rgb),.04);">
    <div style="font-weight:800;font-size:.92em;margin-bottom:.4em;color:var(--ds-success);">♻️ &ensp;Owned lifetimes</div>
    <div style="font-size:.86em;line-height:1.6;opacity:.85;">
      The graph owns every transient resource from first use to last. You never call create or destroy.
    </div>
  </div>
</div>

These three ideas produce a natural pipeline — declare your intent, let the compiler optimize, then execute:

<!-- Timeline: declaration → compile → execution -->
<div class="diagram-phases">
  <div class="dph-col" style="border-color:var(--ds-info)">
    <div class="dph-title" style="color:var(--ds-info)">① Declaration <span style="font-weight:400;font-size:.75em;opacity:.7;">CPU</span></div>
    <div class="dph-body">
      <code>addPass(setup, execute)</code><br>
      ├ setup lambda runs<br>
      &nbsp;&nbsp;• declare reads / writes<br>
      &nbsp;&nbsp;• request resources<br>
      └ <strong>no GPU work, no allocation</strong>
      <div style="margin-top:.6em;padding:.35em .6em;border-radius:5px;background:rgba(var(--ds-info-rgb),.08);font-size:.82em;line-height:1.4;border:1px solid rgba(var(--ds-info-rgb),.12);">
        Resources are <strong>virtual</strong> — just a description + handle index. Zero bytes allocated.
      </div>
    </div>
  </div>
  <div class="dph-col" style="border-color:var(--ds-code)">
    <div class="dph-title" style="color:var(--ds-code)">② Compile <span style="font-weight:400;font-size:.75em;opacity:.7;">CPU</span></div>
    <div class="dph-body">
      ├ <strong>sort</strong> — topo order (Kahn's)<br>
      ├ <strong>cull</strong> — remove dead passes<br>
      ├ <strong>alias</strong> — map virtual → physical<br>
      └ <strong>barrier</strong> — emit transitions
      <div style="margin-top:.6em;padding:.35em .6em;border-radius:5px;background:rgba(var(--ds-code-rgb),.08);font-size:.82em;line-height:1.4;border:1px solid rgba(var(--ds-code-rgb),.12);">
        Aliasing and allocation <strong>happen</strong> here — non-overlapping lifetimes share the same heap, physical memory is bound before execute.
      </div>
    </div>
  </div>
  <div class="dph-col" style="border-color:var(--ds-success)">
    <div class="dph-title" style="color:var(--ds-success)">③ Execute <span style="font-weight:400;font-size:.75em;opacity:.7;">GPU</span></div>
    <div class="dph-body">
      for each pass in sorted order:<br>
      ├ insert automatic barriers<br>
      └ call execute lambda<br>
      &nbsp;&nbsp;→ draw / dispatch / copy
      <div style="margin-top:.6em;padding:.35em .6em;border-radius:5px;background:rgba(var(--ds-success-rgb),.08);font-size:.82em;line-height:1.4;border:1px solid rgba(var(--ds-success-rgb),.12);">
        Lambdas see a <strong>fully resolved</strong> environment — memory bound, barriers placed, resources ready.
      </div>
    </div>
  </div>
</div>

### 🧩 Putting it together

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
  <div style="padding:.7em 1em;border-radius:8px;border-left:4px solid var(--ds-success);background:rgba(var(--ds-success-rgb),.05);font-size:.9em;line-height:1.5;">
    <strong style="color:var(--ds-success);">✓ What it proves</strong><br>
    The lambda-based pass declaration pattern works. You can already compose passes without manual barrier calls (even though barriers are no-ops here).
  </div>
  <div style="padding:.7em 1em;border-radius:8px;border-left:4px solid var(--ds-danger);background:rgba(var(--ds-danger-rgb),.05);font-size:.9em;line-height:1.5;">
    <strong style="color:var(--ds-danger);">✗ What it lacks</strong><br>
    Executes passes in declaration order, creates every resource upfront. Correct but wasteful. Version 2 adds the graph.
  </div>
</div>

---

## 🔗 MVP v2 — Dependencies & Barriers

<div style="margin:1em 0;padding:.7em 1em;border-radius:8px;border-left:4px solid var(--ds-info);background:rgba(var(--ds-info-rgb),.04);font-size:.92em;line-height:1.6;">
🎯 <strong>Goal:</strong> Automatic pass ordering, dead-pass culling, and barrier insertion — the core value of a render graph.
</div>

Four pieces, each feeding the next:

<div style="margin:.8em 0 1.2em;display:grid;grid-template-columns:repeat(4,1fr);gap:0;border-radius:10px;overflow:hidden;border:1.5px solid rgba(var(--ds-indigo-rgb),.2);">
  <a href="#v2-versioning" style="padding:.7em .6em .5em;background:rgba(var(--ds-info-rgb),.05);border-right:1px solid rgba(var(--ds-indigo-rgb),.12);text-decoration:none;text-align:center;transition:background .15s;" onmouseover="this.style.background='rgba(var(--ds-info-rgb),.12)'" onmouseout="this.style.background='rgba(var(--ds-info-rgb),.05)'">
    <div style="font-size:1.2em;margin-bottom:.15em;">🔀</div>
    <div style="font-weight:800;font-size:.85em;color:var(--ds-info);">Versioning</div>
    <div style="font-size:.72em;opacity:.6;margin-top:.15em;line-height:1.3;">reads/writes → edges</div>
  </a>
  <a href="#v2-toposort" style="padding:.7em .6em .5em;background:rgba(var(--ds-code-rgb),.05);border-right:1px solid rgba(var(--ds-indigo-rgb),.12);text-decoration:none;text-align:center;transition:background .15s;" onmouseover="this.style.background='rgba(var(--ds-code-rgb),.12)'" onmouseout="this.style.background='rgba(var(--ds-code-rgb),.05)'">
    <div style="font-size:1.2em;margin-bottom:.15em;">📦</div>
    <div style="font-weight:800;font-size:.85em;color:var(--ds-code);">Topo Sort</div>
    <div style="font-size:.72em;opacity:.6;margin-top:.15em;line-height:1.3;">edges → execution order</div>
  </a>
  <a href="#v2-culling" style="padding:.7em .6em .5em;background:rgba(var(--ds-warn-rgb),.05);border-right:1px solid rgba(var(--ds-indigo-rgb),.12);text-decoration:none;text-align:center;transition:background .15s;" onmouseover="this.style.background='rgba(var(--ds-warn-rgb),.12)'" onmouseout="this.style.background='rgba(var(--ds-warn-rgb),.05)'">
    <div style="font-size:1.2em;margin-bottom:.15em;">✂️</div>
    <div style="font-weight:800;font-size:.85em;color:var(--ds-warn);">Pass Culling</div>
    <div style="font-size:.72em;opacity:.6;margin-top:.15em;line-height:1.3;">kill unreachable passes</div>
  </a>
  <a href="#v2-barriers" style="padding:.7em .6em .5em;background:rgba(var(--ds-danger-rgb),.05);text-decoration:none;text-align:center;transition:background .15s;" onmouseover="this.style.background='rgba(var(--ds-danger-rgb),.12)'" onmouseout="this.style.background='rgba(var(--ds-danger-rgb),.05)'">
    <div style="font-size:1.2em;margin-bottom:.15em;">🚧</div>
    <div style="font-weight:800;font-size:.85em;color:var(--ds-danger);">Barriers</div>
    <div style="font-size:.72em;opacity:.6;margin-top:.15em;line-height:1.3;">emit GPU transitions</div>
  </a>
</div>

<span id="v2-versioning"></span>

### 🔀 Resource versioning — the data structure

[Part I](/posts/frame-graph-theory/#how-edges-form--resource-versioning) introduced resource versioning — each write bumps a version number, readers attach to the current version, and that's what creates precise dependency edges. Here we implement it.

The key data structure: each resource entry tracks its **current version** (incremented on write) and a **writer pass index** per version. When a pass calls `read(h)`, the graph looks up the current version's writer and adds a dependency edge from that writer to the reading pass.

Here's what changes from v1. The `ResourceDesc` array becomes `ResourceEntry` — each entry carries a version list. `RenderPass` gains dependency tracking fields. And two new methods, `read()` and `write()`, wire everything together:

{{< code-diff title="v1 → v2 — Resource versioning & dependency tracking" >}}
@@ New types (resource state + version tracking) @@
+enum class ResourceState { Undefined, ColorAttachment, DepthAttachment,
+                           ShaderRead, Present };
+
+struct ResourceVersion {                 // NEW v2
+    uint32_t writerPass = UINT32_MAX;    // which pass wrote this version
+    std::vector<uint32_t> readerPasses;  // which passes read it
+};
+
+struct ResourceEntry {
+    ResourceDesc desc;
+    std::vector<ResourceVersion> versions;  // version 0, 1, 2...
+    ResourceState currentState = ResourceState::Undefined;
+};

@@ RenderPass — new fields @@
 struct RenderPass {
     std::string name;
     std::function<void()>             setup;
     std::function<void(/*cmd list*/)> execute;
+    std::vector<ResourceHandle> reads;     // NEW v2
+    std::vector<ResourceHandle> writes;    // NEW v2
+    std::vector<uint32_t> dependsOn;       // NEW v2
+    std::vector<uint32_t> successors;      // NEW v2
+    uint32_t inDegree = 0;                 // NEW v2 (Kahn's)
+    bool     alive    = false;             // NEW v2 (culling)
 };

@@ FrameGraph — read/write methods @@
+    void read(uint32_t passIdx, ResourceHandle h) {
+        auto& ver = entries_[h.index].versions.back();
+        if (ver.writerPass != UINT32_MAX)
+            passes_[passIdx].dependsOn.push_back(ver.writerPass);
+        ver.readerPasses.push_back(passIdx);
+        passes_[passIdx].reads.push_back(h);
+    }
+
+    void write(uint32_t passIdx, ResourceHandle h) {
+        entries_[h.index].versions.push_back({});
+        entries_[h.index].versions.back().writerPass = passIdx;
+        passes_[passIdx].writes.push_back(h);
+    }

@@ Storage @@
-    std::vector<ResourceDesc>  resources_;
+    std::vector<ResourceEntry> entries_;  // now with versioning
{{< /code-diff >}}

Every `write()` pushes a new version. Every `read()` finds the current version's writer and records a `dependsOn` edge. Those edges feed the next three steps.

---

<span id="v2-toposort"></span>

### 📊 Topological sort (Kahn's algorithm)

[Part I](/posts/frame-graph-theory/#sorting-and-culling) covered *why* the graph needs sorting — every pass must run after the passes it depends on. Here's *how*: **Kahn's algorithm**. Count incoming edges per pass. Any pass with zero means all its dependencies are satisfied — emit it, decrement its neighbors' counts, repeat until the queue drains. If the output is shorter than the pass count, you have a cycle.

Before we can sort, we need an adjacency list. `buildEdges()` deduplicates the raw `dependsOn` entries from the versioning step and builds the `successors` list that Kahn's algorithm walks:

{{< code-diff title="v2 — Edge building + Kahn's topological sort" >}}
@@ buildEdges() — deduplicate and build adjacency list @@
+    void buildEdges() {
+        for (uint32_t i = 0; i < passes_.size(); i++) {
+            std::unordered_set<uint32_t> seen;
+            for (uint32_t dep : passes_[i].dependsOn) {
+                if (seen.insert(dep).second) {
+                    passes_[dep].successors.push_back(i);
+                    passes_[i].inDegree++;
+                }
+            }
+        }
+    }

@@ topoSort() — Kahn's algorithm, O(V + E) @@
+    std::vector<uint32_t> topoSort() {
+        std::queue<uint32_t> q;
+        std::vector<uint32_t> inDeg(passes_.size());
+        for (uint32_t i = 0; i < passes_.size(); i++) {
+            inDeg[i] = passes_[i].inDegree;
+            if (inDeg[i] == 0) q.push(i);
+        }
+        std::vector<uint32_t> order;
+        while (!q.empty()) {
+            uint32_t cur = q.front(); q.pop();
+            order.push_back(cur);
+            for (uint32_t succ : passes_[cur].successors) {
+                if (--inDeg[succ] == 0) q.push(succ);
+            }
+        }
+        assert(order.size() == passes_.size() && "Cycle detected!");
+        return order;
+    }
{{< /code-diff >}}

Step through the algorithm interactively — watch nodes with zero in-degree get emitted one by one:

{{< interactive-toposort >}}

---

<span id="v2-culling"></span>

### ✂️ Pass culling

[Part I](/posts/frame-graph-theory/#sorting-and-culling) described culling as "dead-code elimination for GPU work." The implementation is a single backward walk:

<div style="display:grid;grid-template-columns:auto 1fr;gap:.6em .9em;align-items:start;margin:.8em 0 1.2em;padding:.8em 1em;border-radius:10px;background:linear-gradient(135deg,rgba(var(--ds-warn-rgb),.06),transparent);border:1px solid rgba(var(--ds-warn-rgb),.18);font-size:.9em;line-height:1.6;">
  <span style="font-size:1.3em;line-height:1;">🔙</span>
  <span><strong>Walk:</strong> Start at the final output (present / backbuffer). Follow edges backward, marking every reachable pass as <em>alive</em>.</span>
  <span style="font-size:1.3em;line-height:1;">💀</span>
  <span><strong>Result:</strong> Unmarked passes are dead — removed along with their resource declarations.</span>
  <span style="font-size:1.3em;line-height:1;">⏱️</span>
  <span><strong>Cost:</strong> O(V + E) — one linear walk.</span>
</div>

The code is a single backward walk — mark the final pass alive, then propagate backward through `dependsOn` edges:

{{< code-diff title="v2 — Pass culling" >}}
@@ cull() — backward reachability from output @@
+    void cull(const std::vector<uint32_t>& sorted) {
+        if (sorted.empty()) return;
+        passes_[sorted.back()].alive = true;   // last pass = output
+        for (int i = (int)sorted.size() - 1; i >= 0; i--) {
+            if (!passes_[sorted[i]].alive) continue;
+            for (uint32_t dep : passes_[sorted[i]].dependsOn)
+                passes_[dep].alive = true;
+        }
+    }
{{< /code-diff >}}

Toggle edges in the DAG below — disconnect a pass and the compiler removes it along with its resources in real time:

{{< interactive-dag >}}

---

<span id="v2-barriers"></span>

### 🚧 Barrier insertion

[Part I](/posts/frame-graph-theory/#barriers) explained *why* barriers exist — the GPU can't read and write a resource simultaneously, so a state transition must happen between those uses. Here's how the compiler places them automatically.

The algorithm is a single walk over the sorted execution order. Each resource carries a **current state** (e.g. `RENDER_TARGET`, `SHADER_READ`, `DEPTH_WRITE`). For every pass, the compiler checks each resource the pass touches:

1. **Look up** the resource's current state
2. **Compare** it to the state this pass needs (render target? shader input? UAV?)
3. **If they differ** → emit a barrier transitioning from the old state to the new one
4. **Update** the resource's tracked state to the new value

One linear walk, O(V + E). Every barrier lands exactly where the state actually changes — zero redundant transitions, zero missed ones.

<div style="margin:1em 0;padding:.8em 1em;border-radius:8px;border-left:3px solid rgba(var(--ds-danger-rgb),.5);background:rgba(var(--ds-danger-rgb),.04);font-size:.9em;line-height:1.6;">
A real frame needs <strong>dozens of these</strong>. Miss one → rendering corruption or a GPU crash. Add an unnecessary one → the GPU stalls waiting for nothing. The four-step walk above eliminates both failure modes — every barrier is derived from the declared read/write edges, nothing more.
</div>

The implementation walks each pass's reads and writes, comparing the resource's tracked state to what the pass needs. If they differ — emit a barrier and update:

{{< code-diff title="v2 — Barrier insertion + execute() rewrite" >}}
@@ insertBarriers() — emit transitions where state changes @@
+    void insertBarriers(uint32_t passIdx) {
+        auto stateForUsage = [](bool isWrite, Format fmt) {
+            if (isWrite)
+                return (fmt == Format::D32F) ? ResourceState::DepthAttachment
+                                             : ResourceState::ColorAttachment;
+            return ResourceState::ShaderRead;
+        };
+        for (auto& h : passes_[passIdx].reads) {
+            ResourceState needed = ResourceState::ShaderRead;
+            if (entries_[h.index].currentState != needed) {
+                // emit barrier: old state → new state
+                entries_[h.index].currentState = needed;
+            }
+        }
+        for (auto& h : passes_[passIdx].writes) {
+            ResourceState needed = stateForUsage(true, entries_[h.index].desc.format);
+            if (entries_[h.index].currentState != needed) {
+                entries_[h.index].currentState = needed;
+            }
+        }
+    }

@@ execute() — the full v2 pipeline @@
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
{{< /code-diff >}}

All four pieces — versioning, sorting, culling, barriers — compose into that `execute()` body. Each step feeds the next: versioning creates edges, edges feed the sort, the sort enables culling, and the surviving sorted passes get automatic barriers.

Step through a full pipeline below — watch each resource's state update as passes execute, and see exactly where the compiler fires each barrier:

{{< interactive-barriers >}}

---

### 🧩 Full v2 source

{{< include-code file="frame_graph_v2.h" lang="cpp" compact="true" >}}
{{< include-code file="example_v2.cpp" lang="cpp" compile="true" deps="frame_graph_v2.h" compact="true" >}}

That's three of the four intro promises delivered — automatic ordering, barrier insertion, and dead-pass culling. The only piece missing: resources still live for the entire frame. Version 3 fixes that with lifetime analysis and memory aliasing.

UE5's RDG does the same thing. When you call `FRDGBuilder::AddPass`, RDG builds the dependency graph from your declared reads/writes, topologically sorts it, culls dead passes, and inserts barriers — all before recording a single GPU command.

---

## 💾 MVP v3 — Lifetimes & Aliasing

V2 gives us ordering, culling, and barriers — but every transient resource lives for the entire frame. A 1080p deferred pipeline allocates ~52 MB of transient textures that are each used for only 2–3 passes. If their lifetimes don't overlap, they can share physical memory. That's aliasing, and it typically saves 30–50% VRAM.

The algorithm has two steps. First, **scan lifetimes**: walk the sorted pass list and record each transient resource's `firstUse` and `lastUse` pass indices (imported resources are excluded — they're externally owned). Second, **free-list scan**: sort resources by first-use, then greedily try to fit each one into an existing physical block that's compatible (same memory type, large enough, and whose last user finished before this resource's first use). Fit → reuse. No fit → allocate a new block. This is greedy interval-coloring.

Two new structs capture this — a `Lifetime` per resource and a `PhysicalBlock` per heap slot. The lifetime scan is a single walk over the sorted pass list:

{{< code-diff title="v2 → v3 — Lifetime structs & scan" >}}
@@ New structs @@
+struct PhysicalBlock {              // physical memory slot
+    uint32_t sizeBytes  = 0;
+    Format   format     = Format::RGBA8;
+    uint32_t availAfter = 0;        // free after this pass index
+};
+
+struct Lifetime {                   // per-resource timing
+    uint32_t firstUse = UINT32_MAX;
+    uint32_t lastUse  = 0;
+    bool     isTransient = true;
+};

@@ scanLifetimes() — walk sorted passes, record first/last use @@
+    std::vector<Lifetime> scanLifetimes(const std::vector<uint32_t>& sorted) {
+        std::vector<Lifetime> life(entries_.size());
+        for (uint32_t order = 0; order < sorted.size(); order++) {
+            if (!passes_[sorted[order]].alive) continue;
+            for (auto& h : passes_[sorted[order]].reads) {
+                life[h.index].firstUse = std::min(life[h.index].firstUse, order);
+                life[h.index].lastUse  = std::max(life[h.index].lastUse,  order);
+            }
+            for (auto& h : passes_[sorted[order]].writes) {
+                life[h.index].firstUse = std::min(life[h.index].firstUse, order);
+                life[h.index].lastUse  = std::max(life[h.index].lastUse,  order);
+            }
+        }
+        return life;
+    }
{{< /code-diff >}}

Now that we know every resource's lifetime, we can see the waste. Without aliasing, every transient resource is a **committed allocation** — its own chunk of VRAM from creation to end of frame, even if it's only used for 2–3 passes. Here's what that looks like for six transient resources at 1080p:

<div style="margin:1.2em 0;font-size:.85em;">
  <div style="border-radius:10px;overflow:hidden;border:1.5px solid rgba(var(--ds-danger-rgb),.15);">
    <div style="padding:.5em .8em;background:rgba(var(--ds-danger-rgb),.06);border-bottom:1px solid rgba(var(--ds-danger-rgb),.1);font-weight:700;font-size:.9em;text-align:center;">❌ No aliasing — every resource owns its memory for the full frame</div>
    <div style="display:grid;grid-template-columns:140px repeat(7,1fr);gap:0;">
      <div style="padding:.35em .6em;background:rgba(127,127,127,.04);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.08);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.75em;font-weight:600;opacity:.5;"></div>
      <div style="padding:.35em .3em;background:rgba(127,127,127,.04);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.08);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.72em;font-weight:600;opacity:.4;">P1</div>
      <div style="padding:.35em .3em;background:rgba(127,127,127,.04);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.08);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.72em;font-weight:600;opacity:.4;">P2</div>
      <div style="padding:.35em .3em;background:rgba(127,127,127,.04);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.08);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.72em;font-weight:600;opacity:.4;">P3</div>
      <div style="padding:.35em .3em;background:rgba(127,127,127,.04);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.08);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.72em;font-weight:600;opacity:.4;">P4</div>
      <div style="padding:.35em .3em;background:rgba(127,127,127,.04);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.08);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.72em;font-weight:600;opacity:.4;">P5</div>
      <div style="padding:.35em .3em;background:rgba(127,127,127,.04);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.08);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.72em;font-weight:600;opacity:.4;">P6</div>
      <div style="padding:.35em .3em;background:rgba(127,127,127,.04);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.08);text-align:center;font-size:.72em;font-weight:600;opacity:.4;">P7</div>
      <div style="padding:.3em .6em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.78em;font-weight:600;">GBuffer Albedo<div style="font-size:.8em;opacity:.4;">8 MB</div></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-info-rgb),.15);border-top:3px solid var(--ds-info);border-bottom:3px solid var(--ds-info);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-info-rgb),.15);border-top:3px solid var(--ds-info);border-bottom:3px solid var(--ds-info);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-info-rgb),.15);border-top:3px solid var(--ds-info);border-bottom:3px solid var(--ds-info);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="padding:.3em .6em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.78em;font-weight:600;">GBuffer Normals<div style="font-size:.8em;opacity:.4;">8 MB</div></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-code-rgb),.15);border-top:3px solid var(--ds-code);border-bottom:3px solid var(--ds-code);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-code-rgb),.15);border-top:3px solid var(--ds-code);border-bottom:3px solid var(--ds-code);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-code-rgb),.15);border-top:3px solid var(--ds-code);border-bottom:3px solid var(--ds-code);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="padding:.3em .6em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.78em;font-weight:600;">SSAO Scratch<div style="font-size:.8em;opacity:.4;">2 MB</div></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-success-rgb),.15);border-top:3px solid var(--ds-success);border-bottom:3px solid var(--ds-success);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-success-rgb),.15);border-top:3px solid var(--ds-success);border-bottom:3px solid var(--ds-success);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="padding:.3em .6em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.78em;font-weight:600;">SSAO Result<div style="font-size:.8em;opacity:.4;">2 MB</div></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-warn-rgb),.15);border-top:3px solid var(--ds-warn);border-bottom:3px solid var(--ds-warn);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-warn-rgb),.15);border-top:3px solid var(--ds-warn);border-bottom:3px solid var(--ds-warn);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="padding:.3em .6em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.78em;font-weight:600;">HDR Lighting<div style="font-size:.8em;opacity:.4;">16 MB</div></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-pink-rgb),.15);border-top:3px solid var(--ds-pink);border-bottom:3px solid var(--ds-pink);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-pink-rgb),.15);border-top:3px solid var(--ds-pink);border-bottom:3px solid var(--ds-pink);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="padding:.3em .6em;border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.78em;font-weight:600;">Bloom Scratch<div style="font-size:.8em;opacity:.4;">16 MB</div></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-danger-rgb),.06);"></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(14,165,233,.15);border-top:3px solid #0ea5e9;border-bottom:3px solid #0ea5e9;"></div>
      <div style="background:rgba(14,165,233,.15);border-top:3px solid #0ea5e9;border-bottom:3px solid #0ea5e9;"></div>
    </div>
  </div>
  <div style="margin-top:.4em;font-size:.82em;opacity:.6;">
    <span style="color:var(--ds-danger);">Red cells</span> = memory allocated but unused — wasted VRAM. Each resource holds its full allocation across the entire frame even though it's only active for 2–3 passes. Total: <strong style="color:var(--ds-danger);">52 MB</strong> committed.
  </div>
</div>

Most of that memory sits idle. The colored bars show when each resource is actually used — everything else is waste. The graph knows every lifetime, so it can do better. Resources whose lifetimes don't overlap can share the same physical memory:

<div style="margin:1.2em 0;font-size:.85em;">
  <div style="border-radius:10px;overflow:hidden;border:1.5px solid rgba(var(--ds-indigo-rgb),.15);">
    <div style="display:grid;grid-template-columns:140px repeat(7,1fr);gap:0;">
      <div style="padding:.4em .6em;background:rgba(var(--ds-indigo-rgb),.06);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-weight:700;font-size:.82em;">Resource</div>
      <div style="padding:.4em .3em;background:rgba(var(--ds-indigo-rgb),.06);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.75em;font-weight:600;opacity:.5;">P1</div>
      <div style="padding:.4em .3em;background:rgba(var(--ds-indigo-rgb),.06);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.75em;font-weight:600;opacity:.5;">P2</div>
      <div style="padding:.4em .3em;background:rgba(var(--ds-indigo-rgb),.06);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.75em;font-weight:600;opacity:.5;">P3</div>
      <div style="padding:.4em .3em;background:rgba(var(--ds-indigo-rgb),.06);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.75em;font-weight:600;opacity:.5;">P4</div>
      <div style="padding:.4em .3em;background:rgba(var(--ds-indigo-rgb),.06);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.75em;font-weight:600;opacity:.5;">P5</div>
      <div style="padding:.4em .3em;background:rgba(var(--ds-indigo-rgb),.06);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);text-align:center;font-size:.75em;font-weight:600;opacity:.5;">P6</div>
      <div style="padding:.4em .3em;background:rgba(var(--ds-indigo-rgb),.06);border-bottom:1px solid rgba(var(--ds-indigo-rgb),.1);text-align:center;font-size:.75em;font-weight:600;opacity:.5;">P7</div>
      <div style="padding:.35em .6em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.82em;font-weight:600;">GBuffer Albedo<div style="font-size:.8em;opacity:.4;">8 MB</div></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-info-rgb),.2);border-top:3px solid var(--ds-info);border-bottom:3px solid var(--ds-info);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-info-rgb),.2);border-top:3px solid var(--ds-info);border-bottom:3px solid var(--ds-info);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-info-rgb),.2);border-top:3px solid var(--ds-info);border-bottom:3px solid var(--ds-info);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);"></div>
      <div style="padding:.35em .6em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.82em;font-weight:600;">HDR Lighting<div style="font-size:.8em;opacity:.4;">16 MB → <span style="color:var(--ds-info);">slot A</span></div></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-info-rgb),.2);border-top:3px solid var(--ds-info);border-bottom:3px solid var(--ds-info);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-info-rgb),.2);border-top:3px solid var(--ds-info);border-bottom:3px solid var(--ds-info);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);"></div>
      <div style="padding:.35em .6em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.82em;font-weight:600;">GBuffer Normals<div style="font-size:.8em;opacity:.4;">8 MB</div></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-code-rgb),.2);border-top:3px solid var(--ds-code);border-bottom:3px solid var(--ds-code);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-code-rgb),.2);border-top:3px solid var(--ds-code);border-bottom:3px solid var(--ds-code);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-code-rgb),.2);border-top:3px solid var(--ds-code);border-bottom:3px solid var(--ds-code);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);"></div>
      <div style="padding:.35em .6em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.82em;font-weight:600;">Bloom Scratch<div style="font-size:.8em;opacity:.4;">16 MB → <span style="color:var(--ds-code);">slot B</span></div></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-code-rgb),.2);border-top:3px solid var(--ds-code);border-bottom:3px solid var(--ds-code);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);background:rgba(var(--ds-code-rgb),.2);border-top:3px solid var(--ds-code);border-bottom:3px solid var(--ds-code);"></div>
      <div style="padding:.35em .6em;border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.82em;font-weight:600;">SSAO Scratch<div style="font-size:.8em;opacity:.4;">2 MB</div></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-success-rgb),.2);border-top:3px solid var(--ds-success);border-bottom:3px solid var(--ds-success);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-success-rgb),.2);border-top:3px solid var(--ds-success);border-bottom:3px solid var(--ds-success);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-bottom:1px solid rgba(var(--ds-indigo-rgb),.06);"></div>
      <div style="padding:.35em .6em;border-right:1px solid rgba(var(--ds-indigo-rgb),.08);font-size:.82em;font-weight:600;">SSAO Result<div style="font-size:.8em;opacity:.4;">2 MB → <span style="color:var(--ds-success);">slot C</span></div></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-success-rgb),.2);border-top:3px solid var(--ds-success);border-bottom:3px solid var(--ds-success);"></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);background:rgba(var(--ds-success-rgb),.2);border-top:3px solid var(--ds-success);border-bottom:3px solid var(--ds-success);"></div>
      <div style="border-right:1px solid rgba(var(--ds-indigo-rgb),.05);"></div>
      <div></div>
    </div>
  </div>
  <div style="margin-top:.5em;font-size:.82em;opacity:.6;">Same color = same physical memory. GBuffer Albedo dies at P4, HDR Lighting starts at P5 → both fit in <span style="color:var(--ds-info);font-weight:600;">slot A</span>. Three physical blocks serve six virtual resources.</div>
</div>

<div style="display:flex;align-items:center;gap:1em;margin:1em 0;padding:.6em 1em;border-radius:8px;background:linear-gradient(90deg,rgba(var(--ds-danger-rgb),.06),rgba(var(--ds-success-rgb),.06));">
  <div style="text-align:center;line-height:1.3;">
    <div style="font-size:.75em;opacity:.6;text-transform:uppercase;letter-spacing:.05em;">Without aliasing</div>
    <div style="font-size:1.4em;font-weight:800;color:var(--ds-danger);">52 MB</div>
  </div>
  <div style="font-size:1.5em;opacity:.3;">→</div>
  <div style="text-align:center;line-height:1.3;">
    <div style="font-size:.75em;opacity:.6;text-transform:uppercase;letter-spacing:.05em;">With aliasing</div>
    <div style="font-size:1.4em;font-weight:800;color:var(--ds-success);">36 MB</div>
  </div>
  <div style="margin-left:auto;font-size:.85em;line-height:1.4;opacity:.8;">
    3 physical blocks shared across 6 virtual resources.<br>
    <strong style="color:var(--ds-success);">31% saved</strong> — in complex pipelines: 40–50%.
  </div>
</div>

This requires **placed resources** at the API level — GPU memory allocated from a heap, with resources bound to offsets within it. In D3D12, that means `ID3D12Heap` + `CreatePlacedResource`. In Vulkan, `VkDeviceMemory` + `vkBindImageMemory` at different offsets. Without placed resources (i.e., `CreateCommittedResource` or Vulkan dedicated allocations), each resource gets its own memory and aliasing is impossible — which is why the graph's allocator works with heaps.

The second half of the algorithm — the greedy free-list allocator. Sort resources by `firstUse`, then try to fit each one into an existing block whose previous user has finished:

{{< code-diff title="v3 — Greedy free-list aliasing + compile() integration" >}}
@@ aliasResources() — greedy free-list scan @@
+    std::vector<uint32_t> aliasResources(const std::vector<Lifetime>& lifetimes) {
+        std::vector<PhysicalBlock> freeList;
+        std::vector<uint32_t> mapping(entries_.size(), UINT32_MAX);
+
+        // sort resources by firstUse
+        std::vector<uint32_t> indices(entries_.size());
+        std::iota(indices.begin(), indices.end(), 0);
+        std::sort(indices.begin(), indices.end(), [&](uint32_t a, uint32_t b) {
+            return lifetimes[a].firstUse < lifetimes[b].firstUse;
+        });
+
+        for (uint32_t resIdx : indices) {
+            if (!lifetimes[resIdx].isTransient) continue;
+            if (lifetimes[resIdx].firstUse == UINT32_MAX) continue;
+            uint32_t needed = /* width * height * bpp */;
+            bool reused = false;
+            for (uint32_t b = 0; b < freeList.size(); b++) {
+                if (freeList[b].availAfter < lifetimes[resIdx].firstUse
+                    && freeList[b].sizeBytes >= needed) {
+                    mapping[resIdx] = b;         // reuse this block
+                    freeList[b].availAfter = lifetimes[resIdx].lastUse;
+                    reused = true; break;
+                }
+            }
+            if (!reused) {
+                mapping[resIdx] = freeList.size();
+                freeList.push_back({ needed, fmt, lifetimes[resIdx].lastUse });
+            }
+        }
+        return mapping;
+    }

@@ compile() — v3 adds lifetime scan + aliasing @@
     auto sorted = topoSort();
     cull(sorted);
+    auto lifetimes = scanLifetimes(sorted);     // NEW v3
+    auto mapping   = aliasResources(lifetimes); // NEW v3
+    // mapping[virtualIdx] → physicalBlock — execute just runs passes
{{< /code-diff >}}

~70 new lines on top of v2. Aliasing runs once per frame in O(R log R) — sort, then linear scan of the free list. Sub-microsecond for 15 transient resources.

Drag the interactive timeline below to see how resources share physical blocks as their lifetimes end:

{{< interactive-aliasing >}}

That's the full value prop — automatic memory aliasing *and* automatic barriers from a single `FrameGraph` class. UE5's transient resource allocator does the same thing: any `FRDGTexture` created through `FRDGBuilder::CreateTexture` (vs `RegisterExternalTexture`) is transient and eligible for aliasing, using the same lifetime analysis and free-list scan we just built.

---

### 🧩 Full v3 source

{{< include-code file="frame_graph_v3.h" lang="cpp" compact="true" >}}
{{< include-code file="example_v3.cpp" lang="cpp" compile="true" deps="frame_graph_v3.h" compact="true" >}}

---

### ✅ What the MVP delivers

Three iterations produced a single `FrameGraph` class. Here's what it does every frame, broken down by phase — the same declare → compile → execute lifecycle from [Part I](/posts/frame-graph-theory/):

<div style="margin:1.2em 0;display:grid;grid-template-columns:repeat(3,1fr);gap:.8em;">
  <div style="padding:.8em 1em;border-radius:10px;border-top:3px solid var(--ds-info);background:rgba(var(--ds-info-rgb),.04);">
    <div style="font-weight:800;font-size:.88em;margin-bottom:.5em;color:var(--ds-info);">① Declare</div>
    <div style="font-size:.84em;line-height:1.6;opacity:.85;">
      Each <code>addPass</code> runs its setup lambda:<br>
      • declare reads &amp; writes<br>
      • request virtual resources<br>
      • version tracking builds edges
    </div>
    <div style="margin-top:.5em;padding:.3em .5em;border-radius:5px;background:rgba(var(--ds-info-rgb),.08);font-size:.76em;line-height:1.4;border:1px solid rgba(var(--ds-info-rgb),.12);">
      <strong>Zero GPU work.</strong> Resources are descriptions — no memory allocated yet.
    </div>
  </div>
  <div style="padding:.8em 1em;border-radius:10px;border-top:3px solid var(--ds-code);background:rgba(var(--ds-code-rgb),.04);">
    <div style="font-weight:800;font-size:.88em;margin-bottom:.5em;color:var(--ds-code);">② Compile</div>
    <div style="font-size:.84em;line-height:1.6;opacity:.85;">
      All automatic, all linear-time:<br>
      • <strong>sort</strong> — topo order (Kahn's)<br>
      • <strong>cull</strong> — kill dead passes<br>
      • <strong>scan lifetimes</strong> — first/last use<br>
      • <strong>alias</strong> — free-list reuse<br>
      • <strong>compute barriers</strong>
    </div>
    <div style="margin-top:.5em;padding:.3em .5em;border-radius:5px;background:rgba(var(--ds-code-rgb),.08);font-size:.76em;line-height:1.4;border:1px solid rgba(var(--ds-code-rgb),.12);">
      Everything linear or near-linear — all data fits in L1 cache.
    </div>
  </div>
  <div style="padding:.8em 1em;border-radius:10px;border-top:3px solid var(--ds-success);background:rgba(var(--ds-success-rgb),.04);">
    <div style="font-weight:800;font-size:.88em;margin-bottom:.5em;color:var(--ds-success);">③ Execute</div>
    <div style="font-size:.84em;line-height:1.6;opacity:.85;">
      Walk sorted, living passes:<br>
      • insert automatic barriers<br>
      • call execute lambda<br>
      • resources already aliased &amp; bound
    </div>
    <div style="margin-top:.5em;padding:.3em .5em;border-radius:5px;background:rgba(var(--ds-success-rgb),.08);font-size:.76em;line-height:1.4;border:1px solid rgba(var(--ds-success-rgb),.12);">
      <strong>Lambdas see a fully resolved environment.</strong> No manual barriers, no manual memory.
    </div>
  </div>
</div>

**Compile cost by step:**

<div style="overflow-x:auto;margin:.6em 0 1em">
<table style="width:100%;border-collapse:collapse;font-size:.88em">
  <thead>
    <tr>
      <th style="padding:.5em .8em;text-align:left;border-bottom:2px solid rgba(var(--ds-code-rgb),.3);color:var(--ds-code);width:30%">Compile step</th>
      <th style="padding:.5em .8em;text-align:center;border-bottom:2px solid rgba(var(--ds-code-rgb),.3);width:18%">Complexity</th>
      <th style="padding:.5em .8em;text-align:left;border-bottom:2px solid rgba(var(--ds-code-rgb),.3)">Algorithm</th>
    </tr>
  </thead>
  <tbody>
    <tr><td style="padding:.4em .8em;font-weight:600;">Topological sort</td><td style="padding:.4em .8em;text-align:center;font-family:ui-monospace,monospace;color:var(--ds-code)">O(V + E)</td><td style="padding:.4em .8em;font-size:.9em;opacity:.8">Kahn's — passes + edges</td></tr>
    <tr style="background:rgba(127,127,127,.04)"><td style="padding:.4em .8em;font-weight:600;">Pass culling</td><td style="padding:.4em .8em;text-align:center;font-family:ui-monospace,monospace;color:var(--ds-code)">O(V + E)</td><td style="padding:.4em .8em;font-size:.9em;opacity:.8">Backward reachability from output</td></tr>
    <tr><td style="padding:.4em .8em;font-weight:600;">Lifetime scan</td><td style="padding:.4em .8em;text-align:center;font-family:ui-monospace,monospace;color:var(--ds-code)">O(V + E)</td><td style="padding:.4em .8em;font-size:.9em;opacity:.8">Walk sorted passes and their read/write edges</td></tr>
    <tr style="background:rgba(127,127,127,.04)"><td style="padding:.4em .8em;font-weight:600;">Aliasing</td><td style="padding:.4em .8em;text-align:center;font-family:ui-monospace,monospace;color:var(--ds-code)">O(R log R)</td><td style="padding:.4em .8em;font-size:.9em;opacity:.8">Sort by first-use, greedy free-list scan</td></tr>
    <tr><td style="padding:.4em .8em;font-weight:600;">Barrier computation</td><td style="padding:.4em .8em;text-align:center;font-family:ui-monospace,monospace;color:var(--ds-code)">O(V + E)</td><td style="padding:.4em .8em;font-size:.9em;opacity:.8">Walk passes and their read/write edges with state lookup</td></tr>
  </tbody>
</table>
</div>
<div style="font-size:.84em;line-height:1.5;opacity:.7;margin:-.3em 0 1em 0">V = passes (~25), E = dependency edges (~50), R = transient resources (~15). Everything linear or near-linear.</div>

The graph doesn't care about your rendering *strategy*. It cares about your *dependencies*. Deferred or forward, the same `FrameGraph` class handles both — different topology, same automatic barriers and aliasing. That's the whole point.

---

<div style="margin:2em 0 0;padding:1em 1.2em;border-radius:10px;border:1px solid rgba(var(--ds-indigo-rgb),.2);background:rgba(var(--ds-indigo-rgb),.03);display:flex;justify-content:space-between;align-items:center;">
  <a href="../frame-graph-theory/" style="text-decoration:none;font-weight:700;font-size:.95em;">
    ← Previous: Part I — Theory
  </a>
  <a href="../frame-graph-production/" style="text-decoration:none;font-weight:700;font-size:.95em;">
    Next: Part III — Production Engines →
  </a>
</div>
