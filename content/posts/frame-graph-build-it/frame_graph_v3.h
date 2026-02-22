#pragma once
// Frame Graph MVP v3 â€” Lifetimes & Aliasing
// Adds: lifetime analysis, greedy free-list memory aliasing.
// Builds on v2 (dependencies, topo-sort, culling, barriers).
//
// Compile: g++ -std=c++17 -o example_v3 example_v3.cpp frame_graph_v3.cpp

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

// == Resource state tracking ===================================
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

struct ResourceVersion {
    uint32_t writerPass = UINT32_MAX;
    std::vector<uint32_t> readerPasses;
};

struct ResourceEntry {
    ResourceDesc desc;
    std::vector<ResourceVersion> versions;
    ResourceState currentState = ResourceState::Undefined;
    bool imported = false;   // imported resources are not owned by the graph
};

// == Physical memory block (NEW v3) ============================
struct PhysicalBlock {
    uint32_t sizeBytes   = 0;
    uint32_t availAfter  = 0;  // pass index after which this block is free
};

// == Bytes-per-pixel helper (NEW v3) ===========================
inline uint32_t bytesPerPixel(Format fmt) {
    switch (fmt) {
        case Format::R8:      return 1;
        case Format::RGBA8:   return 4;
        case Format::D32F:    return 4;
        case Format::RGBA16F: return 8;
        default:              return 4;
    }
}

// == Lifetime info per resource (NEW v3) =======================
struct Lifetime {
    uint32_t firstUse = UINT32_MAX;
    uint32_t lastUse  = 0;
    bool     isTransient = true;
};

// == Render pass ===============================================
struct RenderPass {
    std::string name;
    std::function<void()>             setup;
    std::function<void(/*cmd list*/)> execute;

    std::vector<ResourceHandle> reads;
    std::vector<ResourceHandle> writes;
    std::vector<uint32_t> dependsOn;
    std::vector<uint32_t> successors;
    uint32_t inDegree = 0;
    bool     alive    = false;
};

// == Frame graph (v3: full MVP) ================================
class FrameGraph {
public:
    ResourceHandle createResource(const ResourceDesc& desc);
    ResourceHandle importResource(const ResourceDesc& desc,
                                  ResourceState initialState = ResourceState::Undefined);

    void read(uint32_t passIdx, ResourceHandle h);
    void write(uint32_t passIdx, ResourceHandle h);

    template <typename SetupFn, typename ExecFn>
    void addPass(const std::string& name, SetupFn&& setup, ExecFn&& exec) {
        uint32_t idx = static_cast<uint32_t>(passes.size());
        passes.push_back({ name, std::forward<SetupFn>(setup),
                                   std::forward<ExecFn>(exec) });
        currentPass = idx;
        passes.back().setup();
    }

    // == v3: compile â€” builds the execution plan + allocates memory ==
    struct CompiledPlan {
        std::vector<uint32_t> sorted;
        std::vector<uint32_t> mapping;   // mapping[virtualIdx] â†’ physicalBlock
    };

    CompiledPlan compile();

    // == v3: execute â€” runs the compiled plan =================
    void execute(const CompiledPlan& plan);

    // convenience: compile + execute in one call
    void execute();

private:
    uint32_t currentPass = 0;
    std::vector<RenderPass>    passes;
    std::vector<ResourceEntry> entries;

    void buildEdges();
    std::vector<uint32_t> topoSort();
    void cull(const std::vector<uint32_t>& sorted);
    void insertBarriers(uint32_t passIdx);
    std::vector<Lifetime> scanLifetimes(const std::vector<uint32_t>& sorted);  // NEW v3
    std::vector<uint32_t> aliasResources(const std::vector<Lifetime>& lifetimes); // NEW v3
};
