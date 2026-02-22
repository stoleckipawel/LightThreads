#include "frame_graph_v1.h"
#include <cstdio>

// == FrameGraph implementation =================================

ResourceHandle FrameGraph::createResource(const ResourceDesc& desc) {
    resources.push_back(desc);
    return { static_cast<uint32_t>(resources.size() - 1) };
}

ResourceHandle FrameGraph::importResource(const ResourceDesc& desc) {
    resources.push_back(desc);  // v1: same as create (no aliasing yet)
    return { static_cast<uint32_t>(resources.size() - 1) };
}

void FrameGraph::execute() {
    // v1: no compile step -- no sorting, no culling, no barriers.
    // Just run every pass in the order it was added.
    printf("\n[1] Executing (declaration order -- no compile step):\n");
    for (auto& pass : passes) {
        printf("  >> exec: %s\n", pass.name.c_str());
        pass.execute(/* &cmdList */);
    }

    // Frame over -- clear everything for next frame.
    passes.clear();
    resources.clear();
}
