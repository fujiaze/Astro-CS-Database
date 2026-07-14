// stf_panel.cpp - STF 控制面板实现 (healpix_browser_qt app)
// 功能: 实现 QDockWidget, 包含 4 个滑块 + 预设下拉 + 自动拉伸按钮
// 用途: 用户调整 STF 显示拉伸参数, 实时更新渲染
// 依赖: Qt6::Widgets, core/ (STFEngine)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §4.2
// 滑块精度: QSlider 范围 0-1000 (int), 映射到 [0, 1] float, 精度 0.001

#include "stf_panel.h"
#include "logger.h"

#include <QSlider>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QWidget>
#include <QString>
#include <algorithm>  // std::clamp

// ============================================================================
// 构造
// ============================================================================

STFPanel::STFPanel(QWidget* parent)
    : QDockWidget("STF 控制", parent),
      shadows_slider_(nullptr),
      highlights_slider_(nullptr),
      midtones_slider_(nullptr),
      compression_slider_(nullptr),
      preset_combo_(nullptr),
      auto_button_(nullptr),
      shadows_label_(nullptr),
      highlights_label_(nullptr),
      midtones_label_(nullptr),
      compression_label_(nullptr) {
    setup_ui();
}

// ============================================================================
// UI 设置
// ============================================================================

void STFPanel::setup_ui() {
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);

    // ---- 预设下拉 ----
    auto* preset_layout = new QHBoxLayout();
    preset_layout->addWidget(new QLabel("预设:", this));
    preset_combo_ = new QComboBox(this);
    preset_combo_->addItem("linear", "linear");
    preset_combo_->addItem("sqrt", "sqrt");
    preset_combo_->addItem("asinh", "asinh");
    preset_combo_->addItem("log", "log");
    connect(preset_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &STFPanel::on_preset_changed);
    preset_layout->addWidget(preset_combo_);
    preset_layout->addStretch();
    layout->addLayout(preset_layout);

    // ---- 滑块网格 ----
    auto* grid = new QGridLayout();

    // Shadows
    shadows_slider_ = new QSlider(Qt::Horizontal, this);
    shadows_slider_->setRange(0, 999);  // [0, 1)
    shadows_slider_->setValue(0);
    shadows_label_ = new QLabel("0.000", this);
    shadows_label_->setMinimumWidth(50);
    grid->addWidget(new QLabel("Shadows:", this), 0, 0);
    grid->addWidget(shadows_slider_, 0, 1);
    grid->addWidget(shadows_label_, 0, 2);
    connect(shadows_slider_, &QSlider::valueChanged,
            this, &STFPanel::on_slider_changed);

    // Highlights
    highlights_slider_ = new QSlider(Qt::Horizontal, this);
    highlights_slider_->setRange(1, 1000);  // (0, 1]
    highlights_slider_->setValue(1000);
    highlights_label_ = new QLabel("1.000", this);
    highlights_label_->setMinimumWidth(50);
    grid->addWidget(new QLabel("Highlights:", this), 1, 0);
    grid->addWidget(highlights_slider_, 1, 1);
    grid->addWidget(highlights_label_, 1, 2);
    connect(highlights_slider_, &QSlider::valueChanged,
            this, &STFPanel::on_slider_changed);

    // Midtones
    midtones_slider_ = new QSlider(Qt::Horizontal, this);
    midtones_slider_->setRange(1, 999);  // (0, 1)
    midtones_slider_->setValue(500);     // 0.5 = 线性
    midtones_label_ = new QLabel("0.500", this);
    midtones_label_->setMinimumWidth(50);
    grid->addWidget(new QLabel("Midtones:", this), 2, 0);
    grid->addWidget(midtones_slider_, 2, 1);
    grid->addWidget(midtones_label_, 2, 2);
    connect(midtones_slider_, &QSlider::valueChanged,
            this, &STFPanel::on_slider_changed);

    // Compression
    compression_slider_ = new QSlider(Qt::Horizontal, this);
    compression_slider_->setRange(0, 1000);  // [0, 1]
    compression_slider_->setValue(0);
    compression_label_ = new QLabel("0.000", this);
    compression_label_->setMinimumWidth(50);
    grid->addWidget(new QLabel("Compression:", this), 3, 0);
    grid->addWidget(compression_slider_, 3, 1);
    grid->addWidget(compression_label_, 3, 2);
    connect(compression_slider_, &QSlider::valueChanged,
            this, &STFPanel::on_slider_changed);

    layout->addLayout(grid);

    // ---- 自动拉伸按钮 ----
    auto_button_ = new QPushButton("自动拉伸 (MAD)", this);
    connect(auto_button_, &QPushButton::clicked,
            this, &STFPanel::on_auto_stretch_clicked);
    layout->addWidget(auto_button_);

    layout->addStretch();
    setWidget(container);
}

