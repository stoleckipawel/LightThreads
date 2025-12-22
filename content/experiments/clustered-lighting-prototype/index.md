---
title: "Example Experiment: Clustered Lighting Prototype — Notes"
date: 2025-12-22
draft: false
description: "A practical experiment log template: cluster build, light assignment, debug overlays, and perf metrics." 
tags: ["clustered", "lighting", "forward+", "gpu"]
---

This is an **example experiment log**.

Use this format when you’re iterating daily and want to capture what you tried + what you learned.

## Goals

- Stable cluster build cost across scenes
- Easy instrumentation (GPU timers + counters)
- Clear debug overlays for correctness
- A comparison point vs tiled Forward+

## Minimal data model (example)

- `clusters[]`: bounds in view space (or derived from slice/tile)
- `clusterLightCount[]`: how many lights per cluster
- `clusterLightOffset[]`: prefix-sum offsets
- `clusterLightList[]`: packed light indices

## Build steps (example)

1. Choose Z slicing (linear or logarithmic in view space)
2. Build clusters (XY tiles × Z slices)
3. For each light, compute affected cluster range
4. Append light indices into cluster lists (atomics or two-pass prefix sum)
5. Shade pixels: fetch the cluster list for that pixel

## Checklist

- [ ] Cluster build (Z slices + XY tiles)
- [ ] Light list build (two-pass: counts → prefix sum → scatter)
- [ ] Debug visualization: cluster heatmap + max lights/cluster
- [ ] Instrumentation: GPU timer per step + counters (atomics, list sizes)
- [ ] Compare vs tiled Forward+ baseline

## Metrics to record (so your results are interpretable)

- Scene resolution
- Lights total + average lights visible
- Max lights per cluster + percentile distribution (P50/P95/P99)
- Time for: counts pass, prefix sum, scatter pass, shading pass

## Debug overlays

- Cluster ID visualization (color by Z slice)
- Lights-per-cluster heatmap
- “Light influence” view: show which lights hit the current pixel

## Capture placeholder

![Placeholder screenshot](/images/sample-screenshot.svg)
