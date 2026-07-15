#include <cstdio>
#include <cstdlib>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "simulation.h"
#include "renderer.h"

const int WINDOW_W = 2560;
const int WINDOW_H = 1440;

float cam_x = 1280.0f;
float cam_y = 720.0f;
float cam_zoom = 1.0f;
float cam_zoom_target = 1.0f;

double prev_mouse_x = 0, prev_mouse_y = 0;
bool mouse_dragging = false;

static Simulation* g_sim = nullptr;
static Renderer*   g_renderer = nullptr;

static void error_callback(int error, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", error, desc);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (action != GLFW_PRESS) return;
    if (!g_sim || !g_renderer) return;

    switch (key) {
    case GLFW_KEY_R:
        g_sim->reset();
        g_renderer->resetSSBOs(g_sim);
        printf("Reset\n");
        break;

    case GLFW_KEY_F:
        g_sim->randomize_forces();
        g_renderer->updateForcesSSBO(g_sim);
        printf("Forces randomized\n");
        break;

    case GLFW_KEY_C:
        g_sim->randomize_colors();
        printf("Colors randomized\n");
        break;

    case GLFW_KEY_X:
        g_sim->regenerate();
        g_renderer->resetSSBOs(g_sim);
        printf("Full random -- %d particles, %d types, r=%.0f, R=%.0f\n",
            g_sim->particle_count, g_sim->particle_types,
            g_sim->repulsion_radius, g_sim->interaction_radius);
        break;
    }
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    cam_zoom_target += 0.25f * (float)yoffset * cam_zoom_target;
    if (cam_zoom_target < 0.5f) cam_zoom_target = 0.5f;
    if (cam_zoom_target > 10.0f) cam_zoom_target = 10.0f;
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            mouse_dragging = true;
            glfwGetCursorPos(window, &prev_mouse_x, &prev_mouse_y);
        } else {
            mouse_dragging = false;
        }
    }
}

int main() {
    glfwSetErrorCallback(error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to init GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_W, WINDOW_H, "Particle Life", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to init GLEW\n");
        return -1;
    }

    printf("OpenGL: %s\n", glGetString(GL_VERSION));
    printf("GLSL:   %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    glfwSetKeyCallback(window, key_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    Simulation sim;
    sim.reset();
    g_sim = &sim;

    Renderer renderer;
    g_renderer = &renderer;
    if (!renderer.init(window, &sim)) {
        fprintf(stderr, "Failed to init renderer\n");
        return -1;
    }
    glfwSetWindowUserPointer(window, &sim);

    double last_time = glfwGetTime();
    glClearColor(0.123f, 0.126f, 0.153f, 1.0f);

    while (!glfwWindowShouldClose(window)) {
        double t = glfwGetTime();
        float dt = (float)(t - last_time);
        last_time = t;
        if (dt > 0.05f) dt = 0.05f;

        if (mouse_dragging) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            double dx = mx - prev_mouse_x;
            double dy = my - prev_mouse_y;
            cam_x -= (float)dx / cam_zoom;
            cam_y -= (float)dy / cam_zoom;
            prev_mouse_x = mx;
            prev_mouse_y = my;
        }

        cam_zoom += (cam_zoom_target - cam_zoom) * dt * 4.0f;
        renderer.updateOffsets(cam_x, cam_y, cam_zoom);

        glClear(GL_COLOR_BUFFER_BIT);
        renderer.render(dt);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
