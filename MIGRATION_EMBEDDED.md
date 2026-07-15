# Embedded Linux 移植方案 — ParticleLifeGL

将 ParticleLifeGL 从 Windows Desktop (OpenGL 4.3) 移植到 Embedded Linux 的完整评估与执行计划。

## 1. 架构差异总览

| 当前依赖 | Embedded Linux 替代 | 原因 |
|---|---|---|
| OpenGL 4.3 Core + GLSL 430 | OpenGL ES 3.1 (推荐) / ES 2.0 (兼容) | 嵌入式 GPU 多数仅支持 ES |
| GLFW (窗口+上下文+输入) | SDL2 (kmsdrm / wayland) 或 EGL + libinput | GLFW 的 fbdev 支持已废弃 |
| GLEW (扩展加载) | glad2-ES / eglGetProcAddress | GLEW 不支持 OpenGL ES |
| x86-64 Windows | aarch64 / armv7l Linux | 嵌入式目标平台 |
| 原生桌面窗口系统 | DRM/KMS / Wayland / X11 | 取决于板级 BSP |

## 2. 移植路线对比

### 路线 A：OpenGL ES 3.1 + Compute Shader（推荐）

适用于 **Mali G52/G72、Vivante GC7000、Intel HD** 等支持 ES 3.1 的 GPU。

**优点**：物理仍在 GPU 运行，性能最高，代码改动最小。  
**缺点**：要求 ES 3.1 计算着色器，部分老 GPU 不可用。

### 路线 B：CPU 物理 + OpenGL ES 2.0 渲染（最通用）

适用于 **Mali-400/450、无 GPU compute** 或纯软件渲染场景。

**优点**：硬件兼容性最广。  
**缺点**：需将 compute shader 迁到 CPU 并行执行，工作量翻倍。

### 场景推荐

| 硬件平台 | 推荐路线 | 原因 |
|---|---|---|
| Raspberry Pi 4/5 | **A** | VideoCore VI 支持 ES 3.1 |
| Raspberry Pi 3 | **B** | VideoCore IV 仅 ES 2.0 |
| Rockchip RK3588 | **A** | Mali G52 完整支持 ES 3.1 |
| Allwinner A64 (Mali-400) | **B** | 仅 ES 2.0 |
| NXP i.MX8M Plus (GC7000) | **A** | Vivante ES 3.1 支持 |
| 任意 x86-64 (Intel HD) | **A** | Mesa EGL + ES 3.1 |
| 无 GPU / 纯 CPU | **B** | SDL2 软件渲染器回退 |

## 3. 路线 A 详细步骤 (ES 3.1)

### 3.1 Shader 移植

**vertex shader** (`particle.vert`):

```glsl
#version 310 es          // 原 #version 430 core
precision highp float;
precision highp int;
```

改动清单：
- `#version 430` → `#version 310 es`
- 所有 `float`/`int` 前加 `precision highp`
- `layout(location = N) in` → 部分驱动需 `glBindAttribLocation()`
- uniform 数组语法不变

**fragment shader** (`particle.frag`):

```glsl
#version 310 es
precision mediump float;
in vec4 vColor;
out vec4 FragColor;
// 原逻辑不变
```

**compute shader** (`physics.comp`):

```glsl
#version 310 es
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
precision highp float;
precision highp int;
```

ES 3.1 支持的 compute 特性：
- `layout(std430)` SSBO
- `gl_GlobalInvocationID`
- `barrier()` / `memoryBarrier()`
- `readonly` / `writeonly`

### 3.2 窗口系统：GLFW → SDL2

```c
SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "kmsdrm");
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
```

| GLFW 函数 | SDL2 替代 |
|---|---|
| `glfwSetKeyCallback()` | `SDL_PollEvent()` → `SDL_KEYDOWN` |
| `glfwSetScrollCallback()` | `SDL_MOUSEWHEEL` |
| `glfwSetMouseButtonCallback()` | `SDL_MOUSEBUTTONDOWN/UP` |
| `glfwGetCursorPos()` | `SDL_GetMouseState()` 或 event pos |
| `glfwGetTime()` | `SDL_GetTicks() / 1000.0f` |
| `glfwSwapBuffers()` | `SDL_GL_SwapWindow()` |
| `glfwPollEvents()` | `SDL_PollEvent()` |

### 3.3 扩展加载：GLEW → glad2

```bash
python -m glad --profile es --api "gles3.1" --generator c --out-path glad/
```

替换：
```c
// glewExperimental = GL_TRUE; glewInit();
if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress))
    return -1;
```

