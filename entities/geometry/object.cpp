/*
 * object.cpp
 */

#include "object.h"

// helper functions

// safe from string to int
static std::optional<int> safe_stoi(const std::string &token)
{
    try {
        size_t pos = 0;
        int v = std::stoi(token, &pos, 10);
        if (pos != token.size())
            return std::nullopt;
        return v;
    }
    catch (const std::exception &) {
        return std::nullopt;
    }
}

// from obj index to vector index
static int relative_index(const int idx, int total_vertices)
{
    if (idx == 0 || idx < -total_vertices || idx > total_vertices)
    {
        std::cerr << "warning: invalid vertex index " << idx << std::endl;
        return -1;
    }

    return idx < 0 ? total_vertices + idx : idx - 1;
}

// clean string
static void strip_line(std::string &line)
{
    std::erase_if(line, [](const char c) { return c == '\r' || c == '\n'; });
    std::ranges::replace(line, '\t', ' ');
}

// check open file
static std::optional<std::ifstream> open_file(const std::string &filename)
{
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (!in.is_open())
    {
        std::cerr << "error: can't open file " << filename << std::endl;
        return std::nullopt;
    }

    return in;
}

// parse functions

bool Object::validate() const
{
    if (vertices.empty() || faces.empty())
    {
        std::cerr << "error: invalid object" << std::endl;
        return false;
    }

    size_t n = vertices.size();
    for (const auto &f : faces)
    {
        for (auto idx : f.indices)
        {
            if (idx >= n)
            {
                std::cerr << "error: invalid object" << std::endl;
                return false;
            }
        }
    }

    return true;
}

// parse v x y z
bool Object::parse_vertex(const std::string &line)
{
    std::stringstream ss(line);

    float x, y, z;
    if (!(ss >> x >> y >> z))
    {
        std::cerr << "warning: invalid vertex format" << std::endl;
        return false;
    }

    vertices.emplace_back(x, y, z);
    return true;
}

// parse f
bool Object::parse_face(const std::string &line, std::optional<int> current_material)
{
    std::stringstream ss(line);
    std::vector<unsigned int> local_indices;

    std::string token;
    while (ss >> token)
    {
        if (const auto slash_pos = token.find('/'); slash_pos != std::string::npos)
        {
            token.erase(slash_pos); // keep only first index
        }

        auto maybe_idx = safe_stoi(token);
        if (!maybe_idx)
        {
            std::cerr << "warning: invalid face token " << token << std::endl;
            return false;
        }

        int ridx = relative_index(*maybe_idx, static_cast<int>(vertices.size()));
        if (ridx < 0 || static_cast<size_t>(ridx) >= vertices.size())
        {
            std::cerr << "warning: vertex index " << *maybe_idx << " out of range" << std::endl;
            return false;
        }
        local_indices.push_back(static_cast<unsigned int>(ridx));
    }

    if (local_indices.size() < 3)
    {
        std::cerr << "warning: face contains less than 3 indexes" << std::endl;
        return false;
    }

    if (local_indices.size() == 3)
    {
        faces.emplace_back(local_indices[0], local_indices[1], local_indices[2], current_material);
        return true;
    }

    // triangularization
    std::vector<Vec3> polygon;
    polygon.reserve(local_indices.size());

    for (const auto idx : local_indices)
    {
        polygon.push_back(vertices[idx]);
    }

    const auto result = triangularize(polygon);
    if (!result.has_value())
    {
        std::cerr << "warning: triangularize failed" << std::endl;
        return false;
    }

    // adding faces
    const auto &triangle_indices = result.value();
    for (size_t i = 0; i < triangle_indices.size(); i += 3)
    {
        unsigned int i1 = local_indices[ triangle_indices[i] ];
        unsigned int i2 = local_indices[ triangle_indices[i+1] ];
        unsigned int i3 = local_indices[ triangle_indices[i+2] ];
        faces.emplace_back(i1, i2, i3, current_material);
    }

    return true;
}

// parse mtllib
bool Object::parse_mtl_file(const std::string &line, const std::string &obj_filename)
{
    std::stringstream ss(line);

    std::string mtl_filename;
    ss >> mtl_filename;
    if (mtl_filename.empty())
    {
        std::cerr << "error: can't parse mtl filename" << std::endl;
        return false;
    }

    const auto parent = std::filesystem::path(obj_filename).parent_path();
    const auto full_mtl_filename = parent / mtl_filename;
    load_materials(full_mtl_filename.string());
    return true;
}

// parse usemtl
std::optional<int> Object::parse_material(const std::string &line) const
{
    std::stringstream ss(line);

    std::string material_name;
    ss >> material_name;

    return find_material(material_name);
}

