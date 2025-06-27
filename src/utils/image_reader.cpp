// Internal Headers
#include "core/core.hpp"
#include "image_reader.hpp"
#include "utils/utilities.hpp"
#include "graphics/color.hpp"

// External Headers
#include "stb_image.h"

Raytracing::ImageReader::ImageReader(const char* image_filename)
{
    // Define image specs
    image_type = ImageType::LOADED;
    auto filename = string(image_filename);

    // Hunt for the image file in some likely locations.
    string prefix = "";
    for (int i = 0; i < 7; ++i)
    {
        if (load(prefix + textures_directory + "\\" + filename)) return;
        prefix += "..\\";
    }
    if (load(filename)) return;

    Logger::error("ImageReader", "Could not load image file: " + string(image_filename));
}

Raytracing::ImageReader::ImageReader(const sTextureData& tex_data)
{
    // Check texture data
    if (tex_data.data.size() == 0)
    {
        string error = Logger::error("IMAGE_READER", "Texture data is empty");
        throw std::invalid_argument(error);
    }

    // Set image specs
    is_linear = true; // All textures from framework are linear (assumption)
    image_type = ImageType::PARSED;
    image_width = tex_data.image_width;
    image_height = tex_data.image_height;
    channels = tex_data.channels;
    bytes_per_pixel = tex_data.bytes_per_pixel;
    bytes_per_scanline = image_width * bytes_per_pixel;
    bit_depth = (bytes_per_pixel / channels) * 8;

    // Get image data type
    data_type = get_data_type(bit_depth);

    // Set proper pointer to point to texture data
    switch (data_type)
    {
    case ImageDataType::UINT8_T:
        external_uint8_data = tex_data.data.data();
        break;
    case ImageDataType::UINT16_T:
        external_uint16_data = reinterpret_cast<const uint16_t*>(tex_data.data.data());
        break;
    case ImageDataType::FLOAT:
        external_float_data = reinterpret_cast<const float*>(tex_data.data.data());
        break;
    }
}

Raytracing::ImageReader::~ImageReader()
{
    // No need to check for nullptr, since deleting nullptr is safe
    delete[] owned_uint8_data;
    delete[] owned_uint16_data;
    stbi_image_free(owned_float_data);

}

bool Raytracing::ImageReader::load(const string& filename)
{
    // Load image data (directly loads data in linear (gamma = 1))
    float* float_ptr = stbi_loadf(filename.c_str(), &image_width, &image_height, &channels, 4);

    // Check
    if (float_ptr == nullptr)
        return false;

    // Get image specs
    data_type = get_data_type(filename);

    switch (data_type)
    {
    case ImageDataType::UINT8_T:
        owned_uint8_data = convert_float_to_uint8(float_ptr, image_width * image_height * channels);
        bytes_per_pixel = sizeof(uint8_t) * channels;
        bit_depth = 8;
        stbi_image_free(float_ptr);
        break;
    case ImageDataType::UINT16_T:
        owned_uint16_data = convert_float_to_uint16(float_ptr, image_width * image_height * channels);
        bytes_per_pixel = sizeof(uint16_t) * channels;
        bit_depth = 16;
        stbi_image_free(float_ptr);
        break;
    case ImageDataType::FLOAT:
        bytes_per_pixel = sizeof(float) * channels;
        bit_depth = 32;
        owned_float_data = float_ptr;
        break;
    }

    is_linear = true; // stbi_loadf returns linearized data (assumption)
    bytes_per_scanline = image_width * bytes_per_pixel;

    return true;
}

const Raytracing::color Raytracing::ImageReader::pixel_data(int x, int y) const
{
    if (image_width == 0 && image_height == 0)
        return MAGENTA;

    if (!is_within(x, 0, image_width, BoundType::inclusive, BoundType::exclusive) ||
        !is_within(y, 0, image_height, BoundType::inclusive, BoundType::exclusive))
    {
        Logger::error("IMAGE_READER", std::format("Pixel coordinates ({}, {}) out of bounds", x, y));
        return MAGENTA;
    }

    color pixel_color = MAGENTA;
    int index = (y * image_width + x) * channels;

    switch (data_type)
    {
    case ImageDataType::UINT8_T:
    {
        const auto pixel = image_type == ImageType::LOADED ? &owned_uint8_data[index] : &external_uint8_data[index];
        auto scale = 1.0f / 255.0f;
        pixel_color = vec3(pixel[0], pixel[1], pixel[2]) * scale;
        break;
    }
    case ImageDataType::UINT16_T:
    {
        const auto pixel = image_type == ImageType::LOADED ? &owned_uint16_data[index] : &external_uint16_data[index];
        auto scale = 1.0f / 65535.0f;
        pixel_color = vec3(pixel[0], pixel[1], pixel[2]) * scale;
        break;
    }
    case ImageDataType::FLOAT:
    {
        const auto pixel = image_type == ImageType::LOADED ? &owned_float_data[index] : &external_float_data[index];
        pixel_color = vec3(pixel[0], pixel[1], pixel[2]);
        break;
    }
    }

    return pixel_color;
}

uint8_t* Raytracing::ImageReader::convert_float_to_uint8(const float* float_data, size_t count)
{
    uint8_t* result = new uint8_t[count];

    for (size_t i = 0; i < count; ++i)
    {
        float clamped = std::clamp(float_data[i], 0.0f, 1.0f);
        result[i] = static_cast<uint8_t>(clamped * 255.0f + 0.5f); // +0.5f for rounding
    }

    return result;
}

uint16_t* Raytracing::ImageReader::convert_float_to_uint16(const float* float_data, size_t count)
{
    uint16_t* result = new uint16_t[count];

    for (size_t i = 0; i < count; ++i)
    {
        float clamped = std::clamp(float_data[i], 0.0f, 1.0f);
        result[i] = static_cast<uint16_t>(clamped * 65535.0f + 0.5f); // +0.5f for rounding
    }

    return result;
}

int Raytracing::ImageReader::width()  const
{
    return image_width;
}

int Raytracing::ImageReader::height() const
{
    return image_height;
}

Raytracing::ImageDataType Raytracing::ImageReader::get_data_type(int bit_depth) const
{
    if (bit_depth == 8)
        return ImageDataType::UINT8_T;

    if (bit_depth == 16)
        return ImageDataType::UINT16_T;

    if (bit_depth == 32)
        return ImageDataType::FLOAT;

    string error = Logger::error("IMAGE_READER", std::format("The texture you are trying to parse has invalid or unavailable bit depth of {} bits per channel. Only 8, 16 and 32 bit formats are supported", bit_depth));
    throw std::runtime_error(error);
}

Raytracing::ImageDataType Raytracing::ImageReader::get_data_type(const string& filename) const
{
    if (stbi_is_hdr(filename.c_str()))
        return ImageDataType::FLOAT;

    if (stbi_is_16_bit(filename.c_str()))
        return ImageDataType::UINT16_T;

    // Assume it is 8 bit
    return ImageDataType::UINT8_T;
}

