#include "renderer.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

std::string Renderer::load_source(const char* path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open: %s\n", path);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint Renderer::compile_shader(const char* source, GLenum type) {
    if (!source || !source[0]) return 0;
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        const char* name = (type == GL_VERTEX_SHADER)   ? "VERT"
                         : (type == GL_FRAGMENT_SHADER) ? "FRAG"
                         : (type == GL_COMPUTE_SHADER)  ? "COMP" : "?";
        fprintf(stderr, "Shader error (%s): %s\n", name, info);
        return 0;
    }
    return shader;
}

GLuint Renderer::link_program(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char info[512];
        glGetProgramInfoLog(prog, 512, nullptr, info);
        fprintf(stderr, "Link error: %s\n", info);
        return 0;
    }
    return prog;
}

GLuint Renderer::link_compute_program(const char* src) {
    GLuint cs = compile_shader(src, GL_COMPUTE_SHADER);
    if (!cs) return 0;
    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char info[512];
        glGetProgramInfoLog(prog, 512, nullptr, info);
        fprintf(stderr, "Compute link error: %s\n", info);
        return 0;
    }
    glDeleteShader(cs);
    return prog;
}

// --- constructor / destructor ---

Renderer::Renderer() {}

Renderer::~Renderer() {
    if (render_program) glDeleteProgram(render_program);
    if (compute_program) glDeleteProgram(compute_program);
    if (vao) glDeleteVertexArrays(1, &vao);
    destroy_ssbo();
}

// --- SSBO management ---

void Renderer::destroy_ssbo() {
    for (int i = 0; i < 2; ++i) {
        if (pos_ssbo[i]) { glDeleteBuffers(1, &pos_ssbo[i]); pos_ssbo[i] = 0; }
        if (vel_ssbo[i]) { glDeleteBuffers(1, &vel_ssbo[i]); vel_ssbo[i] = 0; }
    }
    if (type_ssbo)  { glDeleteBuffers(1, &type_ssbo);  type_ssbo = 0; }
    if (force_ssbo) { glDeleteBuffers(1, &force_ssbo); force_ssbo = 0; }
    buf_capacity = 0;
}

void Renderer::create_ssbo(int n, Simulation* sim) {
    destroy_ssbo();
    buf_capacity = n;

    size_t sz = sizeof(float) * 2 * n;

    auto make_ssbo = [](const void* data, size_t bytes, GLenum usage) -> GLuint {
        GLuint id;
        glGenBuffers(1, &id);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, id);
        glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, data, usage);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        return id;
    };

    std::vector<float> pos_data(n * 2);
    std::vector<float> vel_data(n * 2);
    std::vector<uint32_t> type_data(n);
    for (int i = 0; i < n; ++i) {
        pos_data[i*2+0] = sim->particles[i].x;
        pos_data[i*2+1] = sim->particles[i].y;
        vel_data[i*2+0] = sim->particles[i].vx;
        vel_data[i*2+1] = sim->particles[i].vy;
        type_data[i]     = (uint32_t)sim->particles[i].type;
    }

    pos_ssbo[0] = make_ssbo(pos_data.data(), sz, GL_DYNAMIC_COPY);
    pos_ssbo[1] = make_ssbo(pos_data.data(), sz, GL_DYNAMIC_COPY);
    vel_ssbo[0] = make_ssbo(vel_data.data(), sz, GL_DYNAMIC_COPY);
    vel_ssbo[1] = make_ssbo(vel_data.data(), sz, GL_DYNAMIC_COPY);
    type_ssbo   = make_ssbo(type_data.data(), sizeof(uint32_t) * n, GL_STATIC_DRAW);
    force_ssbo  = make_ssbo(sim->forces, sizeof(float) * MAX_PARTICLE_TYPES * MAX_PARTICLE_TYPES, GL_STATIC_DRAW);

    frame_index = 0;
}
void Renderer::resetSSBOs(Simulation* sim) {
    int n = sim->particle_count;
    if (n > buf_capacity) { create_ssbo(n, sim); return; }
    std::vector<float> pd(n*2), vd(n*2);
    std::vector<uint32_t> td(n);
    for (int i = 0; i < n; ++i) {
        pd[i*2]=sim->particles[i].x; pd[i*2+1]=sim->particles[i].y;
        vd[i*2]=sim->particles[i].vx; vd[i*2+1]=sim->particles[i].vy;
        td[i]=(uint32_t)sim->particles[i].type;
    }
    size_t sz = sizeof(float)*2*n;
    for (int b=0;b<2;++b) {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, pos_ssbo[b]);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sz, pd.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, vel_ssbo[b]);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sz, vd.data());
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, type_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t)*n, td.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, force_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(float)*MAX_PARTICLE_TYPES*MAX_PARTICLE_TYPES, sim->forces);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    frame_index = 0;
}

