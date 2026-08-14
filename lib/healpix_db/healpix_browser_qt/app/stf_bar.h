// stf_bar.h - 单条渐变 + 三控制点 STF 控件 (healpix_browser_qt app)
// 功能: 一条渐变条上三个可拖动控制点:
//   - 暗部截止 (shadows)   左
//   - 中间调   (midtones)  中
//   - 亮部截止 (highlights) 右
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
    QSize minimumSizeHint() const override;

private:
    int handle_at(const QPoint& pos) const;  // 0=shadows 1=midtones 2=highlights, -1
    float x_to_norm(int x) const;
    int norm_to_x(float v) const;
    void emit_params();

    float shadows_ = 0.0f;
    float midtones_ = 0.5f;
    float highlights_ = 1.0f;
    float dmin_ = 0.0f;
    float dmax_ = 1.0f;
    int drag_ = -1;
    static constexpr int kPad = 10;
};

#endif  // STF_BAR_H
