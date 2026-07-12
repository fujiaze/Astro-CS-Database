"""
Gradient Fitter - 2D多项式梯度曲面稳健拟合器
功能: 对星点比值/背景值拟合2D多项式曲面，用于鲁棒流量校准
用途: 乘性梯度(log10(F_syn/F_instr)比值)与加性梯度(PSF背景值B)的空间曲面估计

算法: IRLS + Tukey biweight 稳健回归 + 帽子矩阵LOOCV自动选阶
依据: docs/algorithm.md 第4-6节

核心流程:
  1. 坐标归一化到[-1,1]: x'=(2x/W)-1, y'=(2y/H)-1
  2. IRLS迭代: OLS初值 -> Tukey biweight权重 -> 加权最小二乘 -> 收敛
  3. LOOCV选阶: 帽子矩阵对角元素一次计算留出残差，选CV最小的阶数P*
  4. 防过拟合: n >= (P+1)(P+2)/2 * 3, P_max=5
"""

from __future__ import annotations

import os
import logging
from dataclasses import dataclass
from typing import Optional

import numpy as np


_TUKEY_C = 4.685
_MAD_SCALE = 0.6745
_MIN_ORDER = 1
_MAX_ORDER = 3
_MIN_SAMPLE_FACTOR = 3
_CV_REL_TOL = 0.10


@dataclass
class GradientSurface:
    order: int
    coeffs: np.ndarray
    loocv_error: float
    residual_median: float
    residual_std: float
    n_used: int
    n_rejected: int


