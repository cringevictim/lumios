#include "loader.h"

namespace lumios::assets {

MeshData create_cube(float size) {
    float s = size * 0.5f;
    MeshData mesh;

    // Face data: normal, tangent-u, tangent-v
    struct Face { glm::vec3 n, u, v; };
    Face faces[] = {
        {{ 0, 0, 1}, { 1, 0, 0}, {0, 1, 0}},  // front
        {{ 0, 0,-1}, {-1, 0, 0}, {0, 1, 0}},  // back
        {{ 1, 0, 0}, { 0, 0,-1}, {0, 1, 0}},  // right
        {{-1, 0, 0}, { 0, 0, 1}, {0, 1, 0}},  // left
        {{ 0, 1, 0}, { 1, 0, 0}, {0, 0,-1}},  // top
        {{ 0,-1, 0}, { 1, 0, 0}, {0, 0, 1}},  // bottom
    };

    for (auto& f : faces) {
        u32 base = static_cast<u32>(mesh.vertices.size());
        glm::vec2 uvs[] = {{0,1},{1,1},{1,0},{0,0}};
        glm::vec3 corners[] = {
            f.n * s - f.u * s - f.v * s,
            f.n * s + f.u * s - f.v * s,
            f.n * s + f.u * s + f.v * s,
            f.n * s - f.u * s + f.v * s
        };
        for (int i = 0; i < 4; i++)
            mesh.vertices.push_back({corners[i], f.n, uvs[i], {1,1,1,1}});
        mesh.indices.insert(mesh.indices.end(), {base,base+1,base+2, base,base+2,base+3});
    }
    return mesh;
}

MeshData create_sphere(u32 segments, u32 rings, float radius) {
    MeshData mesh;
    for (u32 y = 0; y <= rings; y++) {
        for (u32 x = 0; x <= segments; x++) {
            float xf = static_cast<float>(x) / segments;
            float yf = static_cast<float>(y) / rings;
            float theta = xf * 2.0f * glm::pi<float>();
            float phi   = yf * glm::pi<float>();

            glm::vec3 n{
                sin(phi) * cos(theta),
                cos(phi),
                sin(phi) * sin(theta)
            };
            mesh.vertices.push_back({n * radius, n, {xf, yf}, {1,1,1,1}});
        }
    }
    for (u32 y = 0; y < rings; y++) {
        for (u32 x = 0; x < segments; x++) {
            u32 a = y * (segments + 1) + x;
            u32 b = a + segments + 1;
            mesh.indices.insert(mesh.indices.end(), {a, b, a+1, a+1, b, b+1});
        }
    }
    return mesh;
}

MeshData create_plane(float size, u32 subdivisions) {
    MeshData mesh;
    float half = size * 0.5f;
    float step = size / subdivisions;
    float uv_step = 1.0f / subdivisions;

    for (u32 z = 0; z <= subdivisions; z++) {
        for (u32 x = 0; x <= subdivisions; x++) {
            float px = -half + x * step;
            float pz = -half + z * step;
            mesh.vertices.push_back({
                {px, 0, pz}, {0, 1, 0},
                {x * uv_step, z * uv_step}, {1,1,1,1}
            });
        }
    }
    for (u32 z = 0; z < subdivisions; z++) {
        for (u32 x = 0; x < subdivisions; x++) {
            u32 a = z * (subdivisions + 1) + x;
            u32 b = a + subdivisions + 1;
            mesh.indices.insert(mesh.indices.end(), {a, b, a+1, a+1, b, b+1});
        }
    }
    return mesh;
}

} // namespace lumios::assets
