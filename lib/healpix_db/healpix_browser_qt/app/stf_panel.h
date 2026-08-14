// stf_panel.h - STF 控制面板 (healpix_browser_qt app)
// 功能: QDockWidget, 包含单条渐变三控制点 (STFBar) + 自动拉伸按钮
// 用途: 用户调整 STF 显示拉伸参数, 实时更新渲染
// 依赖: Qt6::Widgets (QDockWidget/QSlider/QComboBox/QPushButton/QLabel),
//       core/ (STFParams/STFEngine)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §4.2
// V14 v3: 三控制点（暗部截止/中间调/亮部截止），替代 4 滑块

#ifndef STF_PANEL_H
#define STF_PANEL_H

#include <QDockWidget>
#include "healpix_browser_core.h"

class QPushButton;
class STFBar;

class STFPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit STFPanel(QWidget* parent = nullptr);

    // 从外部参数更新控制点 (不触发 stf_changed 信号)
    // 用途: view 内 STF 变化 (如 auto_stretch) 同步到面板
    void update_from_params(const STFParams& params);

    // 设置数据动态范围 (首帧渲染后由 MainWindow 同步)
    // 用途: 控制点 shadows/highlights 映射到 [data_min, data_max] 原始像素值
    void set_data_range(float data_min, float data_max);

signals:
    // 控制点变化时发射 (MainWindow 转发给 view)
    void stf_changed(const STFParams& params);
    // 自动拉伸按钮点击
    void auto_stretch_requested();

private slots:
    void on_auto_stretch_clicked();

private:
    void setup_ui();
    void sync_bar_no_signal(const STFParams& params);

    // 单条渐变三控制点
    STFBar* stf_bar_ = nullptr;

    // 自动拉伸按钮
    QPushButton* auto_button_;
};

#endif // STF_PANEL_H
