#pragma once

#include "volk.h"
#include "vk_types.h"
#include <span>

#include "spirv_reflect.h"

constexpr int MAX_PUSH_CONSTANTS_SIZE = 128;

class ComputeEffect {

public:
    struct PushConstantLocation {
        uint32_t subpass;
        uint32_t offset;
    };

    struct PushConstantInfo {
        // uint32_t offset;
        uint32_t size;
        std::vector<PushConstantLocation> locations;
        SpvReflectTypeFlags type;
    };

    struct PushConstantData {
        std::string name;
        uint32_t size;
        char value[16];

        PushConstantData(std::string name, float value) : name{name} {
            *(float*)this->value = value;
            size = sizeof(float);
        }

        PushConstantData(std::string name, int value) : name{name} {
            *(int*)this->value = value;
            size = sizeof(int);
        }
    };

    struct BindingData {
        std::string name;
        VkDescriptorType descriptorType;
    };

    struct Subpass {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSetLayout descriptorSetLayout;
        
        uint32_t pushConstantsSize;
        char pushConstants[MAX_PUSH_CONSTANTS_SIZE];

        std::vector<BindingData> bindings;
    };



    ComputeEffect(std::string name);
    ComputeEffect(std::string name, std::span<PushConstantData> defaultPushConstants);

    ~ComputeEffect();

    virtual void execute(VkCommandBuffer commandBuffer, const AllocatedImage& image, VkExtent2D extent, bool synchronize);
    virtual void drawGui();

protected:
    void createPipelineLayout();
    void synchronizeWithCompute(VkCommandBuffer commandBuffer);

    void load(std::string name);
    void setDefaultPushConstants(std::span<PushConstantData> defaultPushConstants);
    void addSubpass(std::string path);

    template<typename T>
    void writePushConstant(const std::string& name, T value) {
        if (!mPushInfoMap.contains(name)) {
            return;
        }

        PushConstantInfo& pushInfo = mPushInfoMap[name];

        if (sizeof(T) != pushInfo.size) {
            fmt::println("write push constant failed: size mismatch");
            return;
        }

        for (auto& location : pushInfo.locations) {
            *(T*)getAddr(location) = value;
        }
    }
    
    void writePushConstant(const std::string& name, void* addr, size_t size);

    void freeResources();

    void* getAddr(PushConstantLocation pushLocation) {
        return mSubpasses[pushLocation.subpass].pushConstants + pushLocation.offset;
    }

    std::vector<Subpass> mSubpasses;

    // TODO: improve later
    bool mUseBufferImage = false;
    AllocatedImage mBufferImage;

    std::vector<VkImageView> mBufferImageMipViews;

    std::unordered_map<std::string, PushConstantInfo> mPushInfoMap;

    std::string mName;
    bool mEnabled = true;
};
