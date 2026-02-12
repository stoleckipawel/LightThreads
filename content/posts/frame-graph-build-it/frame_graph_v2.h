#pragma once
// Frame Graph MVP v2 — Dependencies & Barriers
// Adds: resource versioning, DAG with adjacency list, Kahn's topo-sort,
//       pass culling, and automatic barrier insertion.
//
// Compile: any C++17 compiler (header-only, no GPU backend needed)
//   g++ -std=c++17 -c frame_graph_v2.h
//   or just #include it from your example/test file.

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

// ── Resource description (virtual until compile) ──────────────
enum class Format { RGBA8, RGBA16F, R8, D32F };

struct ResourceDesc {
    uint32_t width  = 0;
    uint32_t height = 0;
    Format   format = Format::RGBA8;
};

struct ResourceHandle {
    uint32_t index = UINT32_MAX;
    bool isValid() const { return index != UINT32_MAX; }
};

// ── Resource state tracking (NEW v2) ──────────────────────────
enum class ResourceState { Undefined, ColorAttachment, DepthAttachment,
                           ShaderRead, Present };

inline const char* stateName(ResourceState s) {
    switch (s) {
        case ResourceState::Undefined:       return "Undefined";
        case ResourceState::ColorAttachment: return "ColorAttachment";
        case ResourceState::DepthAttachment: return "DepthAttachment";
        case ResourceState::ShaderRead:      return "ShaderRead";
        case ResourceState::Present:         return "Present";
        default:                             return "?";
    }
}

struct ResourceVersion {                 // NEW v2
    uint32_t writerPass = UINT32_MAX;    // which pass wrote this version
    std::vector<uint32_t> readerPasses;  // which passes read it
};

// Extend ResourceDesc with tracking:
struct ResourceEntry {
    ResourceDesc desc;
    std::vector<ResourceVersion> versions;  // version 0, 1, 2...
    ResourceState currentState = ResourceState::Undefined;
};

// ── Updated render pass ───────────────────────────────────────
struct RenderPass {
    std::string name;
    std::function<void()>             setup;
    std::function<void(/*cmd list*/)> execute;

    std::vector<ResourceHandle> reads;    // NEW v2
    std::vector<ResourceHandle> writes;   // NEW v2
    std::vector<uint32_t> dependsOn;      // NEW v2 — passes this pass depends on
    std::vector<uint32_t> successors;     // NEW v2 — passes that depend on this pass
    uint32_t inDegree = 0;                // NEW v2 — for Kahn's
    bool     alive    = false;            // NEW v2 — for culling
};

// ── Updated FrameGraph ────────────────────────────────────────
class FrameGraph {
public:
    ResourceHandle createResource(const ResourceDesc& desc) {
        entries_.push_back({ desc, {{}}, ResourceState::Undefined });
        return { static_cast<uint32_t>(entries_.size() - 1) };
    }

    // Declare a read — links this pass to the resource's current version.
    void read(uint32_t passIdx, ResourceHandle h) {    // NEW v2
        auto& ver = entries_[h.index].versions.back();
        if (ver.writerPass != UINT32_MAX) {
            passes_[passIdx].dependsOn.push_back(ver.writerPass);
        }
        ver.readerPasses.push_back(passIdx);
        passes_[passIdx].reads.push_back(h);
    }

    // Declare a write — creates a new version of the resource.
    void write(uint32_t passIdx, ResourceHandle h) {   // NEW v2
        entries_[h.index].versions.push_back({});
        entries_[h.index].versions.back().writerPass = passIdx;
        passes_[passIdx].writes.push_back(h);
    }

    template <typename SetupFn, typename ExecFn>
    void addPass(const std::string& name, SetupFn&& setup, ExecFn&& exec) {
        uint32_t idx = static_cast<uint32_t>(passes_.size());
        passes_.push_back({ name, std::forward<SetupFn>(setup),
                                   std::forward<ExecFn>(exec) });
        currentPass_ = idx;   // NEW v2 — so setup can call read()/write()
        passes_.back().setup();
    }

