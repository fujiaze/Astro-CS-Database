#!/usr/bin/env python3
"""真实 L4 观测面描述性统计（低 CPU）：亮区定位、覆盖子集边界边数、帧间散差。"""
import json, os, math
import numpy as np
_HERE = os.path.dirname(os.path.abspath(__file__))   # .../实验/<unit>/code/reverse_verify/smooth_lambda
ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", "..", "..", ".."))   # 仓库根（从脚本自身位置推导）
D = os.path.join(ROOT, "run/reverse_verify/smooth_lambda")
import importlib.util
spec = importlib.util.spec_from_file_location("an", os.path.join(_HERE, "analyze.py"))
an = importlib.util.module_from_spec(spec); spec.loader.exec_module(an)
U = an.read_upmb(os.path.join(D, "real49.upmb"))
F, K = U["n_frames"], U["n_nodes"]
o_f, o_k = U["o_f"], U["o_k"]
val = U["obs"]["value"]; unc = U["obs"]["unc"]
cover = np.zeros((F, K), bool); cover[o_f, o_k] = True
ncov = cover.sum(0)
# 单位权 raw stack（未校正的公共场近似）
s = np.zeros(K); c = np.zeros(K); np.add.at(s, o_k, val); np.add.at(c, o_k, 1.0)
raw = np.where(c > 0, s/np.maximum(c,1), np.nan)
multi = ncov >= 2
v = raw[multi]
q = np.nanpercentile(v, [50, 90, 99, 99.9])
res = dict(n_multi=int(multi.sum()), n_single=int((ncov==1).sum()), n_zero=int((ncov==0).sum()),
           raw_stack_pct={str(p): float(x) for p, x in zip([50,90,99,99.9], q)},
           raw_stack_max=float(np.nanmax(v)), raw_stack_min=float(np.nanmin(v)))
# 覆盖子集边界边
tiles = U["nodes"]["tile_ipix"]; gx = U["nodes"]["gx"]; gy = U["nodes"]["gy"]
key = np.array([(int(t) & 0xFFFFFF)*64 + int(b)*8 + int(a) for t,a,b in zip(tiles,gx,gy)], np.int64)
order = np.argsort(key, kind="stable"); ks = key[order]
same = (ks[1:] == ks[:-1] + 1); a = order[:-1][same]; b = order[1:][same]
diff = (cover[:, a] != cover[:, b]).any(0)
res["edges_total"] = int(len(a)); res["edges_subset_boundary"] = int(diff.sum())
res["edges_internal"] = int((~diff).sum())
# 亮区（raw stack 前 10%）坐标
bright = multi & (raw >= q[1]); peak = multi & (raw >= q[2])
res["n_bright"] = int(bright.sum()); res["n_peak"] = int(peak.sum())
ra = U["nodes"]["ra"]; dec = U["nodes"]["dec"]
if peak.sum():
    res["peak_centroid_radec"] = [float(np.nanmean(ra[peak])), float(np.nanmean(dec[peak]))]
    res["peak_radec_range"] = [[float(np.nanmin(ra[peak])), float(np.nanmax(ra[peak]))],
                               [float(np.nanmin(dec[peak])), float(np.nanmax(dec[peak]))]]
# 帧间散差（>=2 帧）
mean = np.where(c>0, s/np.maximum(c,1), 0.0)
v2 = np.zeros(K); np.add.at(v2, o_k, (val-mean[o_k])**2)
sc = np.sqrt(np.where(c>1, v2/np.maximum(c-1,1), np.nan))
res["frame_scatter_pct"] = {str(p): float(x) for p, x in zip([50,90,99], np.nanpercentile(sc[multi], [50,90,99]))}
res["frame_scatter_over_stack"] = float(np.nanmedian(sc[multi])/np.nanmedian(raw[multi]))
res["unc_over_stack"] = float(np.median(unc)/np.nanmedian(raw[multi]))
res["scatter_over_unc"] = float(np.nanmedian(sc[multi])/np.median(unc))
# 逐帧天光形状：raw stack 的逐帧偏差的低阶结构强度
res["frame_offset_rms_over_stack"] = float(np.nanstd([np.nanmean(val[o_f==f]) for f in range(F)])/np.nanmedian(raw[multi]))
json.dump(res, open(os.path.join(D, "real_descriptive.json"), "w"), indent=1)
print(json.dumps(res, indent=1))
