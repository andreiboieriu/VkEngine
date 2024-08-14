#pragma once

#include "volk.h"
#include "vk_types.h"
#include <span>

#include "spirv_reflect.h"

constexpr int MAX_PUSH_CONSTANTS_SIZE = 128;

class ComputeEffect {

public:
    struct PushConstantInfo {
        uint32_t offset;
        uint32_t size;
        std::string name;
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

    struct Subpass {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSetLayout descriptorSetLayout;
        
        uint32_t pushConstantsSize;
        char pushConstants[MAX_PUSH_CONSTANTS_SIZE];
        std::vector<PushConstantInfo> pushInfos;
    };

    ComputeEffect(std::string name);
    ComputeEffect(std::string name, std::span<PushConstantData> defaultPushConstants);

    ~ComputeEffect();

    virtual void execute(VkCommandBuffer commandBuffer, const AllocatedImage& image, VkExtent2D extent, bool synchronize);
    virtual void drawGui();

protected:
    void createPipelineLayout();
    void addSubpass();
    void synchronizeWithCompute(VkCommandBuffer commandBuffer);

    void load(std::string name);
    void setDefaultPushConstants(std::span<PushConstantData> defaultPushConstants);
    Subpass createSubpass(std::string path);

    void freeResources();

    std::vector<Subpass> mSubpasses;

    std::string mName;
    bool mEnabled;
};