// ============================================================================
// 滑块 → STFParams
// shadows/highlights 滑块 [0,1] 映射到 [data_min, data_max] 原始像素值
// midtones/compression 滑块 [0,1] 保持不变
// ============================================================================

STFParams STFPanel::collect_params() {
    STFParams p;
    float range = data_max_ - data_min_;
    if (range < 1e-30f) range = 1.0f;
    // shadows/highlights: 滑块 [0,1] → 原始像素值 [data_min, data_max]
    p.shadows = data_min_ + static_cast<float>(shadows_slider_->value()) / 1000.0f * range;
    p.highlights = data_min_ + static_cast<float>(highlights_slider_->value()) / 1000.0f * range;
    // midtones/compression: 保持 [0,1]
    p.midtones = static_cast<float>(midtones_slider_->value()) / 1000.0f;
    p.compression = static_cast<float>(compression_slider_->value()) / 1000.0f;
    return p;
}

// ============================================================================
// 槽函数
// ============================================================================

void STFPanel::on_slider_changed() {
    STFParams p = collect_params();
    // 更新数值显示: shadows/highlights 显示原始像素值, midtones/compression 显示 [0,1]
    shadows_label_->setText(QString::number(p.shadows, 'f', 2));
    highlights_label_->setText(QString::number(p.highlights, 'f', 2));
    midtones_label_->setText(QString::number(p.midtones, 'f', 3));
    compression_label_->setText(QString::number(p.compression, 'f', 3));
    emit stf_changed(p);
}

void STFPanel::on_preset_changed(int index) {
    if (index < 0) return;
    QString name = preset_combo_->itemData(index).toString();
    STFParams p = STFEngine::get_preset(name.toStdString(),
                                        data_min_, data_max_);
    update_sliders_no_signal(p);
    // 触发一次完整 stf_changed
    emit stf_changed(p);
}

void STFPanel::on_auto_stretch_clicked() {
    emit auto_stretch_requested();
}

// ============================================================================
// 外部 → 面板同步
// ============================================================================

void STFPanel::update_sliders_no_signal(const STFParams& params) {
    // blockSignals 避免触发 valueChanged → on_slider_changed → stf_changed 回环
    shadows_slider_->blockSignals(true);
    highlights_slider_->blockSignals(true);
    midtones_slider_->blockSignals(true);
    compression_slider_->blockSignals(true);

    // shadows/highlights: 原始像素值 → 滑块 [0,1000]
    float range = data_max_ - data_min_;
    if (range < 1e-30f) range = 1.0f;
    int sh_val = static_cast<int>((params.shadows - data_min_) / range * 1000.0f);
    int hi_val = static_cast<int>((params.highlights - data_min_) / range * 1000.0f);
    shadows_slider_->setValue(std::clamp(sh_val, 0, 999));
    highlights_slider_->setValue(std::clamp(hi_val, 1, 1000));
    // midtones/compression: [0,1] → 滑块 [0,1000]
    midtones_slider_->setValue(static_cast<int>(params.midtones * 1000));
    compression_slider_->setValue(static_cast<int>(params.compression * 1000));

    shadows_slider_->blockSignals(false);
    highlights_slider_->blockSignals(false);
    midtones_slider_->blockSignals(false);
    compression_slider_->blockSignals(false);

    // 更新数值显示: shadows/highlights 显示原始像素值, midtones/compression 显示 [0,1]
    shadows_label_->setText(QString::number(params.shadows, 'f', 2));
    highlights_label_->setText(QString::number(params.highlights, 'f', 2));
    midtones_label_->setText(QString::number(params.midtones, 'f', 3));
    compression_label_->setText(QString::number(params.compression, 'f', 3));
}

void STFPanel::update_from_params(const STFParams& params) {
    update_sliders_no_signal(params);
}

void STFPanel::set_data_range(float data_min, float data_max) {
    data_min_ = data_min;
    data_max_ = data_max;
    LOG_INFO("STFPanel::set_data_range: [%.4g, %.4g]", data_min_, data_max_);
}
