// Frame Graph v2 — SSAO Connected (output IS read → pass stays alive)
// Compile: g++ -std=c++17 -o example_v2_ssao_alive example_v2_ssao_alive.cpp frame_graph_v2.cpp
#include "frame_graph_v2.h"
#include <cstdio>

int main() {
    printf("=== v2 Variant A: SSAO Connected ===\n");

    FrameGraph fg;
    auto depth = fg.createResource({1920, 1080, Format::D32F});
    auto gbufA = fg.createResource({1920, 1080, Format::RGBA8});
    auto ssao  = fg.createResource({1920, 1080, Format::R8});
    auto hdr   = fg.createResource({1920, 1080, Format::RGBA16F});

    fg.addPass("DepthPrepass",
        [&]() { fg.write(0, depth); },
        [&](/*cmd*/) { printf("  >> exec: DepthPrepass\n"); });

    fg.addPass("GBuffer",
        [&]() { fg.read(1, depth); fg.write(1, gbufA); },
        [&](/*cmd*/) { printf("  >> exec: GBuffer\n"); });

    fg.addPass("SSAO",
        [&]() { fg.read(2, depth); fg.write(2, ssao); },
        [&](/*cmd*/) { printf("  >> exec: SSAO\n"); });

    fg.addPass("Lighting",
        [&]() { fg.read(3, gbufA); fg.read(3, ssao); fg.write(3, hdr); },
        [&](/*cmd*/) { printf("  >> exec: Lighting\n"); });

    printf("Lighting reads SSAO -> SSAO stays alive.\n");
    fg.execute();
    return 0;
}