// parse newmtl
bool Object::parse_current_material(const std::string &line, std::string &current_name, Vec3 &current_diffuse, bool &have_active_material)
{
    if (have_active_material)
    {
        materials.emplace_back(current_name, current_diffuse);
    }

    if (std::stringstream ss(line); !(ss >> current_name))
    {
        std::cerr << "error: can't parse material name" << std::endl;
        return false;
    };
    current_diffuse = Vec3(1.0f, 1.0f, 1.0f);
    have_active_material = true;
    return true;
}

// parse kd
bool Object::parse_diffuse_color(const std::string &line, Vec3 &current_diffuse)
{
    std::stringstream ss(line);

    float r, g, b;
    if (!(ss >> r >> g >> b))
    {
        std::cerr << "error: can't parse diffuse colors" << std::endl;
        return false;
    }

    current_diffuse = Vec3(r, g, b);
    return true;
}

// methods
bool Object::load(const std::string &obj_filename, bool color_support)
{
    auto file = open_file(obj_filename);
    if (!file)
    {
        return false;
    }

    std::ifstream in = std::move(*file);

    std::optional<int> current_material = std::nullopt;
    std::string line;

    while (std::getline(in, line))
    {
        strip_line(line);

        if (line.empty() || line[0] == '#') // comment
        {
            continue;
        }

        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        std::string arguments = line.substr(cmd.size());
        if (!arguments.empty() && arguments[0] == ' ')
        {
            arguments.erase(0, 1);
        }

        bool ok = true;

        if (cmd == "v") // vertex
        {
            ok = parse_vertex(arguments);
        }
        else if (cmd == "f") // face
        {
            ok = parse_face(arguments, current_material);
        }
        else if (color_support && cmd == "mtllib")  // material file
        {
            ok = parse_mtl_file(arguments, obj_filename);
        }
        else if (color_support && cmd == "usemtl")  // material
        {
            current_material = parse_material(arguments);

            if (!current_material)
            {
                std::cerr << "warning: unknown material " << arguments << std::endl;
            }
        }
        // ignoring anything else

        if (!ok)
        {
            return false;
        }
    }

    in.close();
    return validate();
}

bool Object::load_materials(const std::string &mtl_filename)
{
    auto file = open_file(mtl_filename);
    if (!file.has_value())
    {
        return false;
    }

    std::ifstream in = std::move(*file);

    std::string current_name;
    Vec3 current_diffuse(1.0f, 1.0f, 1.0f);
    bool have_active_material = false;
    std::string line;

    while (std::getline(in, line))
    {
        strip_line(line);

        if (line.empty() || line[0] == '#') // comment
            continue;

        std::stringstream ss(line);
        std::string cmd;
        ss >> cmd;

        std::string arguments = line.substr(cmd.size());
        if (!arguments.empty() && arguments[0] == ' ')
        {
            arguments.erase(0, 1);
        }

        if (cmd == "newmtl") // current material
        {
            parse_current_material(arguments, current_name, current_diffuse, have_active_material);
        }
        else if (cmd == "Kd") // diffuse color
        {
            parse_diffuse_color(arguments, current_diffuse);
        }
    }

    if (have_active_material)
    {
        materials.emplace_back(current_name, current_diffuse);
    }

    return true;
}

// find material by index
std::optional<int> Object::find_material(const std::string &material_name) const
{
    const auto it = std::ranges::find_if(materials, [&material_name](const Material &m){ return m.material_name == material_name; });
    return (it != materials.end()) ? std::make_optional(std::distance(materials.begin(), it)) : std::nullopt;
}

void Object::scale(float factor)
{
    for (auto &v : vertices)
    {
        v *= factor;
    }
}

// normalize verts of object
void Object::normalize()
{
    if (vertices.empty())
    {
        return;
    }

    Vec3 vmin = vertices[0];
    Vec3 vmax = vertices[0];

    for (const auto &v : vertices)
    {
        vmin.x = std::min(vmin.x, v.x);
        vmin.y = std::min(vmin.y, v.y);
        vmin.z = std::min(vmin.z, v.z);

        vmax.x = std::max(vmax.x, v.x);
        vmax.y = std::max(vmax.y, v.y);
        vmax.z = std::max(vmax.z, v.z);
    }

    const Vec3 center = (vmin + vmax) * 0.5f;
    const float scale = 1.0f / std::max({
        vmax.x - vmin.x,
        vmax.y - vmin.y,
        vmax.z - vmin.z,
        1e-6f
    });

    for (auto &v : vertices)
    {
        v = (v - center) * scale;
    }
}

void Object::flip_faces()
{
    for (auto &f : faces)
    {
        std::swap(f.indices[1], f.indices[2]);
    }
}

void Object::invert_x()
{
    for (auto &v : vertices)
    {
        v.x = -v.x;
    }

    flip_faces();
}

void Object::invert_y()
{
    for (auto &v : vertices)
    {
        v.y = -v.y;
    }

    flip_faces();
}


void Object::invert_z()
{
    for (auto &v : vertices)
    {
        v.z = -v.z;
    }

    flip_faces();
}
