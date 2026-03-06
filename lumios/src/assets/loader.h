#pragma once

#include "../defines.h"
#include "../graphics/gpu_types.h"

namespace lumios::assets {

LUMIOS_API MeshData create_cube(float size = 1.0f);
LUMIOS_API MeshData create_sphere(u32 segments = 32, u32 rings = 16, float radius = 0.5f);
LUMIOS_API MeshData create_plane(float size = 10.0f, u32 subdivisions = 1);

} // namespace lumios::assets
