#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "simulation.h"

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(GLFWwindow* window, Simulation* sim);
    void render(float dt);
    void resetSSBOs(Simulation* sim);
    void updateOffsets(float cx, float cy, float zoom);
    void updateForcesSSBO(Simulation* sim);
    void updateColorsUniform();

private:
    // Rendering (VBO/VAO)
    GLuint vao = 0;
    GLuint render_program = 0;
    GLint uOffset_loc;
    GLint uScale_loc;

    // Compute shader
    GLuint compute_program = 0;

    // SSBOs (double-buffered for pos & vel, single for types & forces)
    GLuint pos_ssbo[2] = {0, 0};
    GLuint vel_ssbo[2] = {0, 0};
    GLuint type_ssbo = 0;
    GLuint force_ssbo = 0;

    // Number of particles allocated in SSBOs
    int buf_capacity = 0;

    // Current read/write indices (ping-pong)
    int frame_index = 0;

    // Reference to simulation (for dynamic colors, types etc.)
    Simulation* sim_ref = nullptr;

    // Camera
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale = 1.0f;

    // Helpers
    GLuint compile_shader(const char* source, GLenum type);
    GLuint link_program(GLuint vert, GLuint frag);
    GLuint link_compute_program(const char* comp_src);
    void create_ssbo(int n, Simulation* sim);
    void destroy_ssbo();
    std::string load_source(const char* path);
};
