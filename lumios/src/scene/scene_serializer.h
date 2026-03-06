#pragma once

#include "scene.h"
#include <string>

namespace lumios {

class SceneSerializer {
public:
    static bool save(const Scene& scene, const std::string& path);
    static bool load(Scene& scene, const std::string& path);

    static std::string serialize(const Scene& scene);
    static bool deserialize(Scene& scene, const std::string& json_str);
};

} // namespace lumios
