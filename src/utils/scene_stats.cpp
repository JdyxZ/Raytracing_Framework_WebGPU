// Headers
#include "core/core.hpp"
#include "scene_stats.hpp"
#include "hittables/box.hpp"
#include "hittables/quad.hpp"
#include "hittables/mesh.hpp"
#include "materials/material.hpp"
#include "hittables/bvh.hpp"

// Usings
using Raytracing::Mesh;
using Raytracing::Material;

scene_stats::scene_stats()
{
    bvh_chrono = Chrono();
}

int scene_stats::get_bvh_depth(const shared_ptr<Hittable> object)
{
    switch (object->get_type())
    {
   
    case BOX:
    {
        auto box_ptr = std::dynamic_pointer_cast<Box>(object);
        return box_ptr->get_stats().bvh_depth;
    }
    case MESH:
    {
        auto mesh_ptr = std::dynamic_pointer_cast<Mesh>(object);
        return mesh_ptr->get_stats().bvh_depth;
    }
    case BVH_NODE:
    {
        auto bvh_node_ptr = std::dynamic_pointer_cast<bvh_node>(object);
        auto stats = bvh_node_ptr->get_stats();
        return stats.bvh_depth;
    }
    }

    return 0;
}

void scene_stats::add(const shared_ptr<Hittable> object)
{
    switch (object->get_type())
    {
    case SPHERE:
        spheres++;
        primitives++;
        break;
    case TRIANGLE:
        triangles++;
        primitives++;
        break;
    case QUAD:
    {
        auto quad_ptr = std::dynamic_pointer_cast<Quad>(object);

        auto material = quad_ptr->get_material();
        if (material->get_class() == DIFFUSE_LIGHT) emissives++;

        quads++;
        primitives++;
        break;
    }
    case BOX:
    {
        auto box_ptr = std::dynamic_pointer_cast<Box>(object);
        if (box_ptr->is_bvh())
        {
            *this += box_ptr->get_stats();
        }
        else
        {
            quads += 6;
            primitives += 6;
        }
        break;
    }
    case MESH:
    {
        auto mesh_ptr = std::dynamic_pointer_cast<Mesh>(object);
        meshes.push_back(mesh_ptr);
        if (mesh_ptr->is_bvh())
        {
            *this += mesh_ptr->get_stats();
        }
        else
        {
            int num_triangles = mesh_ptr->num_triangles();
            triangles += num_triangles;
            primitives += num_triangles;
        }
        break;
    }
    case BVH_NODE:
    {
        auto bvh_node_ptr = std::dynamic_pointer_cast<bvh_node>(object);
        auto stats = bvh_node_ptr->get_stats();
        *this += stats;
        break;
    }
    }
}

scene_stats& scene_stats::operator+=(const scene_stats& s)
{
    spheres += s.spheres;
    quads += s.quads;
    triangles += s.triangles;
    primitives += s.primitives;
    emissives += s.emissives;
    bvh_depth += s.bvh_depth;
    bvh_nodes += s.bvh_nodes;
    bvh_chrono += s.bvh_chrono;
    return *this;
}

