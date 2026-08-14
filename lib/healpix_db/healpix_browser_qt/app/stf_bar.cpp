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
    midtones_ = std::clamp(params.midtones, kMidLo, kMidHi);
    highlights_ = std::clamp(params.highlights, 0.0f, 1.0f);
    enforce_window();
    update();
}

// 控制点最小窗口约束：防 shadows/highlights 反转导致全黑/全白
void STFBar::enforce_window() {
    if (highlights_ - shadows_ < kMinWindow) {
        const float mid = 0.5f * (shadows_ + highlights_);
        shadows_ = std::clamp(mid - 0.5f * kMinWindow, 0.0f, 1.0f - kMinWindow);
        highlights_ = shadows_ + kMinWindow;
    }
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

int STFBar::handle_at(const QPoint& pos) const {
    const float xs[3] = {shadows_, midtones_, highlights_};
    int best = -1;
    int best_d = 12;
    for (int i = 0; i < 3; ++i) {
        if (xs[i] < view_lo_ || xs[i] > view_hi_) continue;  // 窗口外不可见
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
    const int win_x0 = x0 + (int)((view_lo_ - 0.0f) * bar_w);
    const int win_x1 = x0 + (int)((view_hi_ - 0.0f) * bar_w);

    // 窗口外背景（深色，表示比例尺之外）
    p.fillRect(x0, bar_y, std::max(0, win_x0 - x0), bar_h,
               QColor(38, 40, 46));
    p.fillRect(std::min(x1, win_x1), bar_y, std::max(0, x1 - win_x1),
               bar_h, QColor(38, 40, 46));

    // 窗口内渐变条（黑→白，按全局亮度位置取窗口段）
    QLinearGradient g(x0, 0, x1, 0);
    g.setColorAt(0.0, QColor(0, 0, 0));
    g.setColorAt(0.5, QColor(128, 128, 128));
    g.setColorAt(1.0, QColor(255, 255, 255));
    p.setPen(QPen(QColor(90, 90, 90), 1));
    p.setBrush(g);
    p.drawRect(std::max(x0, win_x0), bar_y, std::max(1, std::min(x1, win_x1) - std::max(x0, win_x0)), bar_h);

    // 比例尺刻度（窗口两端标记 + 缩放比例）
    p.setPen(QColor(160, 170, 190));
    for (int i = 0; i <= 4; ++i) {
        const float pos = view_lo_ + (view_hi_ - view_lo_) * i / 4.0f;
        const int tx = x0 + (int)(pos * bar_w);
        p.drawLine(tx, bar_y - 10, tx, bar_y - 5);
    }
    p.drawText(QRect(x0, 0, bar_w, 12), Qt::AlignHCenter,
               QString("[%1, %2] x%3")
                   .arg(view_lo_, 0, 'f', 3)
                   .arg(view_hi_, 0, 'f', 3)
                   .arg(1.0f / (view_hi_ - view_lo_), 0, 'f', 1));

    // 三个控制点
    const float xs[3] = {shadows_, midtones_, highlights_};
    const char* names[3] = {"暗", "中", "亮"};
    const QColor colors[3] = {QColor(70, 130, 240), QColor(60, 200, 120),
                              QColor(240, 120, 70)};
    for (int i = 0; i < 3; ++i) {
        if (xs[i] < view_lo_ || xs[i] > view_hi_) continue;
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
            v = x_to_abs(e->pos().x());
            if (drag_ == 1) v = std::clamp(v, kMidLo, kMidHi);
            enforce_window();
            emit_params();
        } else {
            // 点击条上任一点：把最近的控制点吸附过来（V14 v3 交互改进）
            const float xs[3] = {shadows_, midtones_, highlights_};
            const float a = x_to_abs(e->pos().x());
            int best = 0;
            float bd = 1e9f;
            for (int i = 0; i < 3; ++i) {
                const float d = std::fabs(xs[i] - a);
                if (d < bd) {
                    bd = d;
                    best = i;
                }
            }
            drag_ = best;
            float& v = drag_ == 0 ? shadows_ : (drag_ == 1 ? midtones_ : highlights_);
            v = a;
            if (drag_ == 1) v = std::clamp(v, kMidLo, kMidHi);
            enforce_window();
            emit_params();
        }
    }
}

void STFBar::mouseMoveEvent(QMouseEvent* e) {
    if (drag_ >= 0) {
        float& v = drag_ == 0 ? shadows_ : (drag_ == 1 ? midtones_ : highlights_);
        v = x_to_abs(e->pos().x());
        if (drag_ == 1) v = std::clamp(v, kMidLo, kMidHi);
        enforce_window();
        emit_params();
    }
}

void STFBar::mouseReleaseEvent(QMouseEvent*) {
    drag_ = -1;
}

void STFBar::wheelEvent(QWheelEvent* e) {
    // 以鼠标位置为中心放大/缩小比例尺窗口
    const float anchor = x_to_abs(e->position().x());
    const float delta = e->angleDelta().y();
    const float factor = (delta > 0) ? 0.7f : 1.35f;
    const float w = std::clamp((view_hi_ - view_lo_) * factor, kMinWindow, 1.0f);
    view_lo_ = std::clamp(anchor - w * (anchor - view_lo_) / (view_hi_ - view_lo_),
                          0.0f, 1.0f - w);
    view_hi_ = view_lo_ + w;
    update();
    e->accept();
}

void STFBar::mouseDoubleClickEvent(QMouseEvent*) {
    view_lo_ = 0.0f;
    view_hi_ = 1.0f;
    update();
}

void STFBar::emit_params() {
    emit params_changed(params());
    update();
}

QSize STFBar::minimumSizeHint() const {
    return QSize(220, 52);
}
