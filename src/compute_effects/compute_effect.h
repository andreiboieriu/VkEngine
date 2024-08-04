#pragma once

#include "volk.h"

// struct ComputeEffect {
//     std::string name;

//     VkPipelineLayout pipelineLayout;

//     struct ComputePass {
//         VkPipeline pipeline;
//         ComputePushConstants pushConstants;
//     };

//     std::vector<ComputePass> 
// };

class ComputeEffect {

public:
    struct PushConstants {
        glm::vec4 data[4];
    };

    struct Subpass {
        VkPipeline pipeline;
        PushConstants pushConstants;
        std::vector<VkWriteDescriptorSet> descriptorWrites;
    };

    ComputeEffect();
    ~ComputeEffect();

    void execute();

protected:
    void freeResources();

    VkPipelineLayout mPipelineLayout;
    std::vector<Subpass> mSubpasses;
};
