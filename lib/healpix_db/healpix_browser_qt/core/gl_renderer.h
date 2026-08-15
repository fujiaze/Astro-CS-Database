// gl_renderer.h - HEALPix 浏览器 OpenGL 渲染核心 (healpix_browser_qt)
// 功能: 球面渲染 (.hcsd) + 单帧切面投影渲染 (.hiss), 内嵌 STF 拉伸着色器
// 用途: 为 widgets/ 层提供纯 C++ OpenGL 3.3 Core 渲染入口, 无 Qt 依赖
// 依赖: browser_backend.h (数据源), stf_engine.h (STF uniform 转换),
// healpix_math.h (球面坐标转换), logger.h (日志)
// 编译: g++ -O2 -std=c++17 -Wall -Wextra -Icore -Iinclude -I../../astro_image_io/include
// -c core/gl_renderer.cpp -o core/gl_renderer.o -lopengl32 -lgdi32
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md §3.4

#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include "browser_backend.h"
#include "stf_engine.h"

// 渲染模式
enum class RenderMode {
    SPHERE,             // 球面渲染（.hcsd，UV 球面网格）
    HISS_POLYGON        // .hiss 像素多边形球面渲染（不展平，每像素 4 角点）
    // SINGLE_FRAME 已废弃（TAN 投影展平方案）
};

// 渲染参数（widget 层填充）
struct RenderParams {
    RenderMode mode;
    ViewParams view;             // 视角
    STFParams stf;               // STF 拉伸参数
    float data_min;              // 数据范围（归一化用）
    float data_max;
    float no_data_value;         // 无数据标记（默认 0.0）
    int viewport_w;              // 视口宽（像素）
    int viewport_h;              // 视口高（像素）
    bool grid_visible = false;   // 经纬线网格可见性 (30° 网格)
};

class GLRenderer {
public:
    GLRenderer();
    ~GLRenderer();

    // 初始化（需在 OpenGL 上下文 makeCurrent 后调用）
    // 返回 0=成功, <0=失败
    int init();
    bool is_initialized() const;

    // 释放资源（OpenGL 上下文销毁前调用）
    void cleanup();

    // 主渲染入口（每帧调用）
    // backend: 数据源（已 open_file）
    // params: 渲染参数
    // 返回 0=成功, <0=失败
    int render(BrowserBackend& backend, const RenderParams& params);

    // 更新 STF 参数（无需重建网格，仅更新 uniform）
    void update_stf(const STFParams& stf, float data_min, float data_max);

    // 单帧模式：设置数据边界框（初始化时计算一次）
    void set_single_frame_bbox(double center_ra, double center_dec,
                               double width_deg, double height_deg);

    // .hiss 像素多边形模式: 获取数据 bbox（初始视角用）
    void get_hiss_bbox(double& center_ra, double& center_dec,
                       double& width_deg, double& height_deg) const {
        center_ra = hiss_center_ra_;
        center_dec = hiss_center_dec_;
        width_deg = hiss_width_deg_;
        height_deg = hiss_height_deg_;
    }

    // 球面模式：获取当前已加载的子叶列表（调试用）
    std::vector<uint64_t> get_loaded_leaves() const;

private:
    // 着色器程序
    unsigned int sphere_program_;     // 球面着色器
    unsigned int quad_program_;       // 单帧四边形着色器

    // 球面网格
    unsigned int sphere_vao_;
    unsigned int sphere_vbo_;
    unsigned int sphere_ibo_;
    int sphere_index_count_;

    // 单帧四边形
    unsigned int quad_vao_;
    unsigned int quad_vbo_;

    // 子叶纹理管理（球面模式）
    struct LeafTexture {
        uint64_t leaf_ipix;
        unsigned int texture_id;
        uint32_t nside;
        uint64_t last_used_frame;
    };
    std::vector<LeafTexture> leaf_textures_;
    uint64_t current_frame_;

    // STF uniform 缓存
    STFParams cached_stf_;
    float cached_data_min_;
    float cached_data_max_;
    float cached_no_data_;

    // 单帧模式数据
    unsigned int single_frame_texture_;
    double sf_center_ra_, sf_center_dec_, sf_width_deg_, sf_height_deg_;
    bool sf_texture_valid_;
    double sf_last_view_ra_, sf_last_view_dec_, sf_last_view_zoom_;  // 视角变化检测
    uint32_t sf_nside_;  // 单帧数据 nside

    // ---- .hiss 像素多边形模式数据（新，替代旧 single_frame） ----
    unsigned int hiss_polygon_vao_ = 0;
    unsigned int hiss_polygon_vbo_ = 0;
    int hiss_polygon_vertex_count_ = 0;
    bool hiss_mesh_valid_ = false;

    // .hiss 数据 bbox（初始视角用）
    double hiss_center_ra_ = 0.0;
    double hiss_center_dec_ = 0.0;
    double hiss_width_deg_ = 0.0;
    double hiss_height_deg_ = 0.0;

