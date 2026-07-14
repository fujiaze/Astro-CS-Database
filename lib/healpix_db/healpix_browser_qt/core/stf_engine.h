#ifndef STF_ENGINE_H
#define STF_ENGINE_H

#include <cstdint>
#include <vector>
#include <string>

// STF 拉伸参数
// 说明：封装 Screen Transfer Function 所需的全部参数
//   - shadows/highlights: 像素值裁剪范围（原始像素值，GPU 着色器内归一化）
//   - midtones:           MTF 中点参数，0.5=线性，<0.5 提亮暗部
//   - compression:        asinh/log 等非线性预设的压缩强度 [0,1]
struct STFParams {
    float shadows;       // 暗部裁剪点 [0,1)
    float highlights;    // 亮部裁剪点 (0,1]
    float midtones;      // 中点参数 (0,1)，0.5=线性，<0.5 提亮暗部
    float compression;   // asinh/log 预设压缩强度 [0,1]

    STFParams()
        : shadows(0.0f), highlights(1.0f), midtones(0.5f), compression(0.0f) {}

    // 校验参数合法性：shadows<highlights 且 midtones 在 (0,1)
    bool validate() const;
};

// STFEngine：显示拉伸引擎
// 用途：将原始天文像素值映射到 [0,1] 显示区间，支持 MTF/预设/MAD 自动拉伸
// 特点：纯 C++ 实现，无 Qt 依赖，可独立单元测试
class STFEngine {
public:
    STFEngine();

    // 预设查询（接收数据范围，返回原始像素值，对齐 Siril 显示传递函数行为）
    // 预设=全数据范围 + 曲线形状:
    //   - shadows=data_min, highlights=data_max（全数据范围可见，不裁剪）
    //   - midtones/compression 由预设决定:
    //     linear: (0.5, 0.0)
    //     sqrt:   (0.25, 0.0)
    //     asinh:  (0.25, 0.5)
    //     log:    (0.15, 0.8)
    // 未知名称返回默认参数（linear 等效）并告警
    static STFParams get_preset(const std::string& name,
                                float data_min, float data_max);

    // 标量 MTF: MTF(x, m) = ((m-1)*x) / ((2m-1)*x - m)
    // 性质：MTF(0,m)=0, MTF(1,m)=1, MTF(m,m)=0.5
    // 边界：m=0.5 时退化为线性 y=x；结果 clamp 到 [0,1]
    static float mtf(float x, float m);

    // MAD 自动拉伸：基于中位数绝对偏差估算 shadows/highlights
    //   data: 像素值数组（原始 float，非归一化）
    //   no_data_value: 无效像素值，<= 该值的像素将被过滤
    // 返回 STFParams（shadows/highlights 为原始像素值范围，GPU 着色器内归一化）
    // 复杂度：O(n)（使用 std::nth_element 而非全排序）
    static STFParams auto_stretch(const float* data, size_t n,
                                  float no_data_value = 0.0f);

    // 将 STFParams 转换为 GPU uniform（归一化到 [0,1]）
    //   params:       STF 拉伸参数（shadows/highlights 为原始像素值）
    //   data_min/max: 像素值动态范围，用于归一化
    //   no_data_value:透传给着色器的无效像素标记
    struct GPUUniforms {
        float shadows;
        float highlights;
        float midtones;
        float compression;
        float no_data;
    };
    static GPUUniforms to_uniforms(const STFParams& params,
                                   float data_min, float data_max,
                                   float no_data_value = 0.0f);
};

#endif
