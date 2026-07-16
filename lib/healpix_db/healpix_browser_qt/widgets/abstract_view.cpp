// abstract_view.cpp - HEALPix 浏览器 Qt widget 抽象基类实现 (healpix_browser_qt)
// 功能: 实现 OpenGL 上下文初始化, 事件转发, 数据范围计算, STF 自动拉伸
// 用途: 为 SphereView 提供通用骨架 (SingleFrameView 已废弃归档)
// 依赖: Qt6::OpenGLWidgets, Qt6::Gui, core/ (BrowserBackend + GLRenderer + STFEngine)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §3.1, §6

#include "abstract_view.h"
#include "logger.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QSurfaceFormat>
#include <algorithm>
#include <cmath>
#include <limits>

// ============================================================================
// 构造 / 析构
// ============================================================================

AbstractView::AbstractView(QWidget* parent)
    : QOpenGLWidget(parent),
      backend_(nullptr),
      renderer_(nullptr),
      data_min_(0.0f),
      data_max_(1.0f),
      no_data_value_(0.0f),
      gl_initialized_(false),
      data_range_computed_(false) {
    // 请求 OpenGL 3.3 Core Profile
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);
    setFormat(fmt);
}

AbstractView::~AbstractView() {
    // Qt 保证 widget 销毁前 OpenGL 上下文仍有效 (在 ~QOpenGLWidget 之前)
    // 但需手动 makeCurrent 才能调用 OpenGL 命令释放资源
    if (gl_initialized_ && renderer_) {
        makeCurrent();
        renderer_->cleanup();
        renderer_.reset();
        doneCurrent();
    }
}

// ============================================================================
// 数据源 / STF 控制
// ============================================================================

void AbstractView::set_backend(BrowserBackend* backend) {
    backend_ = backend;
    // 切换数据源后需重新计算数据范围
    data_range_computed_ = false;
    if (gl_initialized_) {
        compute_data_range();
    }
    request_render();
}

void AbstractView::set_stf_params(const STFParams& params) {
    stf_params_ = params;
    // 仅更新 uniform, 不重建网格/纹理
    if (gl_initialized_ && renderer_) {
        makeCurrent();
        renderer_->update_stf(stf_params_, data_min_, data_max_);
        doneCurrent();
    }
    emit stf_changed(stf_params_);
    request_render();
}

void AbstractView::auto_stretch() {
    if (!backend_ || !backend_->is_open()) {
        return;
    }

    // 首次需要先计算数据范围 (data_min/data_max)
    if (!data_range_computed_) {
        compute_data_range();
    }

    // 取数据样本做 MAD 自动拉伸
    // .hiss: 全量数据; .hcsd: 取前若干子叶合并样本
    std::vector<float> sample;
    const size_t MAX_SAMPLE = 100000;  // 采样上限, 避免大数据集耗时

    if (backend_->is_hiss()) {
        LeafData all = backend_->get_all_data();
        if (all.n_pix == 0 || all.pixel == nullptr) {
            return;
        }
        size_t step = std::max<size_t>(1, all.n_pix / MAX_SAMPLE);
        for (size_t i = 0; i < all.n_pix; i += step) {
            float v = all.pixel[i];
            if (v > no_data_value_ && std::isfinite(v)) {
                sample.push_back(v);
            }
        }
        // 注意: get_all_data 返回的对象由 backend 持有, 不调用 release_leaf
    } else if (backend_->is_hcsd()) {
        // .hcsd: 加载前 4 个子叶作为采样
        // 注: .hcsd 全量加载成本高, 用子叶采样近似数据范围
        ViewParams dummy_view;
        dummy_view.center_ra = 0.0;
        dummy_view.center_dec = 0.0;
        dummy_view.zoom = 1.0;
        dummy_view.fov_deg = 180.0;
        auto leaves = backend_->get_required_leaves(dummy_view);
        size_t sample_leaves = std::min<size_t>(4, leaves.size());
        for (size_t i = 0; i < sample_leaves; ++i) {
            LeafData leaf = backend_->load_leaf(leaves[i], 256);
            if (leaf.n_pix == 0 || leaf.pixel == nullptr) {
                continue;
            }
            size_t step = std::max<size_t>(1, leaf.n_pix / (MAX_SAMPLE / 4));
            for (size_t j = 0; j < leaf.n_pix; j += step) {
                float v = leaf.pixel[j];
                if (v > no_data_value_ && std::isfinite(v)) {
                    sample.push_back(v);
                }
            }
            backend_->release_leaf(leaf);
        }
    }

    if (sample.empty()) {
        return;
    }

    STFParams params = STFEngine::auto_stretch(sample.data(), sample.size(),
                                                no_data_value_);
    set_stf_params(params);
}

// ============================================================================
// QOpenGLWidget 重载
// ============================================================================

