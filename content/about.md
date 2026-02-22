---
title: "About"
description: "Personal notes on real-time rendering and software engineering."
date: 2026-02-09
showDate: false
showReadingTime: false
showWordCount: false
---

<!-- Animations -->
<style>
@keyframes fadeSlideUp {
  from { opacity: 0; transform: translateY(18px); }
  to   { opacity: 1; transform: translateY(0); }
}
@keyframes fadeIn {
  from { opacity: 0; }
  to   { opacity: 1; }
}
@keyframes pulseGlow {
  0%, 100% { box-shadow: 0 0 0 0 rgba(var(--ds-accent-rgb),.5), 0 0 8px rgba(var(--ds-accent-rgb),.15); }
  50%      { box-shadow: 0 0 0 8px rgba(var(--ds-accent-rgb),0), 0 0 16px rgba(var(--ds-highlight-rgb),.08); }
}
@keyframes shimmer {
  0%   { background-position: -200% center; }
  100% { background-position: 200% center; }
}
@keyframes tagPop {
  from { opacity: 0; transform: scale(.85); }
  to   { opacity: 1; transform: scale(1); }
}

/* Hero */
.about-hero {
  animation: fadeSlideUp .7s ease-out both;
}
.about-hero::before {
  content: '';
  position: absolute; inset: -1.5px; border-radius: 15px;
  background: linear-gradient(90deg, transparent 15%, rgba(var(--ds-accent-rgb),.35) 45%, rgba(var(--ds-highlight-rgb),.25) 55%, transparent 85%);
  background-size: 200% 100%;
  animation: shimmer 5s ease-in-out infinite;
  pointer-events: none;
  mask: linear-gradient(#fff 0 0) content-box, linear-gradient(#fff 0 0);
  mask-composite: exclude; -webkit-mask-composite: xor;
  padding: 1.5px;
}
.about-hero::after {
  content: '';
  position: absolute; inset: 0; border-radius: 14px;
  background: radial-gradient(ellipse 70% 80% at 85% 20%, rgba(var(--ds-accent-rgb),.07) 0%, transparent 70%);
  pointer-events: none;
}

/* Topic cards */
.about-card {
  animation: fadeSlideUp .5s ease-out both;
  transition: transform .22s ease, box-shadow .22s ease, border-color .22s ease;
}
.about-card:nth-child(1) { animation-delay: .10s; }
.about-card:nth-child(2) { animation-delay: .17s; }
.about-card:nth-child(3) { animation-delay: .24s; }
.about-card:nth-child(4) { animation-delay: .31s; }
.about-card:nth-child(5) { animation-delay: .38s; }
.about-card:nth-child(6) { animation-delay: .45s; }
.about-card:hover {
  transform: translateY(-3px);
  box-shadow: 0 6px 24px rgba(var(--ds-accent-rgb),.12), 0 0 40px rgba(var(--ds-accent-rgb),.04);
  border-color: rgba(var(--ds-accent-rgb),.4);
  background: rgba(var(--ds-accent-rgb),.06);
}
.about-card .card-icon {
  display: inline-block;
  transition: transform .25s ease;
}
.about-card:hover .card-icon {
  transform: scale(1.2);
}

/* Author card */
.about-author {
  animation: fadeSlideUp .6s .3s ease-out both;
}
.about-author::before {
  content: '';
  position: absolute; left: 0; top: 12%; bottom: 12%;
  width: 3px; border-radius: 2px;
  background: linear-gradient(180deg, var(--ds-accent) 0%, var(--ds-highlight) 50%, var(--ds-warm) 100%);
}

/* Timeline entries */
.tl-entry {
  animation: fadeSlideUp .55s ease-out both;
}
.tl-entry:nth-child(1) { animation-delay: .4s; }
.tl-entry:nth-child(2) { animation-delay: .55s; }
.tl-entry:nth-child(3) { animation-delay: .70s; }

/* Pulsing current-role dot — uses pseudo-element so box-shadow doesn’t shift the dot */
.tl-dot-active {
  position: relative;
}
.tl-dot-active::after {
  content: '';
  position: absolute; inset: -3px;
  border-radius: 50%;
  animation: pulseGlow 2.4s ease-in-out infinite;
}

/* Skill tags staggered pop */
.tl-tags span {
  animation: tagPop .35s ease-out both;
}
.tl-tags span:nth-child(1) { animation-delay: .50s; }
.tl-tags span:nth-child(2) { animation-delay: .57s; }
.tl-tags span:nth-child(3) { animation-delay: .64s; }
.tl-tags span:nth-child(4) { animation-delay: .71s; }
.tl-tags span:nth-child(5) { animation-delay: .78s; }
.tl-tags span:nth-child(6) { animation-delay: .85s; }

/* Current badge pulse */
.badge-current {
  animation: fadeIn .4s .45s ease-out both;
  position: relative;
}
.badge-current::after {
  content: '';
  position: absolute; inset: -2px; border-radius: 7px;
  background: rgba(var(--ds-highlight-rgb),.12);
  animation: pulseGlow 2.8s 1s ease-in-out infinite;
}

/* Section headings — warm underline accent */
.article-content h2 {
  position: relative;
  padding-bottom: .35em;
}
.article-content h2::after {
  content: '';
  position: absolute; left: 0; bottom: 0;
  width: 2.5em; height: 2.5px; border-radius: 2px;
  background: linear-gradient(90deg, var(--ds-accent), var(--ds-highlight));
  opacity: .7;
}

/* Disclaimer fade */
.about-disclaimer {
  animation: fadeIn .6s .9s ease-out both;
}

/* Reduce motion for accessibility */
@media (prefers-reduced-motion: reduce) {
  *, *::before, *::after {
    animation-duration: 0.01ms !important;
    animation-delay: 0ms !important;
    transition-duration: 0.01ms !important;
  }
}
</style>

<!-- Hero intro -->
<div class="about-hero" style="position:relative;margin:0 0 2em;padding:1.6em 1.8em;border-radius:14px;border:1.5px solid rgba(var(--ds-accent-rgb),.2);background:linear-gradient(135deg,rgba(var(--ds-accent-rgb),.06) 0%,rgba(var(--ds-highlight-rgb),.03) 40%,rgba(var(--ds-warm-rgb),.04) 100%);line-height:1.7;overflow:hidden;">
  <div style="font-size:1.3em;font-weight:800;margin-bottom:.35em;background:linear-gradient(90deg,var(--ds-soft),var(--ds-accent),var(--ds-highlight));-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text;display:inline-block;">Deep Spark</div>
  <div style="font-size:1em;">
    A compact notebook for <strong>engine design</strong>, <strong>GPU/CPU/memory profiling</strong>, <strong>PBR</strong>, <strong>ray tracing</strong>, and <strong>practical performance work</strong>.
  </div>
</div>

---

## About the Author

<div class="about-author" style="position:relative;margin:1.2em 0 1.5em;padding:1.3em 1.5em 1.3em 1.8em;border-radius:12px;border:1.5px solid rgba(var(--ds-accent-rgb),.18);background:linear-gradient(135deg,rgba(var(--ds-accent-rgb),.05) 0%,rgba(var(--ds-highlight-rgb),.02) 60%,transparent 100%);line-height:1.7;overflow:hidden;">
  <div style="font-size:1.12em;font-weight:800;margin-bottom:.4em;color:var(--ds-highlight);">Pawel Stolecki</div>
  <div style="font-size:.95em;">
    3D graphics software engineer focused on real-time rendering. I enjoy the intersection where visual-quality research meets hands-on optimization — pushing shading closer to ground truth while keeping systems efficient and shippable.
  </div>
</div>

### Experience

<div style="position:relative;margin:1.4em 0 2em;padding-left:2.2em;border-left:3px solid transparent;border-image:linear-gradient(180deg,rgba(var(--ds-accent-rgb),.35),rgba(var(--ds-highlight-rgb),.15),rgba(var(--ds-warm-rgb),.1)) 1;">

  <!-- CD PROJEKT RED -->
  <div class="tl-entry" style="position:relative;margin-bottom:2.2em;">
    <div class="tl-dot-active" style="position:absolute;left:-2.9em;top:.15em;width:1.4em;height:1.4em;border-radius:50%;background:linear-gradient(135deg,var(--ds-soft),var(--ds-accent));display:flex;align-items:center;justify-content:center;">
      <div style="width:.5em;height:.5em;border-radius:50%;background:#fff;"></div>
    </div>
    <div style="display:flex;align-items:baseline;gap:.6em;flex-wrap:wrap;">
      <span style="font-weight:800;font-size:1.08em;">CD PROJEKT RED</span>
      <span class="badge-current" style="font-size:.78em;padding:.18em .6em;border-radius:5px;background:rgba(var(--ds-highlight-rgb),.12);color:var(--ds-highlight);font-weight:600;">Current</span>
    </div>
    <div style="font-size:.82em;opacity:.55;margin:.15em 0 .5em;">Rendering Engineer</div>
    <div style="font-size:.92em;line-height:1.7;">
      Driving lighting technology improvements. The work is hands-on and investigative — prototype a change, measure it on hardware, trace what the GPU, CPU, memory is actually doing, and ship what moves the needle.
    </div>
    <div class="tl-tags" style="display:flex;flex-wrap:wrap;gap:.4em;margin-top:.6em;">
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-accent-rgb),.18);background:rgba(var(--ds-accent-rgb),.06);color:rgba(var(--ds-accent-rgb),.85);font-weight:600;">Direct Lighting</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-accent-rgb),.18);background:rgba(var(--ds-accent-rgb),.06);color:rgba(var(--ds-accent-rgb),.85);font-weight:600;">Indirect Lighting</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-highlight-rgb),.18);background:rgba(var(--ds-highlight-rgb),.06);color:rgba(var(--ds-highlight-rgb),.85);font-weight:600;">GPU/CPU Profiling</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-highlight-rgb),.18);background:rgba(var(--ds-highlight-rgb),.06);color:rgba(var(--ds-highlight-rgb),.85);font-weight:600;">Memory Profiling</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-warm-rgb),.18);background:rgba(var(--ds-warm-rgb),.06);color:rgba(var(--ds-warm-rgb),.85);font-weight:600;">Platform Optimization</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-warm-rgb),.18);background:rgba(var(--ds-warm-rgb),.06);color:rgba(var(--ds-warm-rgb),.85);font-weight:600;">Cross-Platform</span>
    </div>
  </div>

  <!-- Techland -->
  <div class="tl-entry" style="position:relative;">
    <div style="position:absolute;left:-2.9em;top:.15em;width:1.4em;height:1.4em;border-radius:50%;background:rgba(var(--ds-accent-rgb),.3);display:flex;align-items:center;justify-content:center;">
      <div style="width:.5em;height:.5em;border-radius:50%;background:#fff;"></div>
    </div>
    <div style="font-weight:800;font-size:1.08em;">Techland</div>
    <div style="font-size:.82em;opacity:.55;margin:.15em 0 .5em;">Rendering | Technical Artist</div>
    <div style="font-size:.92em;line-height:1.7;">
      Worked across the full rasterization pipeline — GBuffer generation, lighting, and post-processing — while scaling the renderer to ship on everything from handhelds to high-end PC. Began as a Technical Artist, which built a lasting focus on artist-facing tools, visual debugging, and workflows that keep content creators unblocked.
    </div>
    <div class="tl-tags" style="display:flex;flex-wrap:wrap;gap:.4em;margin-top:.6em;">
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-accent-rgb),.18);background:rgba(var(--ds-accent-rgb),.06);color:rgba(var(--ds-accent-rgb),.85);font-weight:600;">GBuffer Fill</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-accent-rgb),.18);background:rgba(var(--ds-accent-rgb),.06);color:rgba(var(--ds-accent-rgb),.85);font-weight:600;">Lighting</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-highlight-rgb),.18);background:rgba(var(--ds-highlight-rgb),.06);color:rgba(var(--ds-highlight-rgb),.85);font-weight:600;">Post-Processing</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-highlight-rgb),.18);background:rgba(var(--ds-highlight-rgb),.06);color:rgba(var(--ds-highlight-rgb),.85);font-weight:600;">Multi-Platform Scalability</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-warm-rgb),.18);background:rgba(var(--ds-warm-rgb),.06);color:rgba(var(--ds-warm-rgb),.85);font-weight:600;">Artist Tooling</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-warm-rgb),.18);background:rgba(var(--ds-warm-rgb),.06);color:rgba(var(--ds-warm-rgb),.85);font-weight:600;">Visual Debugging</span>
    </div>
  </div>

  <!-- The Farm 51 -->
  <div class="tl-entry" style="position:relative;">
    <div style="position:absolute;left:-2.9em;top:.15em;width:1.4em;height:1.4em;border-radius:50%;background:rgba(var(--ds-accent-rgb),.2);display:flex;align-items:center;justify-content:center;">
      <div style="width:.5em;height:.5em;border-radius:50%;background:#fff;"></div>
    </div>
    <div style="font-weight:800;font-size:1.08em;">The Farm 51</div>
    <div style="font-size:.82em;opacity:.55;margin:.15em 0 .5em;">Technical Artist</div>
    <div style="font-size:.92em;line-height:1.7;">
      Built procedural shader systems for environmental effects — forest fire, stormy ocean, wind-driven foliage — alongside a landscape production pipeline and general-purpose shader library. Owned R&D for simulation systems and drove performance optimization across the board.
    </div>
    <div class="tl-tags" style="display:flex;flex-wrap:wrap;gap:.4em;margin-top:.6em;">
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-accent-rgb),.18);background:rgba(var(--ds-accent-rgb),.06);color:rgba(var(--ds-accent-rgb),.85);font-weight:600;">Procedural Shaders</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-accent-rgb),.18);background:rgba(var(--ds-accent-rgb),.06);color:rgba(var(--ds-accent-rgb),.85);font-weight:600;">Wind Simulation</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-highlight-rgb),.18);background:rgba(var(--ds-highlight-rgb),.06);color:rgba(var(--ds-highlight-rgb),.85);font-weight:600;">Landscape Pipeline</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-highlight-rgb),.18);background:rgba(var(--ds-highlight-rgb),.06);color:rgba(var(--ds-highlight-rgb),.85);font-weight:600;">Ocean Rendering</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-warm-rgb),.18);background:rgba(var(--ds-warm-rgb),.06);color:rgba(var(--ds-warm-rgb),.85);font-weight:600;">R&amp;D</span>
      <span style="font-size:.75em;padding:.2em .55em;border-radius:5px;border:1px solid rgba(var(--ds-warm-rgb),.18);background:rgba(var(--ds-warm-rgb),.06);color:rgba(var(--ds-warm-rgb),.85);font-weight:600;">Optimization</span>
    </div>
  </div>

</div>

---

<div class="about-disclaimer" style="margin-top:1em;padding:.9em 1.2em;border-radius:8px;border:1px solid rgba(var(--ds-accent-rgb),.08);background:rgba(var(--ds-accent-rgb),.02);font-size:.82em;line-height:1.6;opacity:.6;">
<strong>Disclaimer:</strong> All content on this site reflects my personal opinions and experiences only. Nothing here represents the views, positions, or endorsements of any company I currently work for or have previously worked for.
</div>



