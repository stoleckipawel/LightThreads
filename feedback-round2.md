# Feedback Round 2 — Combined Observations & Issues

## Reader Feedback (from friend)

| # | Observation | Quote | Severity |
|---|-------------|-------|----------|
| O1 | Structure inverts the promise — tagline says What→Why but article delivers Why→What→How | *"What-How-Why, ale dokument ma strukturę Why-What-How"* | High — structural |
| O2 | Core Idea buried as section 3; should lead so newcomers know what a frame graph *is* before being told to want one | *"Core Idea powinno być pierwszą sekcją"* | High — structural |
| O3 | Inline justifications missing throughout Declare/Compile/Execute; motivation only appears 200 lines later in The Payoff | *"nie mówisz dlaczego — dopiero w rozdziale The Payoff"* | High — readability |
| O4 | WHY→HOW→WHY→HOW zigzag (Problem → Lifecycle → Payoff → Advanced) creates confusion | *"czuję lekki chaos"* | Medium — flow |
| O5 | Clinical, emotionless tone — reads like documentation, not a blog post from someone who lived the pain | *"AI'owy, klinicznie, bez emocji"* | Low–Medium — voice |

---

## Critical Issues (fix before submission)

| # | Issue | Location | Severity | Effort |
|---|-------|----------|----------|--------|
| C1 | "Three things happen" but compile diagram shows four stages (SCHEDULE → ALLOCATE → SYNCHRONIZE → BACK RESOURCES) | Theory, Compile intro (~L285) | Critical — factually wrong | 1 min |
| C2 | Transient resource card says "physically backed at execute" — contradicts compile change | Theory, Declare section (Transient card) | Critical — contradicts Feedback 2 | 1 min |
| C3 | Build It phase diagram says "Aliasing **planned** here" — should say aliasing and allocation happen here | Build It, 3-column declaration/compile/execute diagram | Critical — same contradiction | 1 min |
| C4 | `compile()` returns `{ sorted, mapping }` but `execute()` never references `plan.mapping` — reader asks "we computed aliasing but never applied it?" | Build It, `frame_graph_v3.h` execute() | High — confusing code | 2 min |

---

## Structural / Flow Issues

| # | Issue | Location | Severity | Effort |
|---|-------|----------|----------|--------|
| S1 | Theory is 713 lines — Advanced Features (~300 lines, nearly half) covers features the MVP doesn't implement. Consider moving to Part III or adding a signpost ("skip to Part II — these are covered again in Part III") | Theory, Advanced Features section | Medium — reader retention | 5 min (signpost) |
| S2 | Production article is shortest (267 lines) and thinnest — Frostbite column often just "described in GDC talk"; either drop Frostbite comparison or add substance (e.g. pseudocode for pass merging eligibility) | Production, throughout | Medium — depth | 15 min |
| S3 | No TL;DR or key takeaways in any article — for 700+ line technical content, a 3-5 bullet summary at the end helps retention and shareability | All three articles | Low — nice to have | 10 min |

---

## Content Gaps

| # | Issue | Location | Severity | Effort |
|---|-------|----------|----------|--------|
| G1 | No concrete hidden-dependency bug example — Theory warns abstractly but never shows a before/after code snippet of what goes wrong when you miss a `read()` call | Theory, hidden-dependency warning | High — misses depth | 10 min |
| G2 | Build It has no trade-off warning — someone reading only Part II (common) gets no warning about hidden-dependency risks discussed in Part I | Build It, after v2 barrier section | High — incomplete coverage | 5 min |
| G3 | No debugging / validation tooling section in Build It — team feedback said "asserts aren't sufficient"; show how to detect missing read/write, cycles, print dependency graph | Build It, after v2 | Medium — team asked for this | 15 min |

---

## Polish / Consistency Issues

| # | Issue | Location | Severity | Effort |
|---|-------|----------|----------|--------|
| P1 | Inconsistent emoji usage — Compile diagram uses plain text labels while Problem and Payoff sections use emoji-heavy headers | Theory, throughout | Low | 5 min |
| P2 | Payoff table shows "Async compute: Compiler schedules across queues" but MVP doesn't implement it — oversells; add "(Part III)" qualifier | Theory, Payoff table | Low–Medium | 1 min |
| P3 | Interactive pipeline explorer has no color legend (green=alive, gray=culled, amber=output, purple=async) | Theory, capstone widget | Medium — usability | 10 min |
| P4 | Barrier computation shown as O(V) but is technically O(V+E) since edges are walked per pass | Build It, compile cost table | Low | 1 min |

---

## Writing Quality

| # | Issue | Location | Severity | Effort |
|---|-------|----------|----------|--------|
| W1 | Execute section repeats the same idea twice in 4 lines ("Every decision has already been made…" then "Each execute lambda sees a fully resolved environment…") — pick the stronger one | Theory, Execute section | Low | 1 min |
| W2 | "The graph doesn't care about your rendering strategy" — strong closing line buried after a dense table and before nav link; deserves more breathing room, pull up as section opener | Build It, closing | Low | 2 min |

---

## Priority Order

| Priority | ID | Issue | Effort |
|----------|----|-------|--------|
| 🔴 Fix now | C1 | "Three things happen" → four | 1 min |
| 🔴 Fix now | C2 | "physically backed at execute" | 1 min |
| 🔴 Fix now | C3 | "Aliasing planned here" in Build It | 1 min |
| 🔴 Fix now | C4 | `plan.mapping` unused in execute | 2 min |
| 🟠 Before submission | O1+O2+O4 | Restructure Theory: Core Idea first, inline motivations, flatten WHY→HOW zigzag | 30 min |
| 🟠 Before submission | O3 | Add inline "because" justifications throughout Declare/Compile/Execute | 15 min |
| 🟠 Before submission | G1 | Add concrete hidden-dependency bug example | 10 min |
| 🟠 Before submission | G2 | Add trade-off warning in Build It | 5 min |
| 🟡 Should do | O5 | Warm up tone — add personal voice, anecdotes | 20 min |
| 🟡 Should do | G3 | Add validation/debugging section in Build It | 15 min |
| 🟡 Should do | S1 | Add signpost before Advanced Features | 5 min |
| 🟡 Should do | P3 | Add legend to pipeline explorer | 10 min |
| 🟢 Nice to have | S3 | Add key takeaways to all 3 articles | 10 min |
| 🟢 Nice to have | P2 | Add "(Part III)" to async row in Payoff | 1 min |
| 🟢 Nice to have | P4 | Fix O(V) → O(V+E) | 1 min |
| 🟢 Nice to have | W1 | Remove redundant execute description | 1 min |
| 🟢 Nice to have | W2 | Move closing line to section opener | 2 min |
| 🟢 Nice to have | S2 | Flesh out Production / Frostbite coverage | 15 min |
| 🟢 Nice to have | P1 | Normalize emoji usage | 5 min |
