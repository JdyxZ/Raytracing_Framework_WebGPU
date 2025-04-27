#pragma once

// Headers
#include "math/vec3.hpp"
#include "core/core.hpp"
#include "graphics/texture.h"

// Forward declarations
class Perlin;

// Namespace forward declarations
namespace Raytracing
{
    class ImageReader;
}

namespace Raytracing
{
    class Texture
    {
    public:
        virtual ~Texture() = default;
        virtual color value(pair<double, double> texture_coordinates, const point3& p) const = 0;
    };

    class SolidColor : public Raytracing::Texture
    {
    public:
        SolidColor(const color& albedo);
        SolidColor(double red, double green, double blue);

        color value(pair<double, double> texture_coordinates, const point3& p) const override;

    private:
        color albedo;
    };

    class CheckerTexture : public Raytracing::Texture
    {
    public:
        CheckerTexture(double scale, shared_ptr<Raytracing::Texture> even, shared_ptr<Raytracing::Texture> odd);
        CheckerTexture(double scale, const color& c1, const color& c2);

        color value(pair<double, double> texture_coordinates, const point3& p) const override;

    private:
        double inv_scale;
        shared_ptr<Raytracing::Texture> even;
        shared_ptr<Raytracing::Texture> odd;
    };

    class ImageTexture : public Raytracing::Texture
    {
    public:
        ImageTexture(const char* filename);
        ImageTexture(string filename);
        ImageTexture(const sTextureData& data);

        color value(pair<double, double> texture_coordinates, const point3& p) const override;

    private:
        shared_ptr<ImageReader> image;
    };

    class NoiseTexture : public Raytracing::Texture
    {
    public:
        NoiseTexture(double scale, int depth = 7);

        color value(pair<double, double> texture_coordinates, const point3& p) const override;

    private:
        shared_ptr<Perlin> noise;
        double scale = 1;
        int depth = 7;
    };
}


