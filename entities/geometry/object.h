/*
 * object.h
 */

#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>

#include "utils/algorithms.h"
#include "utils/tools.h"

// triangular face
class Face {
public:
    std::array<unsigned int, 3> indices;    // indices of vertices
    std::optional<int> material;            // index of material

    Face(const unsigned int idx1, const unsigned int idx2, const unsigned int idx3, const std::optional<int> mat = std::nullopt) : indices{idx1, idx2, idx3}, material(mat) {}
};

// material properties
class Material {
public:
    std::string material_name;      // material name
    Vec3 diffuse;                   // diffuse color (Kd) - red, green, blue components

    Material(const std::string &name, const Vec3 &color) : material_name(name), diffuse(color) {}
};

// object (3d model)
class Object {
public:
    Object() = default;

    std::vector<Vec3> vertices;
    std::vector<Face> faces;
    std::vector<Material> materials;

    // load obj file with optional material mtl support
    bool load(const std::string &obj_filename, bool color_support = false);


    void normalize();           // normalize object
    void scale(float factor);   // scale object
    void flip_faces();          // flip faces winding order

    void invert_x();    // invert axes
    void invert_y();
    void invert_z();

private:
    // material related methods
    bool load_materials(const std::string &mtl_filename);
    std::optional<int> find_material(const std::string &material_name) const;

    // composite methods of parser
    bool parse_vertex(const std::string &line);
    bool parse_face(const std::string &line, std::optional<int> current_material);
    bool parse_mtl_file(const std::string &line, const std::string &obj_filename);
    std::optional<int> parse_material(const std::string &line) const;
    bool parse_current_material(const std::string &line, std::string &current_name, Vec3 &current_diffuse, bool &have_active_material);
    static bool parse_diffuse_color(const std::string &line, Vec3 &current_diffuse);

    // validation of object after parsing
    bool validate() const;

};
