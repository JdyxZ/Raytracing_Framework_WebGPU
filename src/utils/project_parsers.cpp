// Headers
#include "core/core.hpp"
#include "project_parsers.hpp"
#include "hittables/mesh.hpp"
#include "hittables/surface.hpp"
#include "math/matrix.hpp"
#include "materials/material.hpp"
#include "materials/texture.hpp"
#include "graphics/color.hpp"
#include "hittables/triangle.hpp"
#include "hittables/hittable_list.hpp"
#include "graphics/texture.h"
#include "graphics/camera.hpp"

// Framework headers
#include "framework/nodes/mesh_instance_3d.h"
#include "framework/nodes/environment_3d.h"
#include "framework/camera/camera.h"

// Usings
using Raytracing::Mesh;
using Raytracing::Matrix44;
using Raytracing::ImageTexture;
using Raytracing::Lambertian;
using Raytracing::color;
using Raytracing::normal;
using Raytracing::CameraData;
using Raytracing::SkyboxTexture;

shared_ptr<ParsedScene> parse_nodes(const vector<Node*>& nodes, const bool use_bvh)
{
    shared_ptr<ParsedScene> parsed_scene;
    vector<shared_ptr<Mesh>> meshes;

    for (auto node : nodes)
    {
        // Skip skybox node
        if (node->get_node_type() == "Environment3D") // Revisar
            continue;

        auto parsed_node = parse_node(node, use_bvh);
        auto node_meshes = parsed_node.meshes;

        if (!node_meshes.empty())
            meshes.insert(meshes.end(), node_meshes.begin(), node_meshes.end());
    }

    parsed_scene = make_shared<ParsedScene>(meshes);

    return parsed_scene;
}

ParsedNode parse_node(Node* node, const bool use_bvh)
{
    ParsedNode parsed_node;
    vector<shared_ptr<Mesh>> meshes;

    std::function<void(Node*)> parse = [&](Node* node)
    {
        MeshInstance3D* scene_mesh = dynamic_cast<MeshInstance3D*>(node);

        if (scene_mesh)
        {
            // Name
            string name = scene_mesh->get_name();

            // Model matrix
            auto model = Matrix44(scene_mesh->get_global_model());

            // Surfaces
            hittable_list surfaces;

            for (auto surface : scene_mesh->get_surfaces())
            {
                auto new_surface = parse_surface(surface, model, use_bvh);
                surfaces.add(new_surface);
            }

            // Mesh
            auto new_mesh = make_shared<Mesh>(name, surfaces, model, use_bvh);

            meshes.push_back(new_mesh);
        }

        if (!node->get_children().empty())
        {
            for (auto child : node->get_children())
            {
                parse(child);
            }
        }
    };

    parse(node);

    parsed_node = ParsedNode{ meshes };

    return parsed_node;
}

SkyboxTexture* parse_skybox(Environment3D* skybox)
{
    // Get skybox texture
    Texture* skybox_texture = skybox->get_surface(0)->get_material()->get_diffuse_texture();

    if (!skybox_texture)
    {
        Logger::warn("Raytracing", "Skybox texture is not set for the Environment3D node.");
        return nullptr;
    }

    sTextureData& texture_data = skybox_texture->get_texture_data();
    auto parsed_skybox_texture = new SkyboxTexture(texture_data);

    return parsed_skybox_texture;
}

shared_ptr<Raytracing::Surface> parse_surface(Surface* surface, const Raytracing::Matrix44& mesh_model, const bool use_bvh)
{
    // Parse material
    const Material* material = surface->get_material();
    auto parsed_material = parse_material(material);

    // Triangles
    hittable_list triangles;
    sSurfaceData& surface_data = surface->get_surface_data();

    // Iterate vertices
    if (surface_data.indices.empty())
    {
        auto triangle_vertices = vector<vertex>(3);
        triangles.reserve(surface_data.vertices.size() / 3);

        for (size_t i = 0; i < surface_data.vertices.size(); i++)
        {
            // Create vertex
            vertex v;

            // Position
            v.position = point3(surface_data.vertices[i]);

            // Normal
            v.normal = !surface_data.normals.empty() ? std::optional<normal>(surface_data.normals[i]) : std::nullopt;

            // Texture coordinates
            v.uv = !surface_data.uvs.empty() ? std::optional<pair<double, double>>(make_pair(surface_data.uvs[i].x, surface_data.uvs[i].y)) : std::nullopt;

            // Color
            v.color = !surface_data.colors.empty() ? std::optional<color>(surface_data.colors[i]) : std::nullopt;

			// Store vertex
            triangle_vertices[i % 3] = v;

			// Create and add triangle
			if ((i + 1) % 3 == 0)
			{
				auto triangle = make_shared<Triangle>(triangle_vertices[0], triangle_vertices[1], triangle_vertices[2], parsed_material, mesh_model, false);
				triangles.add(triangle);
			}
        }
    }

    // Iterate indices
    else
    {
        auto triangle_vertices = vector<vertex>(3);
        triangles.reserve(surface_data.indices.size() / 3);

        for (size_t i = 0; i < surface_data.indices.size(); i++)
        {
            // Create vertex
            vertex v;

			// Get surface index
            size_t surface_index = surface_data.indices[i];

            // Position
            v.position = point3(surface_data.vertices[surface_index]);

            // Normal
            v.normal = !surface_data.normals.empty() ? optional<normal>(surface_data.normals[surface_index]) : nullopt;

            // Texture coordinates
            v.uv = !surface_data.uvs.empty() ? optional<pair<double, double>>(make_pair(surface_data.uvs[surface_index].x, surface_data.uvs[surface_index].y)) : nullopt;

            // Color
            v.color = !surface_data.colors.empty() ? optional<color>(surface_data.colors[surface_index]) : nullopt;

			// Store vertex
            triangle_vertices[i % 3] = v;

			// Create and add triangle
			if ((i + 1) % 3 == 0)
			{
				auto triangle = make_shared<Triangle>(triangle_vertices[0], triangle_vertices[1], triangle_vertices[2], parsed_material, mesh_model, false);
				triangles.add(triangle);
			}
        }
    }

    // Parsed surface
    auto parsed_surface = make_shared<Raytracing::Surface>(triangles, parsed_material, nullopt, use_bvh);

    return parsed_surface;
}

