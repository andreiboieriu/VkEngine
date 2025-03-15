#pragma once

#include "volk.h"

namespace vkutil {
    void transitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void copyImageToImage(VkCommandBuffer commandBuffer, VkImage src, VkImage dst, VkExtent2D srcExtent, VkExtent2D dstExtent);
    void generateMipmaps(VkCommandBuffer commandBuffer, VkImage image, VkExtent2D imageExtent, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};
