// Headers
#include "core/core.hpp"
#include "texture.hpp"
#include "math/perlin.hpp"
#include "utils/image_reader.hpp"
#include "graphics/color.hpp"
#include "math/interval.hpp"
#include "utils/project_parsers.hpp"

// Usings
using Raytracing::color;
using Raytracing::ImageReader;

Raytracing::SolidColor::SolidColor(const color& albedo) : albedo(albedo) {}

Raytracing::SolidColor::SolidColor(double red, double green, double blue) : SolidColor(color(red, green, blue)) {}

color Raytracing::SolidColor::value(optional<pair<double, double>> texture_coordinates, const point3& p) const
{
    return albedo;
}

Raytracing::CheckerTexture::CheckerTexture(double scale, shared_ptr<Texture> even, shared_ptr<Texture> odd)
    : inv_scale(1.0 / scale), even(even), odd(odd) {}

Raytracing::CheckerTexture::CheckerTexture(double scale, const color& c1, const color& c2)
    : CheckerTexture(scale, make_shared<SolidColor>(c1), make_shared<SolidColor>(c2)) {}

color Raytracing::CheckerTexture::value(optional<pair<double, double>> texture_coordinates, const point3& p) const
{
    auto x = int(floor(inv_scale * p.x));
    auto y = int(floor(inv_scale * p.y));
    auto z = int(floor(inv_scale * p.z));

    bool isEven = (x + y + z) % 2 == 0;

    return isEven ? even->value(texture_coordinates, p) : odd->value(texture_coordinates, p);
}

Raytracing::NoiseTexture::NoiseTexture(double scale, int depth) : scale(scale), depth(depth)
{
    noise = make_shared<Perlin>();
}

color Raytracing::NoiseTexture::value(optional<pair<double, double>> texture_coordinates, const point3& p) const
{
    return color(.5, .5, .5) * (1 + std::sin(scale * p.z + 10 * noise->turbulance(p, depth)));
}

Raytracing::ImageTexture::ImageTexture(const char* filename)
{
    image = make_shared<ImageReader>(filename);
}

Raytracing::ImageTexture::ImageTexture(string filename)
{
    image = make_shared<ImageReader>(filename.c_str());
}

Raytracing::ImageTexture::ImageTexture(const sTextureData& data, const pair<WGPUAddressMode, WGPUAddressMode>& uv_wrap_modes) : uv_wrap_modes(uv_wrap_modes)
{
    image = make_shared<ImageReader>(data);
}

color Raytracing::ImageTexture::value(optional<pair<double, double>> texture_coordinates, const point3& p) const
{
    if (!texture_coordinates.has_value())
    {
        string error = Logger::error("TEXTURE", "Null texture coordinates!!!");
        throw std::invalid_argument(error);
    }

    // Parse texture coordinates
    auto parsed_texture_uvs = parse_texture_uvs(texture_coordinates, get_uv_wrap_modes());

    // Unwrap texture coordinates
    auto [u, v] = parsed_texture_uvs.value();

    // If we have no texture data, then return solid cyan as a debugging aid.
    if (image->height() <= 0)
        return CYAN;

    // Clamp input texture coordinates to [0,1] x [1,0]
    u = std::clamp(u, 0.0, 1.0);
    v = std::clamp(v, 0.0, 1.0);  // Flip V to image coordinates

    // Get pixel location
    auto i = int(u * image->width());
    auto j = int(v * image->height());

    // Read pixel data
    color pixel_color = image->pixel_data(i, j);

    return pixel_color;
}

pair<WGPUAddressMode, WGPUAddressMode> Raytracing::ImageTexture::get_uv_wrap_modes() const
{
    return uv_wrap_modes;
}

Raytracing::SkyboxTexture::SkyboxTexture(const char* filename)
{
    skybox = make_shared<ImageReader>(filename);
}

Raytracing::SkyboxTexture::SkyboxTexture(string filename)
{
    skybox = make_shared<ImageReader>(filename.c_str());
}

Raytracing::SkyboxTexture::SkyboxTexture(const sTextureData& data)
{
    skybox = make_shared<ImageReader>(data);
}

color Raytracing::SkyboxTexture::value(const vec3& ray_direction) const
{
    // Get hemispherical coordinates from ray direction
    double theta = atan2(ray_direction.x, ray_direction.z);
    double phi = acos(std::clamp(ray_direction.y, -1.0, 1.0));

    // Convert hemispherical coordinates to UV coordinates
    double u = (theta + Raytracing::pi) / (2.0f * Raytracing::pi);
    double v = phi / Raytracing::pi;

    color pixel_color = skybox->pixel_data(int(u * (skybox->width() - 1)), int(v * (skybox->height() - 1)));

    return pixel_color;
}
