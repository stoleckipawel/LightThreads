---
title: "Example Article: Modern Rendering Pipeline — Practical Overview"
date: 2025-12-22
draft: false
description: "A practical end-to-end frame breakdown with a pass checklist, debug views, and common performance traps."
tags: ["rendering", "pipeline", "gpu"]
---

This is an **example article** you can edit/replace.
It’s designed to be genuinely useful: it gives you a “frame mental model” and a checklist you can apply to your own engine.

![Placeholder screenshot](/images/sample-screenshot.svg)

## Frame structure (high-level)

A typical modern frame (game/real-time) can be thought of as:

1. **Visibility** — build a visible set (culling, LOD, occlusion)
2. **Depth** — optional prepass; also drives Hi-Z, SSAO, SSR, etc.
3. **Material** — evaluate material inputs (deferred GBuffer or forward shading)
4. **Lighting** — direct lighting + shadows + (optional) GI
5. **Post** — temporal AA/upscale, tone mapping, bloom, color grading
6. **UI** — compositing and text

If you ever feel lost, ask: “what buffers exist right now, and who produces/consumes them?”

## Pass checklist (what to name and measure)

When you profile, you want stable pass names that map to real work:

- `Cull/BuildVisibleSet`
- `DepthPrepass`
- `GBuffer` (or `ForwardOpaque`)
- `ShadowMap(s)`
- `Lighting` (or `ForwardLighting`)
- `SSR/SSAO` (if used)
- `TAA/Upscale`
- `ToneMap + Bloom`
- `UI`

For each pass, record:

- GPU time (ms)
- Render target formats + resolution
- Key shader(s) + draw/dispatch counts
- Bandwidth-heavy vs ALU-heavy guess

## Minimal pseudo-code

```cpp
void RenderFrame(const Camera& camera)
{
    UpdateScene();

    VisibleSet visible = Cull(camera);

    RenderDepth(visible);
    RenderMaterial(visible);

    RenderLighting(visible);

    RenderPost();
    RenderUI();
}

## GBuffer format sanity (example)

If you use deferred shading, keep a small table in your notes like:

- GBuffer0: baseColor (sRGB) + roughness
- GBuffer1: normal (2ch encoding) + metallic
- GBuffer2: material ID / flags / AO
- Depth: D32 or D24S8 (platform dependent)

Then verify:

- Are you accidentally writing to multiple RTs in a pass that doesn’t need them?
- Are normals in the expected space (view vs world) consistently?

## Debug views that pay for themselves

- World normal (decoded)
- Roughness / metallic
- Motion vectors
- Overdraw / quad overdraw (if available)
- Luminance heatmap pre-tonemap

## Common performance traps

- Doing expensive material evaluation in passes that should be “depth only”
- Recomputing the same BRDF terms in multiple passes
- Full-res post effects that don’t need full-res
- Too many transient allocations (watch your frame graph / aliasing)
```

## Quick notes

- Treat this as a **starting point**. The interesting bits are always the constraints.
- When I add real content, I’ll attach **RenderDoc captures**, graphs, and perf notes.
