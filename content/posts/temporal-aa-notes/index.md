---
title: "Example Article: Temporal AA — Debug Views, Failure Modes, and Fixes"
date: 2025-12-22
draft: false
description: "A practical TAA debugging checklist: what to visualize, how to map artifacts to causes, and what to try first."
tags: ["taa", "temporal", "antialiasing", "rendering"]
---

Temporal techniques are incredibly powerful, but debugging them is mostly about making the *invisible* visible.

This is an **example article** you can replace later.

## Debug views to build on day 1

- Motion vectors (raw + decoded, with magnitude heatmap)
- Reprojection validity mask (in-bounds, depth test, normal test)
- History weight / accumulation weight
- Clamp bounds visualization (min/max or radius)
- “History vs current” difference heatmap
- Reactive / translucency mask (if you do TSR-like behavior)

If you only build one: **history vs current difference heatmap**. It catches a lot fast.

## Common failure modes → likely causes

- **Ghosting** → history weight too high, or reprojection validity too permissive
- **Disocclusion trails** → missing disocclusion detection (depth/normal test), or MV mismatch
- **Jitter “boiling”** → unstable shading inputs (normal maps, specular, SSAO noise)
- **Specular shimmer** → insufficient prefiltering + too much sharpening

## First fixes to try (in order)

1. Tighten reprojection validity (depth + normal threshold)
2. Add neighborhood clamp (or variance clip) to stop history outliers
3. Lower history weight in disocclusions (or use responsive/reactive mask)
4. Ensure MVs are correct for animated/skinned meshes and camera cuts

## Minimal pseudo-code (shape, not exact math)

```cpp
struct TaaInput {
	Texture2D currentColor;
	Texture2D currentDepth;
	Texture2D currentNormal;
	Texture2D motionVectors;   // from current -> previous
	Texture2D historyColor;    // previous resolved
};

Texture2D TaaResolve(const TaaInput& in)
{
	// 1) Reproject history
	float2 uvPrev = uv + in.motionVectors.Sample(uv);
	float3 history = in.historyColor.Sample(uvPrev);

	// 2) Validate reprojection
	bool valid = DepthNormalTest(in.currentDepth, in.currentNormal, uv, uvPrev);

	// 3) Clamp history to neighborhood
	float3 current = in.currentColor.Sample(uv);
	float3 clampedHistory = ClampToNeighborhood(history, in.currentColor, uv);

	// 4) Blend
	float w = valid ? HistoryWeight(uv) : 0.0;
	return lerp(current, clampedHistory, w);
}
```

## Embedded video example

If you want to embed a YouTube clip of an experiment, use this shortcode:

{{< youtube dQw4w9WgXcQ >}}

(Replace the ID with your own video.)
