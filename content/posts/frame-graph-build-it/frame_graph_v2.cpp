#include "frame_graph_v2.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <queue>
#include <unordered_set>

// == FrameGraph implementation =================================

ResourceHandle FrameGraph::createResource(const ResourceDesc& desc) {
    entries.push_back({ desc, {{}}, ResourceState::Undefined, false });
    return { static_cast<uint32_t>(entries.size() - 1) };
}

ResourceHandle FrameGraph::importResource(const ResourceDesc& desc,
                                          ResourceState initialState) {
    entries.push_back({ desc, {{}}, initialState, true });
    return { static_cast<uint32_t>(entries.size() - 1) };
}

void FrameGraph::read(uint32_t passIdx, ResourceHandle h) {
    auto& ver = entries[h.index].versions.back();
    if (ver.writerPass != UINT32_MAX) {
        passes[passIdx].dependsOn.push_back(ver.writerPass);
    }
    ver.readerPasses.push_back(passIdx);
    passes[passIdx].reads.push_back(h);
}

void FrameGraph::write(uint32_t passIdx, ResourceHandle h) {
    entries[h.index].versions.push_back({});
    entries[h.index].versions.back().writerPass = passIdx;
    passes[passIdx].writes.push_back(h);
}

void FrameGraph::execute() {
    printf("\n[1] Building dependency edges...\n");
    buildEdges();
    printf("[2] Topological sort...\n");
    auto sorted = topoSort();
    printf("[3] Culling dead passes...\n");
    cull(sorted);

    printf("[4] Executing (with automatic barriers):\n");
    for (uint32_t idx : sorted) {
        if (!passes[idx].alive) {
            printf("  -- skip: %s (CULLED)\n", passes[idx].name.c_str());
            continue;
        }
        insertBarriers(idx);
        passes[idx].execute(/* &cmdList */);
    }
    passes.clear();
    entries.clear();
}

// == Build dependency edges ====================================

void FrameGraph::buildEdges() {
    for (uint32_t i = 0; i < passes.size(); i++) {
        // Deduplicate dependency edges and build successor list.
        std::unordered_set<uint32_t> seen;
        for (uint32_t dep : passes[i].dependsOn) {
            if (seen.insert(dep).second) {
                passes[dep].successors.push_back(i);
                passes[i].inDegree++;
            }
        }
    }
}

// == Kahn's topological sort â€” O(V + E) ========================

std::vector<uint32_t> FrameGraph::topoSort() {
    std::queue<uint32_t> q;
    std::vector<uint32_t> inDeg(passes.size());
    for (uint32_t i = 0; i < passes.size(); i++) {
        inDeg[i] = passes[i].inDegree;
        if (inDeg[i] == 0) q.push(i);
    }
    std::vector<uint32_t> order;
    while (!q.empty()) {
        uint32_t cur = q.front(); q.pop();
        order.push_back(cur);
        // Walk the adjacency list â€” O(E) total across all nodes.
        for (uint32_t succ : passes[cur].successors) {
            if (--inDeg[succ] == 0)
                q.push(succ);
        }
    }
    assert(order.size() == passes.size() && "Cycle detected!");
    printf("  Topological order: ");
    for (uint32_t i = 0; i < order.size(); i++) {
        printf("%s%s", passes[order[i]].name.c_str(),
               i + 1 < order.size() ? " -> " : "\n");
    }
    return order;
}

// == Cull dead passes (backward walk from output) ==============

void FrameGraph::cull(const std::vector<uint32_t>& sorted) {
    // Mark the last pass (present) as alive, then walk backward.
    if (sorted.empty()) return;
    passes[sorted.back()].alive = true;
    for (int i = static_cast<int>(sorted.size()) - 1; i >= 0; i--) {
        if (!passes[sorted[i]].alive) continue;
        for (uint32_t dep : passes[sorted[i]].dependsOn)
            passes[dep].alive = true;
    }
    printf("  Culling result:   ");
    for (uint32_t i = 0; i < passes.size(); i++) {
        printf("%s=%s%s", passes[i].name.c_str(),
               passes[i].alive ? "ALIVE" : "DEAD",
               i + 1 < passes.size() ? ", " : "\n");
    }
}

// == Insert barriers where resource state changes ==============

void FrameGraph::insertBarriers(uint32_t passIdx) {
    auto stateForUsage = [](bool isWrite, Format fmt) {
        if (isWrite)
            return (fmt == Format::D32F) ? ResourceState::DepthAttachment
                                         : ResourceState::ColorAttachment;
        return ResourceState::ShaderRead;
    };

    for (auto& h : passes[passIdx].reads) {
        ResourceState needed = ResourceState::ShaderRead;
        if (entries[h.index].currentState != needed) {
            printf("    barrier: resource[%u] %s -> %s\n",
                   h.index,
                   stateName(entries[h.index].currentState),
                   stateName(needed));
            entries[h.index].currentState = needed;
        }
    }
    for (auto& h : passes[passIdx].writes) {
        ResourceState needed = stateForUsage(true, entries[h.index].desc.format);
        if (entries[h.index].currentState != needed) {
            printf("    barrier: resource[%u] %s -> %s\n",
                   h.index,
                   stateName(entries[h.index].currentState),
                   stateName(needed));
            entries[h.index].currentState = needed;
        }
    }
}
