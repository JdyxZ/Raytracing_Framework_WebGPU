// Raytracing Headers
#include "core/core.hpp"
#include "raytracing_engine.hpp"
#include "utils/utilities.hpp"
#include "materials/texture.hpp"
#include "graphics/camera.hpp"

// Framework Headers
#include "framework/nodes/environment_3d.h"
#include "graphics/raytracing_renderer.hpp"
#include "graphics/renderer_storage.h"
#include "framework/parsers/parse_gltf.h"
#include "engine/scene.h"
#include "framework/utils/tinyfiledialogs.h"

// Usings
using Raytracing::SkyboxTexture;
using Raytracing::RendererSettings;

int RayTracingEngine::initialize(Renderer* renderer, sEngineConfiguration configuration)
{
	int error = Engine::initialize(renderer, configuration);

    if (error) return error;

    this->renderer = dynamic_cast<RayTracingRenderer*>(renderer);

    return error;
}

int RayTracingEngine::post_initialize()
{
    create_scene();

    return Engine::post_initialize();
}

void RayTracingEngine::clean()
{
    Engine::clean();
}

void RayTracingEngine::update(float delta_time)
{
    Engine::update(delta_time);

    main_scene->update(delta_time);
}

void RayTracingEngine::render()
{
    if (show_imgui)
        render_gui();

    main_scene->render();

    Engine::render();
}

void RayTracingEngine::create_scene()
{
    // Create scene
    main_scene = new Scene("main_scene");
    vector<Node*> scene_nodes;

    // Create scene environment (background)
    Environment3D* skybox = new Environment3D();
    main_scene->add_node(skybox);

    // Parse skybox
    SkyboxTexture* skybox_texture = parse_skybox(skybox);
    settings.skybox = skybox_texture;

    // Parse scene models
    GltfParser gltf_parser;
    // gltf_parser.parse("models/github_assets/Cube/glTF/Cube.gltf", scene_nodes, PARSE_GLTF_FILL_SURFACE_DATA);
    // gltf_parser.parse("models/github_assets/CarbonFibre/glTF-Binary/CarbonFibre.glb", scene_nodes, PARSE_GLTF_FILL_SURFACE_DATA);
    // gltf_parser.parse("models/github_assets/DamagedHelmet/glTF-Binary/DamagedHelmet.glb", scene_nodes, PARSE_GLTF_FILL_SURFACE_DATA);
    // gltf_parser.parse("models/github_assets/Avocado/glTF-Binary/Avocado.glb", scene_nodes, PARSE_GLTF_FILL_SURFACE_DATA);
    gltf_parser.parse("models/github_assets/Corset/glTF-Binary/Corset.glb", scene_nodes, PARSE_GLTF_FILL_SURFACE_DATA);
    // gltf_parser.parse("models/github_assets/BarramundiFish/glTF-Binary/BarramundiFish.glb", scene_nodes, PARSE_GLTF_FILL_SURFACE_DATA);
    // gltf_parser.parse("models/sponza/scene.gltf", scene_nodes, PARSE_GLTF_FILL_SURFACE_DATA);
    

    // Add models to scene
    main_scene->add_nodes(scene_nodes);
}

