#include "simulation.h"
#include <algorithm>
#include <array>

Simulation::Simulation() {
    std::random_device rd;
    rng.seed(rd());
}

void Simulation::reset(int seed_val) {
    if (seed_val < 0) {
        std::random_device rd;
        rng.seed(rd());
    } else {
        rng.seed(seed_val);
    }
    generate_random_force_matrix();
    generate_particles();
}

void Simulation::generate_particles() {
    particles.clear();
    particles.resize(particle_count);

    // Spawn concentrated in the center 30% area, leaving room to spread
    float cx = region_width * 0.5f, cy = region_height * 0.5f;
    float hw = region_width * 0.15f, hh = region_height * 0.15f;
    std::uniform_real_distribution<float> dist_x(cx - hw, cx + hw);
    std::uniform_real_distribution<float> dist_y(cy - hh, cy + hh);
    std::uniform_int_distribution<int> dist_type(0, particle_types - 1);

    for (int i = 0; i < particle_count; ++i) {
        Particle& p = particles[i];
        p.x = dist_x(rng);
        p.y = dist_y(rng);
        p.vx = 0.0f;
        p.vy = 0.0f;
        p.type = dist_type(rng);
    }
}

void Simulation::generate_random_force_matrix() {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (int i = 0; i < MAX_PARTICLE_TYPES * MAX_PARTICLE_TYPES; ++i)
        forces[i] = dist(rng);
    update_point_sizes();
}

void Simulation::update_point_sizes() {
    // Compute total force involvement per type
    float sums[MAX_PARTICLE_TYPES] = {0};
    for (int t = 0; t < MAX_PARTICLE_TYPES; ++t) {
        float total = 0.0f;
        for (int j = 0; j < MAX_PARTICLE_TYPES; ++j)
            total += std::abs(forces[t * MAX_PARTICLE_TYPES + j]);
        for (int i = 0; i < MAX_PARTICLE_TYPES; ++i)
            total += std::abs(forces[i * MAX_PARTICLE_TYPES + t]);
        sums[t] = total;
    }
    // Sort active types by involvement descending
    int order[MAX_PARTICLE_TYPES];
    for (int t = 0; t < MAX_PARTICLE_TYPES; ++t) order[t] = t;
    int n = particle_types;
    std::sort(order, order + n, [&](int a, int b) { return sums[a] > sums[b]; });
    // Fixed LUT: evenly spaced from 20.0 (most involved) down to 4.0 (least)
    const float SIZES[] = {20.f, 17.f, 14.5f, 12.5f, 10.5f, 8.5f, 7.f, 5.5f, 4.5f, 4.f};
    static_assert(sizeof(SIZES)/sizeof(SIZES[0]) == MAX_PARTICLE_TYPES, "SIZES must match MAX_PARTICLE_TYPES");
    for (int r = 0; r < n; ++r)
        point_sizes[order[r]] = SIZES[r];
    for (int t = n; t < MAX_PARTICLE_TYPES; ++t)
        point_sizes[t] = 4.0f;
}

void Simulation::randomize_forces() {
    generate_random_force_matrix();
}

void Simulation::randomize_colors() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    auto hsv_to_rgb = [](float h, float s, float v) -> std::array<float, 3> {
        int hi = int(h * 6.0f);
        float f = h * 6.0f - float(hi);
        float p = v * (1.0f - s);
        float q = v * (1.0f - f * s);
        float t = v * (1.0f - (1.0f - f) * s);
        switch (hi % 6) {
            case 0: return {v, t, p};
            case 1: return {q, v, p};
            case 2: return {p, v, t};
            case 3: return {p, q, v};
            case 4: return {t, p, v};
            default: return {v, p, q};
        }
    };

    auto color_dist = [](float h1, float s1, float v1, float h2, float s2, float v2) -> float {
        // Weighted hue difference (circular), scaled by saturation/value importance
        float dh = std::abs(h1 - h2);
        if (dh > 0.5f) dh = 1.0f - dh;
        // Perceptual distance: hue difference gets stronger when s & v are high
        float hue_weight = (s1 + s2 + v1 + v2) * 0.25f;
        float ds = std::abs(s1 - s2);
        float dv = std::abs(v1 - v2);
        return dh * (0.5f + 0.5f * hue_weight) * 2.0f + ds + dv;
    };

    // Store HSVs for distance checking
    float hues[MAX_PARTICLE_TYPES];
    float sats[MAX_PARTICLE_TYPES];
    float vals[MAX_PARTICLE_TYPES];
    int generated = 0;

    for (int i = 0; i < MAX_PARTICLE_TYPES; ++i) {
        float h, s, v;
        int attempt = 0;
        const int MAX_ATTEMPTS = 50;
        bool accepted = false;

        while (!accepted && attempt < MAX_ATTEMPTS) {
            h = dist(rng);
            s = 0.7f + dist(rng) * 0.3f;
            v = 0.7f + dist(rng) * 0.3f;
            accepted = true;

            // Check against all previously generated colors
            for (int j = 0; j < generated; ++j) {
                float d = color_dist(h, s, v, hues[j], sats[j], vals[j]);
                if (d < 0.55f) {
                    accepted = false;
                    break;
                }
            }
            attempt++;
        }

        hues[i] = h;
        sats[i] = s;
        vals[i] = v;
        generated++;

        auto rgb = hsv_to_rgb(h, s, v);
        colors[i][0] = rgb[0];
        colors[i][1] = rgb[1];
        colors[i][2] = rgb[2];
        colors[i][3] = 1.0f;
    }
}

void Simulation::randomize_parameters() {
    std::uniform_int_distribution<int> dist_types(3, MAX_PARTICLE_TYPES);
    std::uniform_int_distribution<int> dist_count(750, 3000);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    particle_types = dist_types(rng);
    particle_count = dist_count(rng);
    dampening = 0.4f + dist(rng) * 0.4f;
    repulsion_radius = 15.0f + dist(rng) * 50.0f;
    interaction_radius = 80.0f + dist(rng) * 140.0f;
    density_limit = 2.0f + dist(rng) * 10.0f;
}

void Simulation::regenerate() {
    randomize_parameters();
    generate_random_force_matrix();
    randomize_colors();
    generate_particles();
}
