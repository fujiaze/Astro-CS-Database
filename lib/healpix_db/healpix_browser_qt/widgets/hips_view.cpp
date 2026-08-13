// ============================================================================
// hips_view.cpp - V9 HiPS 2D 天空视图 QWidget 实现
// ============================================================================

#include "hips_view.h"

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "hips_browser_backend.h"

namespace {
constexpr double kPi = 3.14159265358979323846;
}

HipsView::HipsView(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setMinimumSize(320, 240);
}

void HipsView::set_backend(HipsBrowserBackend* backend) {
    sky_.set_backend(backend);
    dirty_ = true;
    update();
}

void HipsView::jump_to(double ra, double dec, double fov, int layer) {
    sky_.set_view(ra, dec, fov, (double)width() / std::max(1, height()));
    if (layer >= 0) sky_.set_layer(layer);
    dirty_ = true;
    update();
    emit viewChanged(sky_.center_ra(), sky_.center_dec(), sky_.fov());
    emit layerChanged(sky_.layer());
}

void HipsView::set_layer(int layer) {
    sky_.set_layer(layer);
    dirty_ = true;
    update();
    emit layerChanged(layer);
}

void HipsView::set_stretch(const std::string& preset, bool auto_range) {
    sky_.set_stretch(preset, auto_range);
    dirty_ = true;
    update();
}

void HipsView::set_manual_range(float lo, float hi) {
    sky_.set_manual_range(lo, hi);
    dirty_ = true;
    update();
}

void HipsView::refresh_auto_range() {
    sky_.refresh_auto_range();
    dirty_ = true;
    update();
}

void HipsView::set_auto_view(bool on) {
    sky_.set_auto_view(on);
    dirty_ = true;
    update();
}

void HipsView::set_lod_mode(bool strict_leaf) {
    sky_.set_lod_mode(strict_leaf);
    dirty_ = true;
    update();
}

void HipsView::mark_dirty() {
    dirty_ = true;
    update();
}

void HipsView::ensure_rendered() {
    if (!dirty_ && img_.size() == size()) return;
    const int w = std::max(64, width());
    const int h = std::max(64, height());
    sky_.set_size(w, h);
    sky_.rasterize(rgba_);
    img_ = QImage((const uchar*)rgba_.data(), w, h, (int)(w * 4),
                  QImage::Format_ARGB32)
               .copy();
    dirty_ = false;
}

void HipsView::paintEvent(QPaintEvent*) {
    ensure_rendered();
    QPainter p(this);
    p.drawImage(0, 0, img_);
}

void HipsView::resizeEvent(QResizeEvent*) {
    dirty_ = true;
    sky_.set_size(width(), height());
}

void HipsView::pixel_to_sky(int x, int y, double* ra, double* dec) const {
    const double w = std::max(1, width());
    const double h = std::max(1, height());
    const double u = 2.0 * (x + 0.5) / w - 1.0;
    const double v = 1.0 - 2.0 * (y + 0.5) / h;
    const double tan_half = std::tan(sky_.fov() * kPi / 360.0);
    const double xi = -u * tan_half * (w / h);
    const double eta = v * tan_half;
    const double r0 = sky_.center_ra() * kPi / 180.0;
    const double d0 = sky_.center_dec() * kPi / 180.0;
    const double rr = std::hypot(xi, eta);
    double ra_r, dec_r;
    if (rr < 1e-12) {
        dec_r = d0;
        ra_r = r0;
    } else {
        const double c = std::atan(rr);
        const double sin_c = std::sin(c), cos_c = std::cos(c);
        dec_r = std::asin(cos_c * std::sin(d0) +
                          eta * sin_c * std::cos(d0) / rr);
        ra_r = r0 + std::atan2(xi * sin_c,
                               rr * std::cos(d0) * cos_c -
                                   eta * std::sin(d0) * sin_c);
    }
    ra_r = std::fmod(ra_r, 2.0 * kPi);
    if (ra_r < 0.0) ra_r += 2.0 * kPi;
    *ra = ra_r * 180.0 / kPi;
    *dec = dec_r * 180.0 / kPi;
}

bool HipsView::cursor_sky(double* ra, double* dec) const {
    if (!has_cursor_) return false;
    *ra = cursor_ra_;
    *dec = cursor_dec_;
    return true;
}

void HipsView::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        dragging_ = true;
        last_pos_ = e->pos();
    }
}

void HipsView::mouseMoveEvent(QMouseEvent* e) {
    double ra = 0, dec = 0;
    pixel_to_sky(e->pos().x(), e->pos().y(), &ra, &dec);
    cursor_ra_ = ra;
    cursor_dec_ = dec;
    has_cursor_ = true;
    emit mouseMoved(ra, dec);
    if (dragging_) {
        const int dx = e->pos().x() - last_pos_.x();
        const int dy = e->pos().y() - last_pos_.y();
        last_pos_ = e->pos();
        const double scale = sky_.fov() / std::max(1, height());
        // 抓画面拖：右拖看东（RA 增），下拖看北（Dec 增）
        sky_.set_view(sky_.center_ra() + dx * scale,
                      sky_.center_dec() + dy * scale, sky_.fov(),
                      (double)width() / std::max(1, height()));
        dirty_ = true;
        update();
        emit viewChanged(sky_.center_ra(), sky_.center_dec(), sky_.fov());
    }
}

void HipsView::mouseReleaseEvent(QMouseEvent*) { dragging_ = false; }

void HipsView::wheelEvent(QWheelEvent* e) {
    const double factor =
        (e->angleDelta().y() > 0) ? 0.85 : 1.0 / 0.85;
    sky_.set_view(sky_.center_ra(), sky_.center_dec(),
                  sky_.fov() * factor,
                  (double)width() / std::max(1, height()));
    dirty_ = true;
    update();
    emit viewChanged(sky_.center_ra(), sky_.center_dec(), sky_.fov());
}

bool HipsView::save_snapshot(const QString& path) {
    ensure_rendered();
    if (img_.isNull()) return false;
    return img_.save(path);
}