bool Renderer::init(GLFWwindow* window, Simulation* sim) {
    auto vs = load_source("src/shaders/particle.vert");
    auto fs = load_source("src/shaders/particle.frag");
    if (vs.empty()||fs.empty()) return false;
    GLuint v=compile_shader(vs.c_str(),GL_VERTEX_SHADER);
    GLuint f=compile_shader(fs.c_str(),GL_FRAGMENT_SHADER);
    if(!v||!f) return false;
    render_program=link_program(v,f); glDeleteShader(v); glDeleteShader(f);
    if(!render_program) return false;
    uOffset_loc=glGetUniformLocation(render_program,"uOffset");
    uScale_loc=glGetUniformLocation(render_program,"uScale");

    auto cs=load_source("src/shaders/physics.comp");
    if(cs.empty()) return false;
    compute_program=link_compute_program(cs.c_str());
    if(!compute_program) return false;

    glGenVertexArrays(1,&vao);
    glBindVertexArray(vao);
    glBindVertexArray(0);

    sim_ref = sim;
    create_ssbo(sim->particle_count, sim);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);
    return true;
}

void Renderer::updateOffsets(float cx, float cy, float zoom) {
    offset_x=cx; offset_y=cy; scale=zoom;
}

void Renderer::updateForcesSSBO(Simulation* sim) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, force_ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
        sizeof(float) * MAX_PARTICLE_TYPES * MAX_PARTICLE_TYPES, sim->forces);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void Renderer::updateColorsUniform() {}

void Renderer::render(float dt) {
    if (buf_capacity == 0) return;
    GLuint n = sim_ref ? (GLuint)sim_ref->particle_count : (GLuint)buf_capacity;
    if (n == 0) return;

    int ri = frame_index % 2, wi = (frame_index + 1) % 2;

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos_ssbo[ri]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vel_ssbo[ri]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, type_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, force_ssbo);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, pos_ssbo[wi]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, vel_ssbo[wi]);

    float sim_dt = dt * 8.0f;

    float damp   = sim_ref ? sim_ref->dampening         : 0.5f;
    float repul  = sim_ref ? sim_ref->repulsion_radius   : 30.f;
    float inter  = sim_ref ? sim_ref->interaction_radius : 100.f;
    float dlimit = sim_ref ? sim_ref->density_limit      : 5.f;
    GLuint ntypes = sim_ref ? (GLuint)sim_ref->particle_types : 5;

    glUseProgram(compute_program);
    glUniform2f(glGetUniformLocation(compute_program,"u_region_size"), 2560.f, 1440.f);
    glUniform1ui(glGetUniformLocation(compute_program,"u_particle_count"), n);
    glUniform1ui(glGetUniformLocation(compute_program,"u_particle_types"), ntypes);
    glUniform1f(glGetUniformLocation(compute_program,"u_dt"), sim_dt);
    glUniform1f(glGetUniformLocation(compute_program,"u_dampening"), damp);
    glUniform1f(glGetUniformLocation(compute_program,"u_repulsion_radius"), repul);
    glUniform1f(glGetUniformLocation(compute_program,"u_interaction_radius"), inter);
    glUniform1f(glGetUniformLocation(compute_program,"u_density_limit"), dlimit);
    glDispatchCompute((GLuint)((n+255)/256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    glUseProgram(render_program);
    glUniform2f(uOffset_loc, -offset_x, -offset_y);
    glUniform2f(uScale_loc, scale, scale);

    if (sim_ref) {
        glUniform4fv(glGetUniformLocation(render_program, "uColors"),
            10, &sim_ref->colors[0][0]);
        glUniform1fv(glGetUniformLocation(render_program, "uPointSizes"),
            10, &sim_ref->point_sizes[0]);
    }

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, pos_ssbo[wi]);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_POINTS, 0, (GLsizei)n);
    glBindVertexArray(0);
    frame_index++;
}
