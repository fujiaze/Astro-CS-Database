// stf_panel.h - STF 控制面板 (healpix_browser_qt app)
// 功能: QDockWidget, 包含 STF 滑块 + 预设下拉 + 自动拉伸按钮
// 用途: 用户调整 STF 显示拉伸参数, 实时更新渲染
// 依赖: Qt6::Widgets (QDockWidget/QSlider/QComboBox/QPushButton/QLabel),
//       core/ (STFParams/STFEngine)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §4.2
// 滑块精度: QSlider 范围 0-1000 (int), shadows/highlights 映射到 [data_min, data_max] 原始像素值
//           midtones/compression 映射到 [0, 1]

#ifndef STF_PANEL_H
#define STF_PANEL_H

#include <QDockWidget>
#include "healpix_browser_core.h"

class QSlider;
class QPushButton;
class QComboBox;
class QLabel;

class STFPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit STFPanel(QWidget* parent = nullptr);

    // 从外部参数更新滑块 (不触发 stf_changed 信号)
    // 用途: view 内 STF 变化 (如 auto_stretch) 同步到面板
    void update_from_params(const STFParams& params);

    // 设置数据动态范围 (打开文件后由 MainWindow 同步)
    // 用途: 滑块 shadows/highlights 映射到 [data_min, data_max] 原始像素值
    void set_data_range(float data_min, float data_max);

signals:
    // 滑块/预设变化时发射 (MainWindow 转发给 view)
    void stf_changed(const STFParams& params);
    // 自动拉伸按钮点击
    void auto_stretch_requested();

private slots:
    void on_slider_changed();
    void on_preset_changed(int index);
    void on_auto_stretch_clicked();

private:
    void setup_ui();
    STFParams collect_params();
    void update_sliders_no_signal(const STFParams& params);

    // 滑块 (范围 0-1000)
    QSlider* shadows_slider_;
    QSlider* highlights_slider_;
    QSlider* midtones_slider_;
    QSlider* compression_slider_;

    // 预设下拉
    QComboBox* preset_combo_;

    // 自动拉伸按钮
    QPushButton* auto_button_;

    // 数值显示标签
    QLabel* shadows_label_;
    QLabel* highlights_label_;
    QLabel* midtones_label_;
    QLabel* compression_label_;

    // 数据动态范围 (滑块映射用)
    float data_min_ = 0.0f;
    float data_max_ = 1.0f;
};

#endif // STF_PANEL_H