class GradientFitter:
    """2D多项式梯度曲面稳健拟合器"""

    def __init__(self, log_dir: Optional[str] = None):
        self._logger = self._setup_logger(log_dir)

    @staticmethod
    def _setup_logger(log_dir: Optional[str]) -> logging.Logger:
        logger = logging.getLogger('gradient_fitter')
        logger.setLevel(logging.DEBUG)
        logger.propagate = False
        if logger.handlers:
            return logger
        sh = logging.StreamHandler()
        sh.setLevel(logging.INFO)
        sh.setFormatter(logging.Formatter('[%(levelname)s] %(message)s'))
        logger.addHandler(sh)
        if log_dir:
            os.makedirs(log_dir, exist_ok=True)
            fh = logging.FileHandler(
                os.path.join(log_dir, 'gradient_fitter.log'), encoding='utf-8')
            fh.setLevel(logging.DEBUG)
            fh.setFormatter(
                logging.Formatter('%(asctime)s [%(levelname)s] %(message)s'))
            logger.addHandler(fh)
        return logger

    # ------------------------------------------------------------------
    # 坐标归一化与单项式基
    # ------------------------------------------------------------------

    @staticmethod
    def _normalize_coords(x_arr: np.ndarray, y_arr: np.ndarray,
                          img_w: float, img_h: float):
        x_norm = (2.0 * x_arr / img_w) - 1.0
        y_norm = (2.0 * y_arr / img_h) - 1.0
        return x_norm, y_norm

    @staticmethod
    def _monomial_terms(order: int) -> list[tuple[int, int]]:
        terms = []
        for total in range(order + 1):
            for j in range(total + 1):
                k = total - j
                terms.append((j, k))
        return terms

    def _build_design_matrix(self, x_norm: np.ndarray,
                             y_norm: np.ndarray, order: int) -> np.ndarray:
        terms = self._monomial_terms(order)
        n = x_norm.shape[0]
        X = np.empty((n, len(terms)), dtype=np.float64)
        for idx, (j, k) in enumerate(terms):
            X[:, idx] = (x_norm ** j) * (y_norm ** k)
        return X

    # ------------------------------------------------------------------
    # IRLS + Tukey biweight 稳健拟合
    # ------------------------------------------------------------------

    def _irls_fit(self, X: np.ndarray, r: np.ndarray,
                  max_iter: int = 50, tol: float = 1e-8,
                  ridge_alpha: float = 0.0):
        """IRLS + Tukey biweight + Ridge L2 正则化

        Args:
            X: 设计矩阵 (n, p)
            r: 观测值 (n,)
            max_iter: 最大迭代次数
            tol: 收敛阈值
            ridge_alpha: Ridge 正则化强度 (0=OLS, 越大正则越强)
        """
        n, p = X.shape
        weights = np.ones(n, dtype=np.float64)

        # Ridge: (X^T W X + αI) β = X^T W r
        # 不正则化常数项 (第 0 列)，只正则化高阶项
        reg_diag = np.ones(p, dtype=np.float64)
        reg_diag[0] = 0.0  # 常数项不正则化

        XtX = X.T @ X
        Xtr = X.T @ r
        A = XtX + ridge_alpha * np.diag(reg_diag)
        try:
            coeffs = np.linalg.solve(A, Xtr)
        except np.linalg.LinAlgError:
            coeffs = np.linalg.lstsq(X, r, rcond=None)[0]

        for iteration in range(max_iter):
            residuals = r - X @ coeffs

            med = np.median(residuals)
            mad = np.median(np.abs(residuals - med))
            sigma = mad / _MAD_SCALE
            if sigma < 1e-12:
                sigma = 1e-12

            c = _TUKEY_C * sigma
            u = residuals / c
            abs_u = np.abs(u)
            new_weights = np.where(abs_u < 1.0, (1.0 - u * u) ** 2, 0.0)

            XtWX = (X.T * new_weights) @ X
            XtWr = X.T @ (new_weights * r)
            A = XtWX + ridge_alpha * np.diag(reg_diag)
            try:
                new_coeffs = np.linalg.solve(A, XtWr)
            except np.linalg.LinAlgError:
                new_coeffs = np.linalg.lstsq(A, XtWr, rcond=None)[0]

            coeff_change = float(np.max(np.abs(new_coeffs - coeffs)))
            coeffs = new_coeffs
            weights = new_weights

            n_used = int(np.sum(weights > 0.0))
            self._logger.debug(
                f"IRLS 迭代 {iteration}: 系数变化={coeff_change:.2e}, "
                f"使用点数={n_used}/{n}, sigma={sigma:.4e}, alpha={ridge_alpha:.2e}")

            if coeff_change < tol:
                self._logger.debug(f"IRLS 收敛于迭代 {iteration}")
                break

        residuals = r - X @ coeffs
        return coeffs, weights, residuals

    # ------------------------------------------------------------------
    # 帽子矩阵 LOOCV（一次拟合，不逐次重新拟合）
    # ------------------------------------------------------------------

    def _loocv_score(self, X: np.ndarray, r: np.ndarray,
                     weights: np.ndarray,
                     ridge_alpha: float = 0.0) -> float:
        """帽子矩阵 LOOCV (支持 Ridge 正则化)

        Ridge 模型: (X^T W X + αD) β = X^T W r
        帽子矩阵: H = W^{1/2} X (X^T W X + αD)^{-1} X^T W^{1/2}
        """
        n = X.shape[0]
        p = X.shape[1]
        W = weights

        reg_diag = np.ones(p, dtype=np.float64)
        reg_diag[0] = 0.0

        XtWX = (X.T * W) @ X
        XtWr = X.T @ (W * r)
        A = XtWX + ridge_alpha * np.diag(reg_diag)
        try:
            beta = np.linalg.solve(A, XtWr)
        except np.linalg.LinAlgError:
            beta = np.linalg.lstsq(A, XtWr, rcond=None)[0]

        r_hat = X @ beta
        residuals = r - r_hat

        try:
            A_inv = np.linalg.inv(A)
        except np.linalg.LinAlgError:
            A_inv = np.linalg.pinv(A)

        h_diag = W * np.sum(X * (X @ A_inv), axis=1)

        denom = 1.0 - h_diag
        valid = (np.abs(denom) > 1e-10) & (W > 0.0) & np.isfinite(residuals)
        if not np.any(valid):
            return float('inf')

        loo_residuals = np.where(valid, residuals / np.where(
            np.abs(denom) > 1e-10, denom, 1.0), 0.0)
        cv = float(np.mean(W[valid] * loo_residuals[valid] ** 2))
        return cv

    # ------------------------------------------------------------------
    # LOOCV 自动选阶
    # ------------------------------------------------------------------

    def _select_order(self, x_arr: np.ndarray, y_arr: np.ndarray,
                      r_arr: np.ndarray, img_w: float, img_h: float,
                      max_order: int):
        """LOOCV 自动选阶 + Ridge α 网格搜索

        对每个阶数 order, 搜索 Ridge α ∈ {0, 0.01, 0.1, 1, 10, 100},
        选 (order, α) 使 LOOCV 最小, 再用 10% 容差选最简模型。
        """
        n = r_arr.shape[0]
        x_norm, y_norm = self._normalize_coords(x_arr, y_arr, img_w, img_h)

        upper = min(max_order, _MAX_ORDER)
        ridge_alphas = [0.0, 0.1, 1.0, 10.0, 100.0, 1000.0]

        # results[(order, alpha)] = cv
        results: dict[tuple[int, float], float] = {}

        for order in range(_MIN_ORDER, upper + 1):
            n_coeffs = (order + 1) * (order + 2) // 2
            min_samples = n_coeffs * _MIN_SAMPLE_FACTOR
            if n < min_samples:
                self._logger.info(
                    f"阶数 {order}: 样本数 {n} < 最低要求 {min_samples}, 跳过")
                continue

            X = self._build_design_matrix(x_norm, y_norm, order)
            for alpha in ridge_alphas:
                coeffs, weights, residuals = self._irls_fit(
                    X, r_arr, ridge_alpha=alpha)
                cv = self._loocv_score(X, r_arr, weights, ridge_alpha=alpha)
                results[(order, alpha)] = cv
                n_used = int(np.sum(weights > 0.0))
                self._logger.info(
                    f"阶数 {order}, α={alpha:.2f}: LOOCV={cv:.6e}, "
                    f"使用点数={n_used}/{n}")

        if not results:
            self._logger.warning("无可用阶数, 退化到1阶")
            return _MIN_ORDER, float('inf'), {}, 0.0

        cv_min = min(results.values())

        # 选最简模型: 最低阶 + 最小 α, 在 cv_min 的 10% 容差内
        best_order = upper
        best_alpha = 0.0
        best_cv = float('inf')
        for order in sorted(set(k[0] for k in results.keys())):
            for alpha in ridge_alphas:
                key = (order, alpha)
                if key in results and results[key] <= cv_min * (1.0 + _CV_REL_TOL):
                    best_order = order
                    best_alpha = alpha
                    best_cv = results[key]
                    break
            if best_cv <= cv_min * (1.0 + _CV_REL_TOL):
                break

        self._logger.info(
            f"选阶结果: 最优阶数={best_order}, α={best_alpha:.2f}, "
            f"LOOCV={best_cv:.6e}, 全局最小CV={cv_min:.6e}, 容差={_CV_REL_TOL:.0%}")
        return best_order, best_cv, results, best_alpha

    # ------------------------------------------------------------------
    # 公共拟合接口
    # ------------------------------------------------------------------

    def _fit_surface(self, x_arr: np.ndarray, y_arr: np.ndarray,
                     val_arr: np.ndarray, img_w: float, img_h: float,
                     max_order: int, channel_name: str) -> GradientSurface:
        n = val_arr.shape[0]
        self._logger.info(
            f"{channel_name}梯度拟合: {n} 个采样点, 图像尺寸={img_w}x{img_h}")

        if n < 6:
            self._logger.warning(
                f"{channel_name}: 采样点数 {n} 过少, 返回1阶退化曲面")
            x_norm, y_norm = self._normalize_coords(x_arr, y_arr, img_w, img_h)
            X = self._build_design_matrix(x_norm, y_norm, 1)
            coeffs, weights, residuals = self._irls_fit(X, val_arr)
            n_used = int(np.sum(weights > 0.0))
            used = weights > 0.0
            return GradientSurface(
                order=1, coeffs=coeffs, loocv_error=float('inf'),
                residual_median=float(np.median(residuals[used])) if np.any(used) else 0.0,
                residual_std=float(np.std(residuals[used])) if np.any(used) else 0.0,
                n_used=n_used, n_rejected=n - n_used)

        best_order, best_cv, cv_curve, best_alpha = self._select_order(
            x_arr, y_arr, val_arr, img_w, img_h, max_order)

        x_norm, y_norm = self._normalize_coords(x_arr, y_arr, img_w, img_h)
        X = self._build_design_matrix(x_norm, y_norm, best_order)
        coeffs, weights, residuals = self._irls_fit(
            X, val_arr, ridge_alpha=best_alpha)

        n_used = int(np.sum(weights > 0.0))
        n_rejected = n - n_used
        used = weights > 0.0
        if np.any(used):
            resid_used = residuals[used]
            resid_median = float(np.median(resid_used))
            resid_std = float(np.std(resid_used))
        else:
            resid_median = 0.0
            resid_std = 0.0

        surface = GradientSurface(
            order=best_order, coeffs=coeffs, loocv_error=float(best_cv),
            residual_median=resid_median, residual_std=resid_std,
            n_used=n_used, n_rejected=n_rejected)

        self._logger.info(
            f"{channel_name}梯度拟合完成: 阶数={best_order}, "
            f"LOOCV={best_cv:.6e}, 使用={n_used}, 排除={n_rejected}, "
            f"残差中位数={resid_median:.4e}, 残差标准差={resid_std:.4e}")
        return surface

    def fit_multiplicative(self, x_arr, y_arr, r_arr,
                           img_w, img_h, max_order=5) -> GradientSurface:
        x_arr = np.asarray(x_arr, dtype=np.float64)
        y_arr = np.asarray(y_arr, dtype=np.float64)
        r_arr = np.asarray(r_arr, dtype=np.float64)
        return self._fit_surface(
            x_arr, y_arr, r_arr, img_w, img_h, max_order, "乘性")

    def fit_additive(self, x_arr, y_arr, b_arr,
                     img_w, img_h, max_order=5) -> GradientSurface:
        x_arr = np.asarray(x_arr, dtype=np.float64)
        y_arr = np.asarray(y_arr, dtype=np.float64)
        b_arr = np.asarray(b_arr, dtype=np.float64)
        return self._fit_surface(
            x_arr, y_arr, b_arr, img_w, img_h, max_order, "加性")

    # ------------------------------------------------------------------
    # 曲面评估
    # ------------------------------------------------------------------

    def evaluate_surface(self, surface: GradientSurface,
                         x_grid: np.ndarray, y_grid: np.ndarray) -> np.ndarray:
        x_grid = np.asarray(x_grid, dtype=np.float64)
        y_grid = np.asarray(y_grid, dtype=np.float64)
        orig_shape = x_grid.shape
        X = self._build_design_matrix(
            x_grid.ravel(), y_grid.ravel(), surface.order)
        result = X @ surface.coeffs
        return result.reshape(orig_shape)

    def evaluate_surface_fullimage(self, surface: GradientSurface,
                                   width: int, height: int) -> np.ndarray:
        xs = np.arange(width, dtype=np.float64)
        ys = np.arange(height, dtype=np.float64)
        x_grid, y_grid = np.meshgrid(xs, ys)
        x_norm = (2.0 * x_grid / width) - 1.0
        y_norm = (2.0 * y_grid / height) - 1.0
        return self.evaluate_surface(surface, x_norm, y_norm)


