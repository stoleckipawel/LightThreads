#pragma once
// Frame Graph MVP v2 â€” Dependencies & Barriers
// Adds: resource versioning, DAG with adjacency list, Kahn's topo-sort,
//       pass culling, and automatic barrier insertion.
//
// Compile: g++ -std=c++17 -o example_v2 example_v2.cpp frame_graph_v2.cpp

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// == Resource description (virtual until compile) ==============
enum class Format { RGBA8, RGBA16F, R8, D32F };

struct ResourceDesc {
    uint32_t width  = 0;
    uint32_t height = 0;
    Format   format = Format::RGBA8;
};

struct ResourceHandle {
    uint32_t index = UINT32_MAX;
    bool isValid() const { return index != UINT32_MAX; }
};

// == Resource state tracking (NEW v2) ==========================
enum class ResourceState { Undefined, ColorAttachment, DepthAttachment,
                           ShaderRead, Present };

inline const char* stateName(ResourceState s) {
    switch (s) {
        case ResourceState::Undefined:       return "Undefined";
        case ResourceState::ColorAttachment: return "ColorAttachment";
        case ResourceState::DepthAttachment: return "DepthAttachment";
        case ResourceState::ShaderRead:      return "ShaderRead";
        case ResourceState::Present:         return "Present";
        default:                             return "?";
    }
}

struct ResourceVersion {                 // NEW v2
    uint32_t writerPass = UINT32_MAX;    // which pass wrote this version
    std::vector<uint32_t> readerPasses;  // which passes read it
};

// Extend ResourceDesc with tracking:
struct ResourceEntry {
    ResourceDesc desc;
    std::vector<ResourceVersion> versions;  // version 0, 1, 2...
    ResourceState currentState = ResourceState::Undefined;
    bool imported = false;   // imported resources are not owned by the graph
};

// == Updated render pass =======================================
struct RenderPass {
    std::string name;
    std::function<void()>             setup;
    std::function<void(/*cmd list*/)> execute;

    std::vector<ResourceHandle> reads;    // NEW v2
    std::vector<ResourceHandle> writes;   // NEW v2
    std::vector<uint32_t> dependsOn;      // NEW v2 â€” passes this pass depends on
    std::vector<uint32_t> successors;     // NEW v2 â€” passes that depend on this pass
    uint32_t inDegree = 0;                // NEW v2 â€” for Kahn's
    bool     alive    = false;            // NEW v2 â€” for culling
};

// == Updated FrameGraph ========================================
class FrameGraph {
public:
    ResourceHandle createResource(const ResourceDesc& desc);

    // Import an external resource (e.g. swapchain backbuffer).
    // The graph tracks barriers but does not own or alias its memory.
    ResourceHandle importResource(const ResourceDesc& desc,
                                  ResourceState initialState = ResourceState::Undefined);

    // Declare a read â€” links this pass to the resource's current version.
    void read(uint32_t passIdx, ResourceHandle h);    // NEW v2

    // Declare a write â€” creates a new version of the resource.
    void write(uint32_t passIdx, ResourceHandle h);   // NEW v2

    template <typename SetupFn, typename ExecFn>
    void addPass(const std::string& name, SetupFn&& setup, ExecFn&& exec) {
        uint32_t idx = static_cast<uint32_t>(passes.size());
        passes.push_back({ name, std::forward<SetupFn>(setup),
                                   std::forward<ExecFn>(exec) });
        currentPass = idx;   // NEW v2 â€” so setup can call read()/write()
        passes.back().setup();
    }

    void execute();

private:
    uint32_t currentPass = 0;
    std::vector<RenderPass>    passes;
    std::vector<ResourceEntry> entries;

    void buildEdges();                                // NEW v2
    std::vector<uint32_t> topoSort();                 // NEW v2
    void cull(const std::vector<uint32_t>& sorted);   // NEW v2
    void insertBarriers(uint32_t passIdx);             // NEW v2
};