    void execute() {
        printf("\n[1] Building dependency edges...\n");
        buildEdges();        // NEW v2
        printf("[2] Topological sort...\n");
        auto sorted = topoSort();        // NEW v2
        printf("[3] Culling dead passes...\n");
        cull(sorted);        // NEW v2

        printf("[4] Executing (with automatic barriers):\n");
        for (uint32_t idx : sorted) {
            if (!passes_[idx].alive) {
                printf("  -- skip: %s (CULLED)\n", passes_[idx].name.c_str());
                continue;
            }
            insertBarriers(idx);                 // NEW v2
            passes_[idx].execute(/* &cmdList */);
        }
        passes_.clear();
        entries_.clear();
    }

private:
    uint32_t currentPass_ = 0;
    std::vector<RenderPass>    passes_;
    std::vector<ResourceEntry> entries_;

    // ── Build dependency edges ────────────────────────────────
    void buildEdges() {                              // NEW v2
        for (uint32_t i = 0; i < passes_.size(); i++) {
            // Deduplicate dependency edges and build successor list.
            std::unordered_set<uint32_t> seen;
            for (uint32_t dep : passes_[i].dependsOn) {
                if (seen.insert(dep).second) {
                    passes_[dep].successors.push_back(i);
                    passes_[i].inDegree++;
                }
            }
        }
    }

    // ── Kahn's topological sort — O(V + E) ────────────────────
    std::vector<uint32_t> topoSort() {               // NEW v2
        std::queue<uint32_t> q;
        std::vector<uint32_t> inDeg(passes_.size());
        for (uint32_t i = 0; i < passes_.size(); i++) {
            inDeg[i] = passes_[i].inDegree;
            if (inDeg[i] == 0) q.push(i);
        }
        std::vector<uint32_t> order;
        while (!q.empty()) {
            uint32_t cur = q.front(); q.pop();
            order.push_back(cur);
            // Walk the adjacency list — O(E) total across all nodes.
            for (uint32_t succ : passes_[cur].successors) {
                if (--inDeg[succ] == 0)
                    q.push(succ);
            }
        }
        assert(order.size() == passes_.size() && "Cycle detected!");
        printf("  Topological order: ");
        for (uint32_t i = 0; i < order.size(); i++) {
            printf("%s%s", passes_[order[i]].name.c_str(),
                   i + 1 < order.size() ? " -> " : "\n");
        }
        return order;
    }

    // ── Cull dead passes (backward walk from output) ──────────
    void cull(const std::vector<uint32_t>& sorted) { // NEW v2
        // Mark the last pass (present) as alive, then walk backward.
        if (sorted.empty()) return;
        passes_[sorted.back()].alive = true;
        for (int i = static_cast<int>(sorted.size()) - 1; i >= 0; i--) {
            if (!passes_[sorted[i]].alive) continue;
            for (uint32_t dep : passes_[sorted[i]].dependsOn)
                passes_[dep].alive = true;
        }
        printf("  Culling result:   ");
        for (uint32_t i = 0; i < passes_.size(); i++) {
            printf("%s=%s%s", passes_[i].name.c_str(),
                   passes_[i].alive ? "ALIVE" : "DEAD",
                   i + 1 < passes_.size() ? ", " : "\n");
        }
    }

    // ── Insert barriers where resource state changes ──────────
    void insertBarriers(uint32_t passIdx) {          // NEW v2
        auto stateForUsage = [](bool isWrite, Format fmt) {
            if (isWrite)
                return (fmt == Format::D32F) ? ResourceState::DepthAttachment
                                             : ResourceState::ColorAttachment;
            return ResourceState::ShaderRead;
        };

        for (auto& h : passes_[passIdx].reads) {
            ResourceState needed = ResourceState::ShaderRead;
            if (entries_[h.index].currentState != needed) {
                printf("    barrier: resource[%u] %s -> %s\n",
                       h.index,
                       stateName(entries_[h.index].currentState),
                       stateName(needed));
                entries_[h.index].currentState = needed;
            }
        }
        for (auto& h : passes_[passIdx].writes) {
            ResourceState needed = stateForUsage(true, entries_[h.index].desc.format);
            if (entries_[h.index].currentState != needed) {
                printf("    barrier: resource[%u] %s -> %s\n",
                       h.index,
                       stateName(entries_[h.index].currentState),
                       stateName(needed));
                entries_[h.index].currentState = needed;
            }
        }
    }
};
