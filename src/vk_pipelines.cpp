#include "vk_pipelines.h"

#include <fstream>
#include "vk_initializers.h"

bool vkutil::loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule) {
    // open shader file
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    // get file size
    size_t fileSize = (size_t)file.tellg();

    // create uint32_t buffer
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // set file cursor at the beginning
    file.seekg(0);

    // load file into buffer
    file.read((char*)buffer.data(), fileSize);

    // close file
    file.close();

    // create new shader module
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.codeSize = buffer.size() * sizeof(uint32_t); // size in bytes
    createInfo.pCode = buffer.data();

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

    *outShaderModule = shaderModule;

    return true;
}

