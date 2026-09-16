// stf_bar.h - 单条渐变 + 三控制点 STF 控件 (healpix_browser_qt app)
// 功能: 一条渐变条上三个可拖动控制点:
// - 暗部截止 (shadows) 左
// - 中间调 (midtones) 中
// - 亮部截止 (highlights) 右
// shadows/highlights 为相对数据范围的归一化位置 [0,1];
// midtones 为 MTF 中点参数 [0,1]（0.5=线性）。

#ifndef STF_BAR_H
#define STF_BAR_H

#include <QWidget>

#include "stf_engine.h"

class STFBar : public QWidget {
    Q_OBJECT

public:
    explicit STFBar(QWidget* parent = nullptr);

    // 数据动态范围（像素值），控制点位置映射基准
    void set_range(float dmin, float dmax);
    // 同步参数（shadows/highlights 归一化 [0,1], midtones [0,1]）
    void set_params(const STFParams& params);
    STFParams params() const;

signals:
    void params_changed(const STFParams& params);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;   // 滚轮：放大/缩小比例尺
    void mouseDoubleClickEvent(QMouseEvent*) override;  // 双击：全览
    QSize minimumSizeHint() const override;

private:
    int handle_at(const QPoint& pos) const;  // 0=shadows 1=midtones 2=highlights, -1
    // 显示窗口（比例尺）：条全宽映射 [view_lo_, view_hi_]
    float view_rel_to_abs(float rel) const {
        return view_lo_ + rel * (view_hi_ - view_lo_);
    }
    float x_to_norm(int x) const {  // 条上像素 → 窗口内相对位置
        const int w = std::max(1, width() - 2 * kPad);
        return std::clamp((float)(x - kPad) / (float)w, 0.0f, 1.0f);
    }
    float x_to_abs(int x) const { return view_rel_to_abs(x_to_norm(x)); }
    int norm_to_x(float v) const {  // 绝对位置 → 条上像素（窗口外 clamp）
        const int w = std::max(1, width() - 2 * kPad);
        const float rel = (v - view_lo_) / (view_hi_ - view_lo_);
        return kPad + (int)(std::clamp(rel, 0.0f, 1.0f) * (float)w);
    }
    void enforce_window();         // 控制点最小窗口约束（防全黑/全白）
    void emit_params();

    float shadows_ = 0.0f;
    float midtones_ = 0.5f;
    float highlights_ = 1.0f;
    float view_lo_ = 0.0f;   // 比例尺窗口（绝对显示空间）
    float view_hi_ = 1.0f;
    float dmin_ = 0.0f;
    float dmax_ = 1.0f;
    int drag_ = -1;
    static constexpr int kPad = 10;
    static constexpr float kMinWindow = 0.05f;  // 控制点最小窗口
    static constexpr float kMidLo = 0.001f, kMidHi = 0.999f;
};

#endif  // STF_BAR_H
