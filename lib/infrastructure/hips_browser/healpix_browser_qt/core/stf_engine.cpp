// STFEngine 实现：显示拉伸引擎
// 模块：healpix_browser_qt / core
// 依赖：仅 STL，无 Qt
// 日志：通过 logger.h 输出关键步骤与异常

#include "stf_engine.h"
#include "logger.h"

#include <algorithm>
#include <cmath>
#include <cfloat>

// ---- STFParams 校验 ----
bool STFParams::validate() const {
    // shadows 必须严格小于 highlights，midtones 必须在开区间 (0,1)
    return shadows < highlights && midtones > 0.0f && midtones < 1.0f;
}

// ---- DisplayTransformState（唯一状态结构）----
void DisplayTransformState::normalize() {
    black = std::clamp(black, 0.0f, 0.999f);
    white = std::clamp(white, 0.001f, 1.0f);
    if (white <= black) white = std::min(1.0f, black + 0.05f);
    midtones = std::clamp(midtones, 0.001f, 0.999f);
    compression = std::clamp(compression, 0.0f, 1.0f);
}

STFParams DisplayTransformState::to_params() const {
    STFParams p;
    p.shadows = black;
    p.highlights = white;
    p.midtones = midtones;
    p.compression = compression;
    return p;
}

DisplayTransformState DisplayTransformState::from_params(const STFParams& p,
                                                         STFMode mode,
                                                         bool locked) {
    DisplayTransformState s;
    s.mode = mode;
    s.locked = locked;
    s.black = p.shadows;
    s.white = p.highlights;
    s.midtones = p.midtones;
    s.curve = "asinh";  // 曲线预设由外部设置（UI 不再提供预设下拉）
    s.compression = p.compression;
    s.normalize();
    return s;
}

// ---- STFEngine 构造（无状态，留空） ----
STFEngine::STFEngine() {}

// ---- 预设查询（接收数据范围，返回原始像素值） ----
// 与 Siril 显示传递函数对齐: 预设=全数据范围 + 曲线形状
STFParams STFEngine::get_preset(const std::string& name,
                                float data_min, float data_max) {
    STFParams p;
    // 所有预设: shadows=data_min, highlights=data_max（全数据范围可见）
    p.shadows = data_min;
    p.highlights = data_max;

    if (name == "linear") {
        // 线性: 无中点偏移、无压缩
        p.midtones = 0.5f;
        p.compression = 0.0f;
    } else if (name == "sqrt") {
        // 平方根风格: 中点下移提亮暗部，无压缩
        p.midtones = 0.25f;
        p.compression = 0.0f;
    } else if (name == "asinh") {
        // asinh 风格: 中点下移 + 中等压缩
        p.midtones = 0.25f;
        p.compression = 0.5f;
    } else if (name == "log") {
        // 对数风格: 中点更低 + 强压缩
        p.midtones = 0.15f;
        p.compression = 0.8f;
    } else {
        // 未知预设: 默认 linear 等效并告警
        p.midtones = 0.5f;
        p.compression = 0.0f;
        LOG_WARN("get_preset: 未知预设名 '%s'，返回默认 linear 参数", name.c_str());
    }
    return p;
}

// ---- 标量 MTF ----
float STFEngine::mtf(float x, float m) {
    // MTF(x, m) = ((m-1)*x) / ((2m-1)*x - m)
    // 满足：MTF(0,m)=0, MTF(1,m)=1, MTF(m,m)=0.5
    if (std::fabs(m - 0.5f) < 1e-10f) return x;  // m=0.5 线性
    float denom = (2.0f * m - 1.0f) * x - m;
    if (std::fabs(denom) < 1e-30f) denom = 1e-30f;  // 防除零
    float r = ((m - 1.0f) * x) / denom;
    // 结果裁剪到 [0,1]，避免数值漂移
    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    return r;
}

// ---- MAD 自动拉伸 ----
STFParams STFEngine::auto_stretch(const float* data, size_t n, float no_data_value) {
    // 边界：空指针或零长度
    if (data == nullptr || n == 0) {
        LOG_WARN("auto_stretch: 输入数据为空 (data=%p n=%zu)，返回默认参数", (void*)data, n);
        return STFParams();
    }

    // 1. 过滤 no_data 像素（v <= no_data_value 视为无效）
    std::vector<float> valid;
    valid.reserve(n);
    for (size_t i = 0; i < n; i++) {
        if (data[i] > no_data_value) valid.push_back(data[i]);
    }
    if (valid.empty()) {
        LOG_WARN("auto_stretch: 过滤 no_data 后无有效像素 (no_data_value=%g)", no_data_value);
        return STFParams();
    }

    LOG_INFO("auto_stretch: 输入 %zu 像素，有效 %zu 像素", n, valid.size());

    // 2. 排序后取百分位数 (0.5% / 99.5%) 作为 shadows/highlights
    // 只统计有数据的像素, 避免无数据区域(0值)拉偏统计
    // 用 0.5%/99.5% 而非 min/max, 避免饱和星等异常值
    std::sort(valid.begin(), valid.end());
    size_t n_valid = valid.size();
    size_t lo_idx = static_cast<size_t>(n_valid * 0.005);
    size_t hi_idx = static_cast<size_t>(n_valid * 0.995);
    if (hi_idx >= n_valid) hi_idx = n_valid - 1;
    float p_lo = valid[lo_idx];
    float p_hi = valid[hi_idx];

    // 3. 计算中位数 (已排序, 直接取)
    float median = valid[n_valid / 2];

    STFParams p;
    p.shadows = p_lo;
    p.highlights = p_hi;

    // 4. midtones = 归一化 median 到 [0,1]
    float range = p.highlights - p.shadows;
    if (range < 1e-30f) {
        LOG_WARN("auto_stretch: range 趋零，使用 1.0 兜底");
        range = 1.0f;
    }
    p.midtones = (median - p.shadows) / range;
    // clamp 到 (0.01, 0.99) 避免极端 MTF 行为
    p.midtones = std::clamp(p.midtones, 0.01f, 0.99f);
    // 默认使用 log 风格压缩 (强压缩, 提亮暗部, 适合天文图像)
    p.compression = 0.8f;

    LOG_INFO("auto_stretch: median=%g p_lo(0.5%%)=%g p_hi(99.5%%)=%g shadows=%g highlights=%g midtones=%.4f",
             median, p_lo, p_hi, p.shadows, p.highlights, p.midtones);

    return p;
}

// ---- 转 GPU uniform ----
// 设计: shadows/highlights 直接使用原始像素值传给 GPU uniform
// 着色器中 (vValue - uShadows) / (uHighlights - uShadows) 自动正确计算
// data_min/data_max 参数保留用于 STFPanel 滑块映射, 不参与 uniform 归一化
STFEngine::GPUUniforms STFEngine::to_uniforms(const STFParams& params,
                                              float data_min, float data_max,
                                              float no_data_value) {
    (void)data_min;  // 保留接口兼容, 不参与归一化
    (void)data_max;
    GPUUniforms u;
    u.shadows = params.shadows;       // 原始像素值
    u.highlights = params.highlights; // 原始像素值
    u.midtones = params.midtones;     // [0,1] MTF 中点
    u.compression = params.compression;
    u.no_data = no_data_value;

    LOG_DEBUG("to_uniforms: shadows=%g highlights=%g midtones=%.4f compression=%.4f no_data=%g",
              u.shadows, u.highlights, u.midtones, u.compression, u.no_data);

    return u;
}