void AbstractView::initializeGL() {
    initializeOpenGLFunctions();  // QOpenGLFunctions_3_3_Core

    renderer_ = std::make_unique<GLRenderer>();
    if (renderer_->init() != 0) {
        qCritical("AbstractView: GLRenderer 初始化失败");
        renderer_.reset();
        return;
    }

    gl_initialized_ = true;

    // 首次计算数据范围 (供 STF 归一化用)
    if (!data_range_computed_) {
        compute_data_range();
    }

    // 应用初始 STF 参数到 renderer
    renderer_->update_stf(stf_params_, data_min_, data_max_);
}

void AbstractView::resizeGL(int w, int h) {
    if (renderer_) {
        // 视口在 paintGL 内根据 RenderParams.viewport_w/h 设置
        // 这里只更新 Qt 端默认视口
        glViewport(0, 0, w, h);
    }
}

void AbstractView::paintGL() {
    if (!gl_initialized_ || !renderer_ || !backend_) {
        return;
    }

    RenderParams params = build_render_params();
    renderer_->render(*backend_, params);
}

// ============================================================================
// Qt 事件转发 (final, 强制子类实现 handle_*)
// ============================================================================

void AbstractView::mousePressEvent(QMouseEvent* event) {
    handle_mouse_press(event);
}

void AbstractView::mouseMoveEvent(QMouseEvent* event) {
    handle_mouse_move(event);
}

void AbstractView::mouseReleaseEvent(QMouseEvent* event) {
    handle_mouse_release(event);
}

void AbstractView::wheelEvent(QWheelEvent* event) {
    handle_wheel(event);
}

// ============================================================================
// 辅助方法
// ============================================================================

void AbstractView::request_render() {
    update();  // Qt 异步触发 paintGL
}

void AbstractView::compute_data_range() {
    if (!backend_ || !backend_->is_open() || data_range_computed_) {
        return;
    }

    // 收集所有有效像素值, 用于百分位数计算
    std::vector<float> valid;

    if (backend_->is_hiss()) {
        LeafData all = backend_->get_all_data();
        if (all.n_pix > 0 && all.pixel != nullptr) {
            // 大数据集采样 (避免遍历+排序 61.6M 像素卡死 UI)
            // 采样上限 100K 像素, 足够计算百分位
            const size_t MAX_SAMPLE = 100000;
            size_t step = std::max<size_t>(1, all.n_pix / MAX_SAMPLE);
            valid.reserve(std::min(MAX_SAMPLE, all.n_pix / step + 1));
            for (uint64_t i = 0; i < all.n_pix; i += step) {
                float v = all.pixel[i];
                if (v > no_data_value_ && std::isfinite(v)) {
                    valid.push_back(v);
                }
            }
        }
        // get_all_data 返回对象由 backend 持有, 不释放
    } else if (backend_->is_hcsd()) {
        // .hcsd: 采样前 4 个子叶求百分位
        ViewParams dummy_view;
        dummy_view.center_ra = 0.0;
        dummy_view.center_dec = 0.0;
        dummy_view.zoom = 1.0;
        dummy_view.fov_deg = 180.0;
        auto leaves = backend_->get_required_leaves(dummy_view);
        size_t sample_leaves = std::min<size_t>(4, leaves.size());
        for (size_t i = 0; i < sample_leaves; ++i) {
            LeafData leaf = backend_->load_leaf(leaves[i], 256);
            if (leaf.n_pix == 0 || leaf.pixel == nullptr) {
                continue;
            }
            for (uint64_t j = 0; j < leaf.n_pix; ++j) {
                float v = leaf.pixel[j];
                if (v > no_data_value_ && std::isfinite(v)) {
                    valid.push_back(v);
                }
            }
            backend_->release_leaf(leaf);
        }
    }

    if (valid.empty()) {
        // 兜底: 无有效数据时使用 [0, 1]
        data_min_ = 0.0f;
        data_max_ = 1.0f;
    } else {
        // 排序后取百分位数 (0.5% / 99.5%) 避免异常值 (如饱和星 5081)
        std::sort(valid.begin(), valid.end());
        size_t n = valid.size();
        size_t lo = static_cast<size_t>(n * 0.005);
        size_t hi = static_cast<size_t>(n * 0.995);
        if (hi >= n) hi = n - 1;
        data_min_ = valid[lo];
        data_max_ = valid[hi];

        LOG_INFO("compute_data_range: %zu 有效像素, 百分位 [0.5%%=%.4g, 99.5%%=%.4g]",
                 n, data_min_, data_max_);
    }

    data_range_computed_ = true;

    // 同步数据范围到 backend (供 ud_grade 归一化用)
    if (backend_) {
        backend_->set_data_range(data_min_, data_max_);
    }

    // 若 renderer 已初始化, 同步 uniform
    if (gl_initialized_ && renderer_) {
        renderer_->update_stf(stf_params_, data_min_, data_max_);
    }
}
