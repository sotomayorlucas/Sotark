module;

#include <vector>
#include <array>
#include <string>
#include <string_view>
#include <expected>
#include <fstream>
#include <sstream>
#include <format>
#include <cstdio>

export module sotark.rt:obj;

import sotark.common;

export namespace sotark::rt {

struct ObjMesh {
    std::vector<Vec3>                 verts;
    std::vector<std::array<int, 3>>   tris;
};

// Minimal Wavefront OBJ loader.
// Supports:
//   v x y z              — vertex position
//   f i j k ...          — polygon face (1-indexed); fan triangulation
//   f i/t/n              — slashes ignored (only first int is used)
//   # comments           — skipped
//   groups, materials    — silently ignored
//   negative indices     — offset from end (OBJ spec)
inline std::expected<ObjMesh, std::string>
obj_load(std::string_view path) noexcept {
    std::string path_str(path);
    std::ifstream f(path_str);
    if (!f) return std::unexpected(std::format("could not open '{}'", path));

    ObjMesh mesh;
    std::string line;
    while (std::getline(f, line)) {
        if (line.size() >= 2 && line[0] == 'v' && line[1] == ' ') {
            Vec3 v{};
            if (std::sscanf(line.c_str(), "v %f %f %f", &v.x, &v.y, &v.z) == 3) {
                mesh.verts.push_back(v);
            }
        } else if (line.size() >= 2 && line[0] == 'f' && line[1] == ' ') {
            // Parse up to 16 vertex indices, ignoring '/n/m' suffixes.
            std::array<int, 16> idx{};
            int n_idx = 0;
            const char* p = line.c_str() + 2;
            while (*p && n_idx < 16) {
                while (*p == ' ' || *p == '\t') ++p;
                if (*p == '\0' || *p == '\n' || *p == '\r') break;
                int i = 0;
                if (std::sscanf(p, "%d", &i) != 1) break;
                if (i < 0) i = static_cast<int>(mesh.verts.size()) + i + 1;
                idx[n_idx++] = i - 1;   // OBJ 1-indexed -> 0-indexed
                while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
            }
            // Fan triangulation: (idx[0], idx[i], idx[i+1]).
            for (int i = 1; i < n_idx - 1; ++i) {
                const int a = idx[0], b = idx[i], c = idx[i + 1];
                const int n = static_cast<int>(mesh.verts.size());
                if (a < 0 || a >= n || b < 0 || b >= n || c < 0 || c >= n) continue;
                mesh.tris.push_back({a, b, c});
            }
        }
        // Other directives (vt, vn, g, mtllib, usemtl, #) are silently ignored.
    }
    log_info("obj_load '{}': {} verts, {} tris", path, mesh.verts.size(), mesh.tris.size());
    return mesh;
}

}  // namespace sotark::rt
