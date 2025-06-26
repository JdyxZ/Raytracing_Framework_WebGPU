#pragma once

// Headers
#include "core/core.hpp"
#include "vec3.hpp"
#include "onb.hpp"

// Forward declarations
class Hittable;
struct Ray;

struct PDFSampleData
{
    vec3 direction;                                 // Sampled direction in world space
    optional<vec3> halfway_vector = std::nullopt;   // Halfway vector for VNDF sampling
};

class PDF // Probability Distribution Function (PDF)
{ 
public:
    virtual ~PDF() {};

    virtual double value(const PDFSampleData& sample) const = 0;
    virtual PDFSampleData generate(const Ray& incoming_ray) const = 0;
};

class uniform_sphere_pdf : public PDF 
{
public:
    uniform_sphere_pdf();

    double value(const PDFSampleData& sample) const override;
    PDFSampleData generate(const Ray& incoming_ray) const override;
};

class cosine_hemisphere_pdf : public PDF 
{
public:
    cosine_hemisphere_pdf(const vec3& normal); // Generate a orthonormal basis of the hit point surface normal

    double value(const PDFSampleData& sample) const override;
    PDFSampleData generate(const Ray& incoming_ray) const override;

private:
    ONB uvw;
};

class vndf_pdf : public PDF
{
public:
    vndf_pdf(double alpha, double alpha_squared, vec3 view_direction);

    double value(const PDFSampleData& sample) const override;
    PDFSampleData generate(const Ray& incoming_ray) const override;

private:
    double alpha; // Mapped roughness parameter
    double alpha_squared;
    vec3 view_direction;
};

class hittable_pdf : public PDF
{
public:
    hittable_pdf(shared_ptr<Hittable> object, const point3& hit_point);

    double value(const PDFSampleData& sample) const override;
    PDFSampleData generate(const Ray& incoming_ray) const override;

private:
    shared_ptr<Hittable> object;
    point3 hit_point;
};

class hittables_pdf : public PDF 
{
public:
    hittables_pdf(const vector<shared_ptr<Hittable>>& hittables, const point3& hit_point);

    double value(const PDFSampleData& sample) const override;
    PDFSampleData generate(const Ray& incoming_ray) const override;

private:
    const vector<shared_ptr<Hittable>>& hittables;
    point3 hit_point;
};

class mixture_pdf : public PDF 
{
public:
    mixture_pdf(shared_ptr<PDF> p0, shared_ptr<PDF> p1);
    mixture_pdf(shared_ptr<PDF> p0, shared_ptr<PDF> p1, double w0);

    double value(const PDFSampleData& sample) const override;
    PDFSampleData generate(const Ray& incoming_ray) const override;

private:
    shared_ptr<PDF> p[2];// PDFs to mix
    double w[2]; // Weights for the PDFs
};


