// stf_bar.cpp - 单条渐变 + 三控制点 STF 控件实现

#include "stf_bar.h"

#include <QMouseEvent>
#include <QPainter>
#include <QLinearGradient>
#include <algorithm>

STFBar::STFBar(QWidget* parent) : QWidget(parent) {
    setMinimumSize(220, 48);
    setMouseTracking(true);
}

void STFBar::set_range(float dmin, float dmax) {
    dmin_ = dmin;
    dmax_ = (dmax > dmin) ? dmax : (dmin + 1.0f);
    update();
}

void STFBar::set_params(const STFParams& params) {
    shadows_ = std::clamp(params.shadows, 0.0f, 1.0f);
    midtones_ = std::clamp(params.midtones, 0.0f, 1.0f);
    highlights_ = std::clamp(params.highlights, 0.0f, 1.0f);
    if (midtones_ < shadows_) midtones_ = shadows_;
    if (midtones_ > highlights_) midtones_ = highlights_;
    update();
}

STFParams STFBar::params() const {
    STFParams p;
    p.shadows = shadows_;
    p.highlights = highlights_;
    p.midtones = midtones_;
    p.compression = 0.0f;  // 由曲线预设决定（工具栏）
    return p;
}

float STFBar::x_to_norm(int x) const {
    const int w = std::max(1, width() - 2 * kPad);
    return std::clamp((float)(x - kPad) / (float)w, 0.0f, 1.0f);
}

int STFBar::norm_to_x(float v) const {
    const int w = std::max(1, width() - 2 * kPad);
    return kPad + (int)(std::clamp(v, 0.0f, 1.0f) * (float)w);
}

int STFBar::handle_at(const QPoint& pos) const {
    const float xs[3] = {shadows_, midtones_, highlights_};
    int best = -1;
    int best_d = 12;
    for (int i = 0; i < 3; ++i) {
        const int hx = norm_to_x(xs[i]);
        const int d = std::abs(pos.x() - hx);
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

void STFBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int bar_y = height() / 2 - 6;
    const int bar_h = 12;
    const int x0 = kPad;
    const int x1 = width() - kPad;
    const int bar_w = std::max(1, x1 - x0);

    // 渐变条（黑→白）
    QLinearGradient g(x0, 0, x1, 0);
    g.setColorAt(0.0, QColor(0, 0, 0));
    g.setColorAt(0.5, QColor(128, 128, 128));
    g.setColorAt(1.0, QColor(255, 255, 255));
    p.setPen(QPen(QColor(90, 90, 90), 1));
    p.setBrush(g);
    p.drawRect(x0, bar_y, bar_w, bar_h);

    // 三个控制点
    const float xs[3] = {shadows_, midtones_, highlights_};
    const char* names[3] = {"暗", "中", "亮"};
    const QColor colors[3] = {QColor(70, 130, 240), QColor(60, 200, 120),
                              QColor(240, 120, 70)};
    for (int i = 0; i < 3; ++i) {
        const int hx = norm_to_x(xs[i]);
        p.setPen(QPen(colors[i], 2));
        p.setBrush(colors[i]);
        p.drawLine(hx, bar_y - 6, hx, bar_y + bar_h + 6);
        p.setBrush(colors[i].lighter(130));
        p.drawEllipse(QPoint(hx, bar_y + bar_h / 2), 5, 5);
        p.drawText(QRect(hx - 20, bar_y + bar_h + 8, 40, 14),
                   Qt::AlignHCenter, names[i]);
    }

    // 底部数值
    p.setPen(QColor(200, 200, 200));
    p.drawText(QRect(x0, height() - 16, 90, 14), Qt::AlignLeft,
               QString("sh %1").arg(shadows_, 0, 'f', 3));
    p.drawText(QRect(width() / 2 - 45, height() - 16, 90, 14),
               Qt::AlignHCenter, QString("mid %1").arg(midtones_, 0, 'f', 3));
    p.drawText(QRect(x1 - 90, height() - 16, 90, 14), Qt::AlignRight,
               QString("hi %1").arg(highlights_, 0, 'f', 3));
}

void STFBar::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        drag_ = handle_at(e->pos());
        if (drag_ >= 0) {
            float& v = drag_ == 0 ? shadows_ : (drag_ == 1 ? midtones_ : highlights_);
            v = x_to_norm(e->pos().x());
            if (midtones_ < shadows_) midtones_ = shadows_;
            if (midtones_ > highlights_) midtones_ = highlights_;
            emit_params();
        }
    }
}

void STFBar::mouseMoveEvent(QMouseEvent* e) {
    if (drag_ >= 0) {
        float& v = drag_ == 0 ? shadows_ : (drag_ == 1 ? midtones_ : highlights_);
        v = x_to_norm(e->pos().x());
        if (midtones_ < shadows_) midtones_ = shadows_;
        if (midtones_ > highlights_) midtones_ = highlights_;
        emit_params();
    }
}

void STFBar::mouseReleaseEvent(QMouseEvent*) {
    drag_ = -1;
}

void STFBar::emit_params() {
    emit params_changed(params());
    update();
}

QSize STFBar::minimumSizeHint() const {
    return QSize(220, 52);
}
