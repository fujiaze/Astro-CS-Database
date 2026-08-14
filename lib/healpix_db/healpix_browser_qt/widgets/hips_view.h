// ============================================================================
// hips_view.h - V9 HiPS 2D 天空视图 QWidget（signal/support + pan/zoom）
// ============================================================================

#ifndef HIPS_VIEW_H
#define HIPS_VIEW_H

#include <QImage>
#include <QPoint>
#include <QWidget>

#include <cstdint>
#include <vector>

#include "hips_sky_view.h"

class HipsBrowserBackend;

class HipsView : public QWidget {
    Q_OBJECT

public:
    explicit HipsView(QWidget* parent = nullptr);

    void set_backend(HipsBrowserBackend* backend);
    HipsSkyView* sky() { return &sky_; }

    // 预设/图层/拉伸控制（由 MainWindow 调用）
    void jump_to(double ra, double dec, double fov, int layer = -1);
    void set_layer(int layer);
    void set_stretch(const std::string& preset, bool auto_range);
    void set_manual_range(float lo, float hi);
    void set_manual_stf(const STFParams& params);  // V14 v3
    void refresh_auto_range();   // V14：Reset/Auto View 显式重算 robust STF
    void set_auto_view(bool on); // V14：Auto View（viewport 自适应）
    void set_stf_locked(bool locked); // V14：Lock STF（冻结标尺）
    void set_lod_mode(bool strict_leaf);
    void mark_dirty();

    // 截图：渲染当前视图并保存 PNG
    bool save_snapshot(const QString& path);

    // 返回当前光标对应天球坐标（不存在返回 false）
    bool cursor_sky(double* ra, double* dec) const;

signals:
    void viewChanged(double center_ra, double center_dec, double fov);
    void mouseMoved(double ra, double dec);
    void layerChanged(int layer);
    void rendered();  // V14 v3：首次渲染完成（STF 面板同步数据范围）

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    void ensure_rendered();
    void pixel_to_sky(int x, int y, double* ra, double* dec) const;

    HipsSkyView sky_;
    QImage img_;
    bool dirty_ = true;
    bool rendered_once_ = false;  // V14 v3
    bool dragging_ = false;
    QPoint last_pos_;
    double cursor_ra_ = 0.0, cursor_dec_ = 0.0;
    bool has_cursor_ = false;
    std::vector<std::uint32_t> rgba_;
};

#endif  // HIPS_VIEW_H