# ======================================================================
# 合成数据测试
# ======================================================================
if __name__ == '__main__':
    np.random.seed(42)

    n_points = 200
    img_w, img_h = 2000, 2000
    outlier_frac = 0.20

    x_px = np.random.uniform(0, img_w, n_points)
    y_px = np.random.uniform(0, img_h, n_points)
    x_norm = (2.0 * x_px / img_w) - 1.0
    y_norm = (2.0 * y_px / img_h) - 1.0

    true_terms = [(0, 0), (1, 0), (0, 1), (2, 0), (1, 1), (0, 2)]
    true_coeffs = np.array([0.1, 0.3, 0.2, 0.15, 0.0, 0.0])
    r_true = np.zeros(n_points)
    for idx, (j, k) in enumerate(true_terms):
        r_true += true_coeffs[idx] * (x_norm ** j) * (y_norm ** k)

    noise = np.random.normal(0, 0.01, n_points)
    r_arr = r_true + noise

    n_outliers = int(n_points * outlier_frac)
    outlier_idx = np.random.choice(n_points, n_outliers, replace=False)
    r_arr[outlier_idx] += np.random.uniform(-1.0, 1.0, n_outliers)

    log_dir = os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(
            os.path.abspath(__file__)))), 'logs', 'photometric_calib')
    fitter = GradientFitter(log_dir=log_dir)
    surface = fitter.fit_multiplicative(
        x_px, y_px, r_arr, img_w, img_h, max_order=5)

    print("\n" + "=" * 60)
    print("合成数据测试结果")
    print("=" * 60)
    print(f"数据: {n_points} 点, 离群点 {n_outliers} ({outlier_frac*100:.0f}%)")
    print(f"真实模型: r = 0.1 + 0.3*x' + 0.2*y' + 0.15*x'^2 (2阶)")
    print(f"拟合阶数: {surface.order}")
    print(f"LOOCV误差: {surface.loocv_error:.6e}")
    print(f"使用点数: {surface.n_used} / 排除点数: {surface.n_rejected}")
    print(f"残差中位数: {surface.residual_median:.6e}")
    print(f"残差标准差: {surface.residual_std:.6e}")

    fit_terms = GradientFitter._monomial_terms(surface.order)
    print(f"\n系数对比 (前6项=2阶真实系数):")
    print(f"  {'项':>8s}  {'真实值':>10s}  {'拟合值':>10s}  {'误差%':>8s}")
    max_err = 0.0
    for idx, (j, k) in enumerate(fit_terms[:6]):
        true_idx = true_terms.index((j, k))
        tv = true_coeffs[true_idx]
        fv = float(surface.coeffs[idx])
        denom = abs(tv) if abs(tv) > 1e-6 else 1.0
        err = abs(fv - tv) / denom * 100
        max_err = max(max_err, err)
        print(f"  ({j},{k})    {tv:10.4f}  {fv:10.4f}  {err:8.2f}")

    if surface.order > 2:
        print(f"\n高阶项系数 (应为~0):")
        for idx, (j, k) in enumerate(fit_terms[6:], start=6):
            print(f"  ({j},{k}): {float(surface.coeffs[idx]):.4e}")

    print("\n" + "-" * 60)
    order_ok = surface.order in (2, 3)
    coeff_ok = max_err < 5.0
    print(f"[选阶验证] P*={surface.order}, 期望2或3: "
          f"{'通过' if order_ok else '失败'}")
    print(f"[系数验证] 最大误差={max_err:.2f}%, 阈值5%: "
          f"{'通过' if coeff_ok else '失败'}")

    print("\n全图评估测试 (200x200):")
    full_map = fitter.evaluate_surface_fullimage(surface, 200, 200)
    print(f"  输出形状: {full_map.shape}, 范围: [{full_map.min():.4f}, {full_map.max():.4f}]")

    all_pass = order_ok and coeff_ok
    print("\n" + "=" * 60)
    print(f"测试结果: {'全部通过' if all_pass else '存在失败项'}")
    print("=" * 60)
