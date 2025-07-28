#pragma once

#include "volk.h"

namespace vkutil {
    void transitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    void copyImageToImage(VkCommandBuffer commandBuffer, VkImage src, VkImage dst, VkExtent2D srcExtent, VkExtent2D dstExtent, VkExtent2D srcStart = {0, 0}, VkExtent2D dstStart = {0, 0}, VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    void generateMipmaps(VkCommandBuffer commandBuffer, VkImage image, VkExtent2D imageExtent, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    uint32_t getPixelSize(VkFormat format);
};
