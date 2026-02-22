// Frame Graph MVP v3 -- Usage Example
// Compile: g++ -std=c++17 -o example_v3 example_v3.cpp
#include "frame_graph_v3.h"
#include <cstdio>

int main() {
    printf("=== Frame Graph v3: Lifetimes & Memory Aliasing ===\n");

    FrameGraph fg;

    // Import the swapchain backbuffer — externally owned.
    // The graph tracks barriers but won't alias it.
    auto backbuffer = fg.importResource({1920, 1080, Format::RGBA8},
                                        ResourceState::Present);

    auto depth = fg.createResource({1920, 1080, Format::D32F});
    auto gbufA = fg.createResource({1920, 1080, Format::RGBA8});
    auto gbufN = fg.createResource({1920, 1080, Format::RGBA8});
    auto hdr   = fg.createResource({1920, 1080, Format::RGBA16F});
    auto bloom = fg.createResource({960,  540,  Format::RGBA16F});
    auto debug = fg.createResource({1920, 1080, Format::RGBA8});

    fg.addPass("DepthPrepass",
        [&]() { fg.write(0, depth); },
        [&](/*cmd*/) { printf("  >> exec: DepthPrepass\n"); });

    fg.addPass("GBuffer",
        [&]() { fg.read(1, depth); fg.write(1, gbufA); fg.write(1, gbufN); },
        [&](/*cmd*/) { printf("  >> exec: GBuffer\n"); });

    fg.addPass("Lighting",
        [&]() { fg.read(2, gbufA); fg.read(2, gbufN); fg.write(2, hdr); },
        [&](/*cmd*/) { printf("  >> exec: Lighting\n"); });

    fg.addPass("Bloom",
        [&]() { fg.read(3, hdr); fg.write(3, bloom); },
        [&](/*cmd*/) { printf("  >> exec: Bloom\n"); });

    fg.addPass("Tonemap",
        [&]() { fg.read(4, bloom); fg.write(4, hdr); },
        [&](/*cmd*/) { printf("  >> exec: Tonemap\n"); });

    // Present — reads HDR, writes to imported backbuffer.
    fg.addPass("Present",
        [&]() { fg.read(5, hdr); fg.write(5, backbuffer); },
        [&](/*cmd*/) { printf("  >> exec: Present\n"); });

    // Dead pass — nothing reads debug, so the graph will cull it.
    fg.addPass("DebugOverlay",
        [&]() { fg.write(6, debug); },
        [&](/*cmd*/) { printf("  >> exec: DebugOverlay\n"); });

    auto plan = fg.compile();   // topo-sort, cull, alias
    fg.execute(plan);             // barriers + run
    return 0;
}
