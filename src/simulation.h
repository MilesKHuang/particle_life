#pragma once
#include <vector>
#include <random>

constexpr int MAX_PARTICLE_TYPES = 10;

struct Particle {
    float x, y;
    float vx, vy;
    int type;
};

class Simulation {
public:
    Simulation();
    ~Simulation() = default;

    void reset(int seed_val = -1);
    void randomize_forces();
    void randomize_colors();
    void randomize_parameters();
    void regenerate(); // new random seed, all parameters + particles

    // Parameters
    int particle_count = 1000;
    int particle_types = 5;
    float dampening = 0.5f;
    float repulsion_radius = 30.0f;
    float interaction_radius = 100.0f;
    float density_limit = 5.0f;
    float region_width = 2560.0f;
    float region_height = 1440.0f;

    // Data
    std::vector<Particle> particles;
    float forces[MAX_PARTICLE_TYPES * MAX_PARTICLE_TYPES] = {0};
    float point_sizes[MAX_PARTICLE_TYPES] = {6.0f};
    float colors[MAX_PARTICLE_TYPES][4] = {
        {0.0f, 1.0f, 1.0f, 1.0f},   // 0  cyan
        {1.0f, 0.0f, 0.0f, 1.0f},   // 1  red
        {0.0f, 1.0f, 0.0f, 1.0f},   // 2  green
        {1.0f, 0.0f, 1.0f, 1.0f},   // 3  magenta
        {1.0f, 1.0f, 0.0f, 1.0f},   // 4  yellow
        {0.0f, 0.0f, 1.0f, 1.0f},   // 5  blue
        {1.0f, 0.5f, 0.0f, 1.0f},   // 6  orange
        {0.5f, 0.0f, 1.0f, 1.0f},   // 7  purple
        {0.0f, 1.0f, 0.5f, 1.0f},   // 8  teal
        {1.0f, 1.0f, 1.0f, 1.0f},   // 9  white
    };

    void update_point_sizes();

private:
    std::mt19937 rng;
    void generate_particles();
    void generate_random_force_matrix();
};