void RayTracingEngine::render_gui()
{
    // Aux vars
    bool active = true;

    // Get webgpu context
    WebGPUContext* webgpu_context = renderer->get_webgpu_context();

    // Enum names
    vector<const char*> image_format_names = get_enum_names<IMAGE_FORMAT>();
    vector<const char*> background_type_names = get_enum_names<BACKGROUND_TYPE>();

    // Set window position
    float panel_width = webgpu_context->screen_width * 0.20f; // 20% of screen width
    float panel_height = webgpu_context->screen_height * 0.5f; // 50% of screen height
    ImVec2 window_position = ImVec2(
        static_cast<float>(webgpu_context->screen_width),
        18.0f
    );

    ImGui::SetNextWindowPos(window_position, ImGuiCond_Always, ImVec2(1.0f, 0.0f));

    // Set window size
    ImVec2 min_window_size = ImVec2(panel_width, panel_height);
    ImVec2 max_window_size = ImVec2(FLT_MAX, FLT_MAX); // No maximum limit

    ImGui::SetNextWindowSizeConstraints(min_window_size, max_window_size);

    // Debug panel
    ImGui::Begin("Debug panel", &active, ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

    // Debug panel tabs
    if (ImGui::BeginTabBar("TabBar", ImGuiTabBarFlags_None))
    {
        if (ImGui::BeginTabItem("RayTracer"))
        {
            // === RENDER SETTINGS ===
            ImGui::Text("Render");

            ImGui::SliderInt("Max Bounce Depth", &settings.bounce_max_depth, 1, 1000);
            ImGui::SliderFloat("Min Hit Distance", &settings.min_hit_distance, 0.001f, 10.0f);
            ImGui::SliderInt("Samples per Pixel", &settings.samples_per_pixel, 1, 1000);

            ImGui::NewLine();

            // === OPTIMIZATIONS ===
            ImGui::Checkbox("BVH", &settings.bvh_optimization);
            ImGui::Checkbox("Russian Roulette", &settings.russian_roulette);
            ImGui::Checkbox("Parallel Computation", &settings.parallelize);

            ImGui::NewLine();

            // === BACKGROUND SETTINGS ===
            ImGui::Text("Background");

            ImGui::Combo("Type", (int*)&settings.background_type, background_type_names.data(), int(background_type_names.size()));

            switch (settings.background_type)
            {
            case BACKGROUND_TYPE::STATIC_COLOR:
                ImGui::ColorEdit3("Color", (float*)&settings.background_color);
                break;
            case BACKGROUND_TYPE::GRADIENT:
                ImGui::ColorEdit3("Primary Blend Color", (float*)&settings.primary_blend_color);
                ImGui::ColorEdit3("Secondary Blend Color", (float*)&settings.secondary_blend_color);
                break;
            case BACKGROUND_TYPE::SKYBOX:
                // TO BE IMPLEMENTED
                break;
            }

            ImGui::NewLine();

            // === IMAGE OUTPUT SETTINGS ===
            ImGui::Text("Image");

            if (ImGui::Button("Path"))
            {
                char const* output_path = tinyfd_selectFolderDialog(
                    "Select Image Output Folder",
                    settings.image_path.c_str()
                );

                if (output_path)
                    settings.image_path = output_path;
            }

            ImGui::SameLine();
            ImGui::TextUnformatted(settings.image_path.c_str());

            ImGui::Combo("Format", (int*)&settings.format, image_format_names.data(), int(image_format_names.size()));

            if (settings.format == IMAGE_FORMAT::JPG)
            {
                ImGui::SliderInt("Quality", &settings.quality, 1, 100, "%d");
            }

            ImGui::NewLine();

            // === GENERATE ===
            ImGui::Text("Generate");

            bool is_rendering = renderer->is_rendering.load();

            if (is_rendering)
            {
                // Apply a faded style and disable the button
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f); // Make it look faded
                ImGui::BeginDisabled(true); // Disable interaction
            }

            if (ImGui::Button("Render"))
            {
                // Just in case: If previous thread exists and is joinable, join it before starting a new one
                if (render_jthread.joinable())
                {
                    render_jthread.request_stop(); // Request stop just in case
                    render_jthread.join();         // Wait for it to finish
                }

                // Parse scene nodes to meshes for raytracer
                vector<Node*> scene_nodes = main_scene->get_nodes();
                shared_ptr<ParsedScene> parsed_scene = parse_nodes(scene_nodes, settings.bvh_optimization);

                // Parse camera data
                Camera* engine_camera = renderer->get_camera();
                settings.camera_data = parse_camera_data(engine_camera);

                // Get screen dimensions
                WebGPUContext* webgpu_context = renderer->get_webgpu_context();
                settings.screen_width = webgpu_context->screen_width;
                settings.screen_height = webgpu_context->screen_height;

                // Start rendering params
                renderer->is_rendering = true;
                renderer->render_progress = 0.0f;

                // Start rendering thread
                render_jthread = std::jthread
                (
                    [this, renderer = this->renderer, parsed_scene](std::stop_token stoken)
                    {
                        renderer->render_frame_async(stoken, parsed_scene, settings);
                    }
                );

                /*
                render_jthread = std::jthread
                (
                    &RayTracingRenderer::render_frame_async,    // Render function pointer
                    &renderer,                                  // Pointer to the RayTracingRenderer instance
                    parsed_scene,                               // Scene data
                    settings                                    // Render settings
                );
                */


                // No need to detach/join manually (unless starting a new one)    
            }

            if (is_rendering)
            {
                ImGui::EndDisabled();    // Re-enable interaction for subsequent widgets
                ImGui::PopStyleVar();    // Restore alpha
            }

            if (is_rendering)
            {
                ImGui::SameLine();

                // Cancel button
                if (ImGui::Button("Cancel"))
                {
                    if (render_jthread.joinable())
                    {
                        render_jthread.request_stop(); // Use jthread's cancellation
                    }
                }

                ImGui::NewLine();

                // Get progress status
                float progress = std::clamp(renderer->render_progress.load(), 0.0f, 1.0f);

                // Covert progress to percentage string with 2 decimals
                string percentage = std::format("{:.2f}%", progress * 100);

                // Define progress bar size
                ImVec2 bar_size(0.0f, 0.0f);  // Let the bar auto-size to fit the container
                // ImVec2 bar_size(ImGui::GetContentRegionAvail().x * 0.7f, 0.0f); 

                // Push progress bar
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5, 5));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);  // Rounded corners for the bar
                ImGui::SameLine();
                ImGui::ProgressBar(progress, bar_size, percentage.c_str());
                ImGui::PopStyleVar(2); // Pop both styling variables
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();

    render_default_gui();
}
