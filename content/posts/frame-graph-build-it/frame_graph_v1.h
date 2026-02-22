#pragma once
// Frame Graph MVP v1 -- Declare & Execute
// No dependency tracking, no barriers, no aliasing.
// Passes execute in declaration order.
//
// Compile: g++ -std=c++17 -o example_v1 example_v1.cpp frame_graph_v1.cpp

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// â”€â”€ Resource description (virtual until compile) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
enum class Format { RGBA8, RGBA16F, R8, D32F };

struct ResourceDesc {
    uint32_t width  = 0;
    uint32_t height = 0;
    Format   format = Format::RGBA8;
};

// Handle = typed index into the graph's resource array.
// No GPU memory behind it yet -- just a number.
struct ResourceHandle {
    uint32_t index = UINT32_MAX;
    bool isValid() const { return index != UINT32_MAX; }
};

// â”€â”€ Render pass â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
struct RenderPass {
    std::string                        name;
    std::function<void()>              setup;    // build the DAG (v1: unused)
    std::function<void(/*cmd list*/)>  execute;  // record GPU commands
};

// â”€â”€ Frame graph â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
class FrameGraph {
public:
    // Create a virtual resource -- returns a handle, not GPU memory.
    ResourceHandle createResource(const ResourceDesc& desc);

    // Import an external resource (e.g. swapchain backbuffer).
    // Barriers are tracked, but the graph does not own its memory.
    ResourceHandle importResource(const ResourceDesc& desc);

    // Register a pass. Setup runs now; execute is stored for later.
    template <typename SetupFn, typename ExecFn>
    void addPass(const std::string& name, SetupFn&& setup, ExecFn&& exec) {
        passes.push_back({ name, std::forward<SetupFn>(setup),
                                   std::forward<ExecFn>(exec) });
        passes.back().setup();  // run setup immediately
    }

    // Compile + execute. v1 is trivial -- just run in declaration order.
    void execute();

private:
    std::vector<RenderPass>    passes;
    std::vector<ResourceDesc>  resources;
};