shared_ptr<Raytracing::Material> parse_material(const Material* material)
{
    // Material initialization struct
    PBR_data material_data;

    // Scalaras
    material_data.albedo_scalar = material->get_color();
    material_data.roughness_scalar = material->get_roughness();
    material_data.metalness_scalar = material->get_metallic();

    // Get original textures
    const Texture* albedo_texture = material->get_diffuse_texture();
    const Texture* normal_texture = material->get_normal_texture();
    const Texture* roughness_metalness_texture = material->get_metallic_roughness_texture();

    // Parse textures
    material_data.albedo_texture = albedo_texture ? parse_texture(albedo_texture) : nullptr;
    material_data.normal_texture = normal_texture ? parse_texture(normal_texture) : nullptr;
    material_data.roughness_metalness_texture = roughness_metalness_texture ? parse_texture(roughness_metalness_texture) : nullptr;

    // Create material
    return make_shared<Raytracing::PBR>(material_data);
}

shared_ptr<ImageTexture> parse_texture(const Texture* texture)
{
    // Parse texture wrap modes
    const sTextureData& texture_data = texture->get_texture_data();
    pair<WGPUAddressMode, WGPUAddressMode> uv_wrap_modes = make_pair(texture->get_wrap_u(), texture->get_wrap_v());

    return make_shared<ImageTexture>(texture_data, uv_wrap_modes);
}

optional<pair<double, double>> parse_texture_uvs(const optional<pair<double, double>>& uvs, const pair<WGPUAddressMode, WGPUAddressMode>& uv_wrap_mode)
{
	// Check uvs content
	if (!uvs.has_value())
		return nullopt;

	// UV
	double u = uvs.value().first;
	double v = uvs.value().second;

	// UV modes
	WGPUAddressMode u_wrap_mode = uv_wrap_mode.first;
	WGPUAddressMode v_wrap_mode = uv_wrap_mode.second;

	// Parse coordinates
	double parsed_u = parse_uv(u, u_wrap_mode);
	double parsed_v = parse_uv(v, v_wrap_mode);

	return make_pair(parsed_u, parsed_v);
}

double parse_uv(double coord, WGPUAddressMode wrap_mode)
{
	double parsed_coord = -1;

	switch (wrap_mode)
	{
	case(WGPUAddressMode_Undefined):
		parsed_coord = coord;
		break;
	case(WGPUAddressMode_ClampToEdge):
		parsed_coord = std::clamp(coord, 0.0, 1.0);
		break;
	case(WGPUAddressMode_Repeat):
		parsed_coord = coord - std::floor(coord);
		break;
	case(WGPUAddressMode_MirrorRepeat):
		double mirrored = std::fabs(coord);
		int period = static_cast<int>(std::floor(mirrored));
		double frac = mirrored - period;

		// Flip every other period
		if (period % 2 == 1)
			frac = 1.0 - frac;

		parsed_coord = frac;
		break;
	}

	return parsed_coord;
}

CameraData parse_camera_data(Camera* camera)
{
    CameraData data;

    data.lookfrom = vec3(camera->get_eye());
    data.lookat = vec3(camera->get_center());
    data.world_up = vec3(camera->get_up());

    data.vertical_fov = radians_to_degrees(camera->get_fov());
    data.focus_distance = (data.lookfrom - data.lookat).length(); // Perfectly focused
    data.defocus_angle = 0.0; // No blur

    return data;
}