    // ---- 经纬线网格 (30° 网格, 独立着色器, 固定颜色) ----
    unsigned int grid_program_ = 0;     // 网格着色器
    unsigned int grid_vao_ = 0;         // 网格 VAO
    unsigned int grid_vbo_ = 0;         // 网格 VBO
    int grid_vertex_count_ = 0;         // 网格顶点数 (线段端点)
    bool grid_mesh_valid_ = false;      // 网格是否已构建

    // 初始化标志
    bool initialized_;

    // 球面顶点 (ra, dec) 缓存（render 时查值用）
    struct SphereVertexCoord {
        double ra;
        double dec;
        // 预计算值 (网格重建时计算, 查值时直接用, 避免每帧重复 cos/sin/ang2pix)
        float x, y, z;           // 笛卡尔坐标 (单位球)
        uint64_t leaf_ipix;      // nside=64 子叶 ipix
    };
    std::vector<SphereVertexCoord> sphere_vertex_coords_;

    // ---- 子叶数据跨帧缓存 (避免每帧重新 load_leaf + ud_grade) ----
    // key: leaf_ipix (nside=64), value: 子叶数据 (零拷贝指针或 owned 降采样数据)
    struct CachedLeaf {
        uint32_t nside = 0;
        const uint64_t* ipix = nullptr;
        const float* pixel = nullptr;       // float32 零拷贝 (use_u8=false)
        const uint8_t* pixel_u8 = nullptr;  // uint8 降采样 (use_u8=true)
        size_t n = 0;
        bool owned = false;  // true=需 free (ud_grade 结果), false=指向 backend 内部
        bool use_u8 = false;
        ~CachedLeaf() { release(); }
        CachedLeaf() = default;
        CachedLeaf(CachedLeaf&& o) noexcept
            : nside(o.nside), ipix(o.ipix), pixel(o.pixel), pixel_u8(o.pixel_u8),
              n(o.n), owned(o.owned), use_u8(o.use_u8) {
            o.owned = false; o.ipix = nullptr; o.pixel = nullptr; o.pixel_u8 = nullptr;
        }
        CachedLeaf& operator=(CachedLeaf&& o) noexcept {
            if (this != &o) {
                release();
                nside = o.nside; ipix = o.ipix; pixel = o.pixel; pixel_u8 = o.pixel_u8;
                n = o.n; owned = o.owned; use_u8 = o.use_u8;
                o.owned = false; o.ipix = nullptr; o.pixel = nullptr; o.pixel_u8 = nullptr;
            }
            return *this;
        }
        void release() {
            if (owned) {
                std::free(const_cast<uint64_t*>(ipix));
                if (use_u8) std::free(const_cast<uint8_t*>(pixel_u8));
                else std::free(const_cast<float*>(pixel));
            }
            ipix = nullptr; pixel = nullptr; pixel_u8 = nullptr;
            n = 0; owned = false; use_u8 = false;
        }
    };
    std::unordered_map<uint64_t, CachedLeaf> leaf_cache_;
    double cache_center_ra_ = -999.0;   // 缓存时的视角 (用于失效检测)
    double cache_center_dec_ = -999.0;
    double cache_fov_ = -999.0;

    // ---- 动态网格跟踪 (检测重建时机) ----
    double mesh_center_ra_ = -999.0;
    double mesh_center_dec_ = -999.0;
    double mesh_fov_ = -999.0;
    int mesh_viewport_w_ = 0;
    int mesh_viewport_h_ = 0;

    // 内部方法
    int compile_shaders();
    void build_sphere_mesh(int segments_lat, int segments_lon);
    // 动态网格: 按 FOV 和视口计算细分度, 顶点密度≈屏幕像素
    // 网格覆盖 FOV×1.2 (渲染余量), 以视角中心为局部 UV 网格
    void build_sphere_mesh_dynamic(const ViewParams& view, int viewport_w, int viewport_h);
    // 检查是否需要重建网格 (FOV 或视口变化超过阈值)
    bool need_rebuild_mesh(const ViewParams& view, int viewport_w, int viewport_h) const;
    void build_quad_mesh();
    unsigned int upload_leaf_texture(const LeafData& leaf);
    void evict_unused_leaves(size_t max_leaves);
    int render_sphere(BrowserBackend& backend, const RenderParams& params);
    int render_single_frame(BrowserBackend& backend, const RenderParams& params);

    // .hiss 像素多边形模式: 网格构建与渲染（新）
    int build_hiss_polygon_mesh(BrowserBackend& backend);
    int render_hiss_polygon(BrowserBackend& backend, const RenderParams& params);

    // 经纬线网格构建与渲染 (30° 网格, 独立着色器)
    int build_grid_mesh();
    int render_grid(const RenderParams& params);

    // 矩阵运算（4×4，column-major）
    static void perspective_matrix(double fov_deg, double aspect,
                                   double near_val, double far_val, float* m);
    static void look_at_matrix(double eye_x, double eye_y, double eye_z,
                               double center_x, double center_y, double center_z,
                               double up_x, double up_y, double up_z, float* m);
    static void multiply_matrix(const float* a, const float* b, float* out);
};

#endif