### 3.4 构建系统调整

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLESV2 REQUIRED glesv2)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_executable(ParticleLifeGL src/main.cpp src/simulation.cpp src/renderer.cpp glad/src/glad.c)
target_include_directories(ParticleLifeGL PRIVATE src ${SDL2_INCLUDE_DIRS} ${GLESV2_INCLUDE_DIRS} ${EGL_INCLUDE_DIRS} glad/include)
target_link_libraries(ParticleLifeGL PRIVATE ${SDL2_LIBRARIES} ${GLESV2_LIBRARIES} ${EGL_LIBRARIES} pthread)
```

### 3.5 Renderer API 适配

| OpenGL 4.3 | OpenGL ES 3.1 | 兼容性 |
|---|---|---|
| `glMemoryBarrier(...)` | 相同 | 直接可用 |
| `glDispatchCompute()` | 相同 | 直接可用 |
| `glBindBufferBase(GL_SHADER_STORAGE_BUFFER)` | 相同 | 直接可用 |
| `glEnable(GL_PROGRAM_POINT_SIZE)` | 不需要 | ES 默认开启 |
| `glUniform1ui` | 部分不支持 | 改为 `float` + `int()` |

### 3.6 已知陷阱

- **`u_particle_types` 为 uint**：部分 ES 3.1 驱动报错，改为 `float` + `int()`
- **`gl_PointSize` 上限**：ES 限制到 `GL_ALIASED_POINT_SIZE_RANGE`，通常 ≤ 64px
- **`static_assert` 在 shader 中**：ES 不支持，去掉
- **SSBO 初始大小**：ES 3.1 的 `glBufferData` 要求 `max(1, bytes)`，传 0 会崩溃

## 4. 路线 B 详细步骤 (ES 2.0 + CPU 物理)

### 4.1 物理引擎 CPU 化

将 `physics.comp` 迁移到 `simulation.cpp`，用 C++ + OpenMP 重写：

```cpp
void Simulation::tick(float dt) {
    #pragma omp parallel for
    for (int i = 0; i < particle_count; ++i) {
        // Pass 1: density (同原 compute shader)
        // Pass 2: force + integrate (同原 compute shader)
    }
}
```

**并行策略选项**：

| 方法 | 适用 | 预期加速 |
|---|---|---|
| `#pragma omp parallel for` | 多核 ARM (RK3588) | ~4-6x |
| `std::thread` + 手动分块 | 小核数 (RPi4) | ~3-4x |
| NEON SIMD | 固定类型数场景 | ~2x 额外 |

**性能估算**（1.8GHz Cortex-A72, 4核）：

| 粒子数 | O(n²) 操作数 | 4线程耗时 | 帧率 |
|---|---|---|---|
| 500 | 250k | ~1ms | 60fps |
| 1000 | 1M | ~3ms | 60fps |
| 2000 | 4M | ~8ms | 60fps |
| 3000 | 9M | ~15ms | ~55fps |

### 4.2 Shader 简化 (`#version 100`)

**vertex shader**：
```glsl
attribute vec2 aPos;
uniform vec2 uOffset;
uniform vec2 uScale;
uniform vec4 uColors[10];
uniform float uPointSizes[10];
varying vec4 vColor;
void main() {
    gl_Position = ...;  // 位置计算
    gl_PointSize = uPointSizes[int(type)];
    vColor = uColors[int(type)];
}
```

**fragment shader**：
```glsl
precision mediump float;
varying vec4 vColor;
void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    if (dist > 0.5) discard;
    float alpha = 1.0 - smoothstep(0.3, 0.5, dist);
    gl_FragColor = vec4(vColor.rgb, alpha * 0.85);
}
```

### 4.3 Renderer 简化

- 去掉所有 SSBO 分配和绑定
- 去掉 `compute_program`
- 每帧 `tick()` 跑 CPU 物理，然后上传 pos 到 VBO
- 颜色/点大小用 uniform 每帧上传

### 4.4 额外注意

- ES 2.0 不一定支持 `gl_PointSize` — 需 `GL_OES_point_sprite` 扩展
- `gl_PointCoord` 需要 `GL_OES_point_sprite`
- 若点精灵不可用，用四边形替代（GL_TRIANGLE_STRIP + 计算顶点位置）

## 5. 测试与验证清单

| 序号 | 验证项 | 预期结果 |
|---|---|---|
| 1 | 程序启动，显示粒子 | 黑底上粒子正确渲染 |
| 2 | R 键重置 | 粒子重新初始化 |
| 3 | F 键随机力矩阵 | 粒子运动行为改变 |
| 4 | C 键随机颜色 | 颜色更新，无明显近似色 |
| 5 | X 键完全重随机 | 参数、力、颜色、位置全变 |
| 6 | 鼠标拖拽平移 | 视口平滑移动 |
| 7 | 滚轮缩放 | 缩放平滑，范围 0.5-10x |
| 8 | 连续运行 1 小时 | 无内存泄漏、无崩溃 |
| 9 | 退出 | 正常退出，无撕裂 |

## 6. 排错指南

### Shader 编译失败
- `glGetShaderInfoLog` 检查具体错误
- 常见：`precision` 缺失、`uint` 不支持、变量名与关键字冲突

### 空白屏幕
- `glGetError()` 循环检查
- 确认 `glClearColor` 生效
- 检查 SSBO 绑定索引是否冲突

### 性能不达标
- 用 `glFinish` + 计时器测量 GPU 耗时
- CPU 路线：检查是否开启 `-O2` / `-O3`
- 减少 `particle_count` 上限或 `particle_types` 数量

## 7. 附录

### 7.1 参考资源

- [OpenGL ES 3.1 Specification](https://registry.khronos.org/OpenGL/specs/es/3.1/es_spec_3.1.pdf)
- [Mali GPU Application Developer Guide](https://developer.arm.com/documentation/102613)
- [SDL2 KMSDRM Backend](https://wiki.libsdl.org/SDL2/README/drm)
- [glad2 — Multi-language GL/GLES loader](https://glad.dav1d.de/)

### 7.2 快速交叉编译示例

```bash
# 安装交叉工具链
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# CMake 交叉编译
cmake -S . -B build-arm \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
    -DCMAKE_FIND_ROOT_PATH=/usr/aarch64-linux-gnu
cd build-arm && make -j$(nproc)
```
