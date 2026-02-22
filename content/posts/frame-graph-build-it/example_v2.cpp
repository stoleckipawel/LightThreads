// Frame Graph MVP v2 -- Usage Example
// Compile: g++ -std=c++17 -o example_v2 example_v2.cpp
#include "frame_graph_v2.h"
#include <cstdio>

int main() {
    printf("=== Frame Graph v2: Dependencies & Barriers ===\n");

    FrameGraph fg;

    // Import the swapchain backbuffer — externally owned.
    auto backbuffer = fg.importResource({1920, 1080, Format::RGBA8},
                                        ResourceState::Present);

    auto depth = fg.createResource({1920, 1080, Format::D32F});
    auto gbufA = fg.createResource({1920, 1080, Format::RGBA8});
    auto hdr   = fg.createResource({1920, 1080, Format::RGBA16F});
    auto debug = fg.createResource({1920, 1080, Format::RGBA8});

    fg.addPass("DepthPrepass",
        [&]() { fg.write(0, depth); },
        [&](/*cmd*/) { printf("  >> exec: DepthPrepass\n"); });

    fg.addPass("GBuffer",
        [&]() { fg.read(1, depth); fg.write(1, gbufA); },
        [&](/*cmd*/) { printf("  >> exec: GBuffer\n"); });

    fg.addPass("Lighting",
        [&]() { fg.read(2, gbufA); fg.write(2, hdr); },
        [&](/*cmd*/) { printf("  >> exec: Lighting\n"); });

    // Present — writes to the imported backbuffer.
    fg.addPass("Present",
        [&]() { fg.read(3, hdr); fg.write(3, backbuffer); },
        [&](/*cmd*/) { printf("  >> exec: Present\n"); });

    // Dead pass — nothing reads debug, so the graph will cull it.
    fg.addPass("DebugOverlay",
        [&]() { fg.write(4, debug); },
        [&](/*cmd*/) { printf("  >> exec: DebugOverlay\n"); });

    fg.execute();
    return 0;
}
