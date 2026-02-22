#pragma once
// Frame Graph MVP v1 — Declare & Execute
// No dependency tracking, no barriers, no aliasing.
// Passes execute in declaration order.
//
// Compile: any C++17 compiler (header-only, no GPU backend needed)
//   g++ -std=c++17 -c frame_graph_v1.h
//   or just #include it from your example/test file.

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// ── Resource description (virtual until compile) ──────────────
enum class Format { RGBA8, RGBA16F, R8, D32F };

struct ResourceDesc {
    uint32_t width  = 0;
    uint32_t height = 0;
    Format   format = Format::RGBA8;
};

// Handle = typed index into the graph's resource array.
// No GPU memory behind it yet — just a number.
struct ResourceHandle {
    uint32_t index = UINT32_MAX;
    bool isValid() const { return index != UINT32_MAX; }
};

// ── Render pass ───────────────────────────────────────────────
struct RenderPass {
    std::string                        name;
    std::function<void()>              setup;    // build the DAG (v1: unused)
    std::function<void(/*cmd list*/)>  execute;  // record GPU commands
};

// ── Frame graph ───────────────────────────────────────────────
class FrameGraph {
public:
    // Create a virtual resource — returns a handle, not GPU memory.
    ResourceHandle createResource(const ResourceDesc& desc) {
        resources_.push_back(desc);
        return { static_cast<uint32_t>(resources_.size() - 1) };
    }

    // Import an external resource (e.g. swapchain backbuffer).
    // Barriers are tracked, but the graph does not own its memory.
    ResourceHandle importResource(const ResourceDesc& desc) {
        resources_.push_back(desc);  // v1: same as create (no aliasing yet)
        return { static_cast<uint32_t>(resources_.size() - 1) };
    }

    // Register a pass. Setup runs now; execute is stored for later.
    template <typename SetupFn, typename ExecFn>
    void addPass(const std::string& name, SetupFn&& setup, ExecFn&& exec) {
        passes_.push_back({ name, std::forward<SetupFn>(setup),
                                   std::forward<ExecFn>(exec) });
        passes_.back().setup();  // run setup immediately
    }

    // Compile + execute. v1 is trivial — just run in declaration order.
    void execute() {
        // v1: no compile step — no sorting, no culling, no barriers.
        // Just run every pass in the order it was added.
        printf("\n[1] Executing (declaration order — no compile step):\n");
        for (auto& pass : passes_) {
            printf("  >> exec: %s\n", pass.name.c_str());
            pass.execute(/* &cmdList */);
        }

        // Frame over — clear everything for next frame.
        passes_.clear();
        resources_.clear();
    }

private:
    std::vector<RenderPass>    passes_;
    std::vector<ResourceDesc>  resources_;
};
