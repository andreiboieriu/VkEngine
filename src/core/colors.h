#pragma once
#include <imgui.h>
#include <set>

struct ImGuiContext {
    ImFont* smallFont;
	ImFont* normalFont;
	ImFont* titleFont;
	ImFont* font12;

	ImVec2 spacing;

	std::string selectedEntity;

	bool changedGLTF = false;
	bool changedScript = false;
	bool changedSprite = false;

	bool changedScene = false;
	std::string firstScene;
	std::set<std::string> visitedScenes;

	bool deletedComponent = false;
	std::string componentName = "";

	bool deletedEntity = false;
	bool movedEntity = false;
	std::string entityName = "";
	std::string newParent = "";
};

extern const ImVec4 BUTTON_COLOR;
extern const ImVec4 HOVERED_BUTTON_COLOR;
extern const ImVec4 BUTTON_TEXT_COLOR;
extern const ImVec4 BACKGROUND_COLOR;
extern const ImVec4 WHITE;

inline float srgbToLinear(float c) {
    if (c <= 0.04045f) return c / 12.92f;
    else return powf((c + 0.055f) / 1.055f, 2.4f);
}

inline ImVec4 sRGBtoLinear(const ImVec4& c) {
    return ImVec4(
        srgbToLinear(c.x),
        srgbToLinear(c.y),
        srgbToLinear(c.z),
        c.w
    );
}

inline glm::vec3 linearToSRGB(glm::vec3 rgb)
{
    // See https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
    return mix(1.055f * pow(rgb, glm::vec3(1.0 / 2.4)) - 0.055f,
        rgb * 12.92f,
        lessThanEqual(rgb, glm::vec3(0.0031308f)));
}

inline float linearToSRGB(float c) {
    if (c <= 0.0031308f)
        return 12.92f * c;
    else
        return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

inline ImVec4 linearToSRGB(const ImVec4& c) {
    return ImVec4(
        linearToSRGB(c.x),
        linearToSRGB(c.y),
        linearToSRGB(c.z),
        c.w
    );
}
