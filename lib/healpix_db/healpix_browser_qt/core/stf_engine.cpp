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

    // 2. 计算中位数（nth_element, O(n)）
    //    注意：nth_element 会改变容器顺序，但 valid 是局部副本，安全
    size_t mid = valid.size() / 2;
    std::nth_element(valid.begin(), valid.begin() + mid, valid.end());
    float median = valid[mid];

    // 3. 计算 MAD = median(|v - median|)
    std::vector<float> abs_dev(valid.size());
    for (size_t i = 0; i < valid.size(); i++) {
        abs_dev[i] = std::fabs(valid[i] - median);
    }
    std::nth_element(abs_dev.begin(), abs_dev.begin() + abs_dev.size() / 2, abs_dev.end());
    float mad = abs_dev[abs_dev.size() / 2];

    // 4. sigma = 1.4826 * MAD（正态分布一致性常数）
    float sigma = 1.4826f * mad;
    if (sigma < 1e-10f) {
        // 所有值几乎相同 → sigma 趋零，设最小阈值避免除零
        LOG_WARN("auto_stretch: sigma 趋零 (mad=%g)，数据可能全相同，使用 1e-10 兜底", mad);
        sigma = 1e-10f;
    }

    // 5. shadows = median - 3*sigma, highlights = median + 3*sigma
    //    3-sigma 覆盖约 99.7% 正态分布区间
    STFParams p;
    p.shadows = median - 3.0f * sigma;
    p.highlights = median + 3.0f * sigma;

    // 6. midtones = 归一化 median 到 [0,1]
    //    即 MTF 中点对齐到中位数，使中位数映射到 0.5
    float range = p.highlights - p.shadows;
    if (range < 1e-30f) {
        // 极端边界：range 趋零，退化处理
        LOG_WARN("auto_stretch: range 趋零，使用 1.0 兜底");
        range = 1.0f;
    }
    p.midtones = (median - p.shadows) / range;
    // clamp 到 (0.01, 0.99) 避免极端 MTF 行为
    p.midtones = std::clamp(p.midtones, 0.01f, 0.99f);
    p.compression = 0.0f;

    LOG_INFO("auto_stretch: median=%g mad=%g sigma=%g shadows=%g highlights=%g midtones=%.4f",
             median, mad, sigma, p.shadows, p.highlights, p.midtones);

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
