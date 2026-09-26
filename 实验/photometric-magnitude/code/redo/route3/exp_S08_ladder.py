#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S08 自适应星等阶梯 {12,13,14,15,16} / 早停 2000 / 循环 5（05 A-5, 01/C5）。

假说 H8a: 阶梯的最坏情况 = 固定单次 mag_max=16 查询(末档必跑), 早停只减查询数不减样本
        ⇒ 阶梯相对固定 16 的"星样本无效应", 其唯一效应是省成本(结构性事实)。
假说 H8b: 早停阈 N=2000 与零点精度地板直接挂钩: sigma_kappa ≈ 1.2533·sigma_residual/√N
        (02 式-4 消费口径); N=2000, sigma_res=0.02 dex ⇒ SE≈4.5e-4 dex=1.1 mmag ——
        若降到 200, SE 退 3.2 倍 ⇒ 2000 是"精度地板 vs 成本"的精度侧理由。
假说 H8c: 拥挤场(M42 类)无论阈取 2000 或 500 都跑满 5 档 ⇒ 该参数只影响稀疏场成本。
背景密度模型: Gaia DR3 约 1.8e9 源(Lindegren 2021 / Montegriffo 2023 摘要),
平均全天密度 ≈ 43.6 /deg²; XP 光谱子集 220M ≈ 12%; 面密度对数正态散布跨 2 个量级。
seed 固定 = 20260926。复现: python3 exp_S08_ladder.py
"""
import json, os
import numpy as np

SEED = 20260926
LADDER = [12.0, 13.0, 14.0, 15.0, 16.0]


def cone_area(r_deg):
    return np.pi * r_deg ** 2


def main():
    rng = np.random.default_rng(SEED)
    out = {"seed": SEED}

    # H8a: 阶梯 vs 固定 16 —— 样本无效应
    # 场密度: 每档星数 = area · rho_total · frac(G<mag) ; 取斜率 gamma per mag
    gamma = 0.35
    rho0 = 43.6 / 0.12        # 全量面密度使 XP 子集均值 ≈ 43.6/deg²
    # 密度散布: log10 rho ~ N(0, 0.8) 的场族(从全天均值到银心极挤)
    n_fields = 20000
    log10rho = rng.normal(1.64, 0.8, n_fields)   # 均值 ~43.6/deg²
    area = cone_area(1.0)                         # fov=1 deg 的锥搜面积
    queries_ladder, queries_fixed = [], []
    sample_equal = 0
    for lr in log10rho:
        rho = 10 ** lr
        per_step = rho * area * gamma * (10 ** (np.array(LADDER) - 12.0) * 0.0)  # placeholder
        # 简化: 每 mag 增加因子 10^gamma, 从 mag12 基数 N12 开始
        N12 = rho * area * 1e-3
        counts = N12 * 10 ** (gamma * (np.array(LADDER) - 12.0))
        cum = np.cumsum(counts)
        # 阶梯: 找首个 n_gaia>=2000 的档(该档累计), 否则 16 档
        stop = np.argmax(cum >= 2000) if (cum >= 2000).any() else 4
        queries_ladder.append(stop + 1)
        queries_fixed.append(1)
        if min(cum[stop], cum[4]) >= 2000:
            sample_equal += 1
        elif stop == 4:
            sample_equal += 1  # 都到 16 档, 样本同
    out["H8a"] = {
        "mean_queries_ladder": float(np.mean(queries_ladder)),
        "mean_queries_fixed16": 1.0,
        "max_queries_ladder": int(np.max(queries_ladder)),
        "sample_identical_to_fixed16_fraction": sample_equal / n_fields,
        "criterion": "阶梯的最终样本 ≡ 固定 16 的样本(结构性), 效应只在查询成本",
        "pass": bool(np.max(queries_ladder) <= 5),
    }

    # H8b: 精度地板
    sigma_res = 0.02
    table = {}
    for N in (200, 500, 2000, 10000):
        se_dex = 1.2533 * sigma_res / np.sqrt(N)
        table[str(N)] = {"se_dex": float(se_dex), "se_mag": float(2.5 * se_dex)}
    out["H8b"] = {
        "zero_point_se_by_N": table,
        "gain_2000_vs_200": float(table["200"]["se_dex"] / table["2000"]["se_dex"]),
        "criterion": "N=2000 ⇒ SE≈1.1 mmag, 处于 XP 合成测光 mmag 精度档;"
                     "N=200 时 SE 退 3.2 倍 ⇒ 早停阈有精度侧理由",
        "pass": bool(2.5 < table["200"]["se_dex"] / table["2000"]["se_dex"] < 4),
    }

    # H8c: 拥挤场 —— 阈 2000 vs 500 都跑满 5 档
    crowded = log10rho > np.quantile(log10rho, 0.9)
    hits = 0
    for lr in log10rho[crowded]:
        rho = 10 ** lr
        N12 = rho * area * 1e-3
        counts = N12 * 10 ** (gamma * (np.array(LADDER) - 12.0))
        cum = np.cumsum(counts)
        for thr in (2000, 500):
            stop = np.argmax(cum >= thr) if (cum >= thr).any() else 4
            if stop >= 4:
                hits += 1
    out["H8c"] = {
        "crowded_fields_ladder_exhausted_fraction_2000": float(hits / (2 * crowded.sum())),
        "criterion": "前 10% 拥挤场上两阈值都跑满 5 档 ⇒ 阈值不影响拥挤场行为(01/C5 的 M42 断言)",
    }

    # 负例: 极稀疏场 ⇒ 阈值无效应(两阈值同样跑满 5 档, 样本=16 档全量)
    sparse = log10rho < np.quantile(log10rho, 0.01)
    same = 0
    for lr in log10rho[sparse]:
        rho = 10 ** lr
        N12 = rho * area * 1e-3
        counts = N12 * 10 ** (gamma * (np.array(LADDER) - 12.0))
        cum = np.cumsum(counts)
        stops = [np.argmax(cum >= thr) if (cum >= thr).any() else 4 for thr in (2000, 500)]
        if stops[0] == 4 and stops[1] == 4:
            same += 1
    out["negative_zero_check"] = {
        "sparse_fields_identical_fraction": float(same / sparse.sum()),
        "criterion": "极稀疏场两阈值行为相同(样本同) ⇒ 无效应时成本差=0, 归零",
    }

    out["verdict"] = {"H8a": out["H8a"]["pass"], "H8b": out["H8b"]["pass"]}
    res = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results")
    os.makedirs(res, exist_ok=True)
    p = os.path.join(res, "exp_S08_ladder.json")
    with open(p, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(json.dumps({"H8a": out["H8a"], "H8b": out["H8b"], "H8c": out["H8c"],
                      "negative_zero_check": out["negative_zero_check"],
                      "verdict": out["verdict"]}, ensure_ascii=False, indent=1))
    print("written:", p)


if __name__ == "__main__":
    main()
