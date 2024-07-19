#pragma once

#include <vulkan/vulkan.h>

namespace vkutil {

    void transitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void copyImageToImage(VkCommandBuffer commandBuffer, VkImage src, VkImage dst, VkExtent2D srcExtent, VkExtent2D dstExtent);
    void generateMipmaps(VkCommandBuffer commandBuffer, VkImage image, VkExtent2D imageExtent);
};