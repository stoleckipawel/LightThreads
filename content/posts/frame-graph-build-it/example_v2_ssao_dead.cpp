// Frame Graph v2 — SSAO Disconnected (output NOT read → pass gets culled)
// Compile: g++ -std=c++17 -o example_v2_ssao_dead example_v2_ssao_dead.cpp frame_graph_v2.cpp
#include "frame_graph_v2.h"
#include <cstdio>

int main() {
    printf("=== v2 Variant B: SSAO Disconnected ===\n");

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

    // Lighting does NOT read ssao — only reads gbufA
    fg.addPass("Lighting",
        [&]() { fg.read(3, gbufA); fg.write(3, hdr); },
        [&](/*cmd*/) { printf("  >> exec: Lighting\n"); });

    printf("Lighting does NOT read SSAO -> SSAO gets culled.\n");
    fg.execute();
    return 0;
}
