// Frame Graph — API demo (4 passes, imported backbuffer)
#include "frame_graph_v3.h"

int main() {
    FrameGraph fg;

    // [1] Declare — describe resources and register passes

    // Import the swapchain backbuffer — externally owned, not aliased.
    auto backbuffer = fg.importResource({1920, 1080, Format::RGBA8},
                                        ResourceState::Present);

    // Transient resources — graph-owned, eligible for aliasing.
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

    fg.addPass("Present",
        [&]() { fg.read(3, hdr); fg.write(3, backbuffer); },
        [&](/*cmd*/) { /* copy to backbuffer, present */ });

    // [2] Compile — topo-sort, cull, emit barriers, alias memory
    auto plan = fg.compile();

    // [3] Execute — run each pass in compiled order
    fg.execute(plan);
}
