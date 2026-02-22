// Frame Graph MVP v1 -- Usage Example
// Compile: g++ -std=c++17 -o example_v1 example_v1.cpp frame_graph_v1.cpp
#include "frame_graph_v1.h"
#include <cstdio>

int main() {
    printf("=== Frame Graph v1: Declare & Execute ===\n");

    FrameGraph fg;

    // Import the swapchain backbuffer — the graph tracks barriers
    // but does not own its memory (it lives outside the frame).
    auto backbuffer = fg.importResource({1920, 1080, Format::RGBA8});

    auto depth = fg.createResource({1920, 1080, Format::D32F});
    auto gbufA = fg.createResource({1920, 1080, Format::RGBA8});
    auto gbufN = fg.createResource({1920, 1080, Format::RGBA8});
    auto hdr   = fg.createResource({1920, 1080, Format::RGBA16F});

    fg.addPass("DepthPrepass",
        [&]() { /* setup — v1 doesn't use this */ },
        [&](/*cmd*/) { printf("     draw scene depth-only\n"); });

    fg.addPass("GBuffer",
        [&]() { /* setup */ },
        [&](/*cmd*/) { printf("     draw scene -> GBuffer MRTs\n"); });

    fg.addPass("Lighting",
        [&]() { /* setup */ },
        [&](/*cmd*/) { printf("     fullscreen lighting pass\n"); });

    fg.addPass("Present",
        [&]() { /* setup */ },
        [&](/*cmd*/) { printf("     copy HDR -> backbuffer\n"); });

    fg.execute();
    return 0;
}
