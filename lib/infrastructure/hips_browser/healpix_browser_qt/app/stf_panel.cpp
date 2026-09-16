// stf_panel.cpp - STF 控制面板实现 (healpix_browser_qt app)
// 功能: 实现 QDockWidget, 包含单条渐变三控制点 (STFBar) + 自动拉伸按钮
// 用途: 用户调整 STF 显示拉伸参数, 实时更新渲染
// 依赖: Qt6::Widgets, core/ (STFEngine)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §4.2

#include "stf_panel.h"
#include "stf_bar.h"
#include "logger.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

STFPanel::STFPanel(QWidget* parent)
    : QDockWidget("STF 控制", parent), auto_button_(nullptr) {
    setup_ui();
}

void STFPanel::setup_ui() {
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);

    // ---- 单条渐变三控制点 ----
    stf_bar_ = new STFBar(this);
    layout->addWidget(stf_bar_);
    connect(stf_bar_, &STFBar::params_changed,
            this, &STFPanel::stf_changed);

    // ---- 自动拉伸按钮 ----
    auto_button_ = new QPushButton("自动拉伸 (MAD)", this);
    connect(auto_button_, &QPushButton::clicked,
            this, &STFPanel::on_auto_stretch_clicked);
    layout->addWidget(auto_button_);

    layout->addStretch();
    setWidget(container);
}

void STFPanel::on_auto_stretch_clicked() {
    emit auto_stretch_requested();
}

void STFPanel::sync_bar_no_signal(const STFParams& params) {
    // STFBar::set_params 不发信号（只重绘），无回环
    stf_bar_->set_params(params);
}

void STFPanel::update_from_params(const STFParams& params) {
    sync_bar_no_signal(params);
}

void STFPanel::set_data_range(float data_min, float data_max) {
    stf_bar_->set_range(data_min, data_max);
    LOG_INFO("STFPanel::set_data_range: [%.4g, %.4g]", data_min, data_max);
}
