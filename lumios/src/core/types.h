#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <optional>
#include <unordered_map>
#include <cassert>
#include <span>

namespace lumios {

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i32 = int32_t;
using i64 = int64_t;
using f32 = float;
using f64 = double;

template<typename T> using Unique = std::unique_ptr<T>;
template<typename T> using Shared = std::shared_ptr<T>;

template<typename Tag>
struct ResourceHandle {
    u32 index = UINT32_MAX;
    bool valid() const { return index != UINT32_MAX; }
    bool operator==(const ResourceHandle&) const = default;
};

struct MeshTag {};
struct TextureTag {};
struct MaterialTag {};

using MeshHandle     = ResourceHandle<MeshTag>;
using TextureHandle  = ResourceHandle<TextureTag>;
using MaterialHandle = ResourceHandle<MaterialTag>;

} // namespace lumios
