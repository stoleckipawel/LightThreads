---
title: "Example Analysis: GPU Performance Workflow — From Symptom to Root Cause"
date: 2025-12-22
draft: false
description: "A repeatable workflow for profiling a frame: isolate passes, validate correctness, and decide what to optimize next."
tags: ["profiling", "gpu", "analysis", "rendering"]
---

This is an **example analysis page** you can adapt.

## Goal

Turn a vague symptom ("frame is slow") into a prioritized list of fixes.

## Step 0 — Make timings trustworthy

Before optimization, verify:

- You have **GPU timestamps** around major passes (not just CPU timers)
- VSync is off for profiling runs
- You can reproduce the same camera/scene (fixed seed / deterministic path)

## Step 1 — Find the top 3 passes

Write down the biggest contributors, e.g.:

- `GBuffer`: 2.1 ms
- `Shadows`: 3.4 ms
- `TAA/Upscale`: 1.6 ms

If you don’t have named passes, add them first — optimizing without pass boundaries is guesswork.

## Step 2 — Decide: bandwidth-bound or ALU-bound (quick tests)

Cheap tests that usually reveal direction:

- **Resolution scale test**: render at 70% / 100% / 140%
  - If time scales ~quadratically with pixels, you’re likely bandwidth/fill bound.
- **Disable features**: turn off SSR/SSAO/bloom one at a time
- **Format test**: try smaller RT formats temporarily (where safe)

## Step 3 — Validate correctness with one debug view

For the top pass, define a “correctness view” so you don’t optimize a bug:

- Lighting pass → visualize N·L, shadow factor, and roughness
- TAA → visualize history weight and reprojection validity
- Shadows → visualize receiver plane depth bias / cascade selection

## Step 4 — Pick one change with a measurable hypothesis

Example hypothesis format:

- Change: halve SSAO resolution
- Expectation: SSAO pass drops from 1.2 ms → ~0.5–0.7 ms
- Risk: more noise / loss of fine contact shadows
- Validation: compare before/after with fixed camera and a difference heatmap

## Step 5 — Record results like an engineer

Minimum record per change:

- Scene name + camera pose
- GPU/driver
- Resolution
- Before/after timings (ms)
- Screenshot/video links
- Notes about side effects
