#!/usr/bin/env python3
"""§14.1 stage2 hot-loop allocation-churn estimate (P2-01 quantification, static).
Documents the per-pixel/per-chunk/per-tile heap allocations in lib/phase2/tools/stage2.cpp
and estimates allocation count for a 32R-like mosaic. Static estimate (real 32R run is
Gaia-blocked); recorded as an estimate, not a measurement.
"""
import json, os

# source facts
facts = [
    "L1085: std::vector<uint32_t> src_idx(depth) constructed INSIDE the per-pixel loop (for i<cnt) -> 1 heap alloc per output pixel per chunk",
    "L782-783: std::vector<double> cal/supv(depth*chunk_pixels) per chunk",
    "L636: std::vector<uint32_t> frames per tile (probe reads)",
]

def alloc_estimate(n_out_pixels, depth, chunk_pixels, n_tiles, n_chunks_per_tile):
    per_pixel = n_out_pixels                  # src_idx per output pixel
    per_chunk = n_tiles * n_chunks_per_tile * 2   # cal+supv per chunk
    per_tile = n_tiles                        # frames per tile
    bytes_per_pixel_alloc = 4 * depth
    return {
        "n_out_pixels": n_out_pixels,
        "src_idx_allocs": per_pixel,
        "cal_supv_allocs": per_chunk,
        "frames_allocs": per_tile,
        "total_allocs": per_pixel + per_chunk + per_tile,
        "total_bytes_allocated": per_pixel * bytes_per_pixel_alloc + per_chunk * (depth*chunk_pixels*8*2) + per_tile * (depth*4),
        "note": "per-pixel src_idx dominates: 1 heap alloc per output pixel",
    }

# 32R-like: output mosaic of one 4500x3600 frame at nside=8192 (leaf 512) -> output pixels
cases = {
    "1M output px (partial mosaic)": alloc_estimate(1_000_000, 32, 65536, 64, 8),
    "16.2M output px (full frame)": alloc_estimate(16_200_000, 32, 65536, 256, 8),
    "small synthetic 1024x1024": alloc_estimate(1_048_576, 8, 65536, 16, 4),
}
for k, v in cases.items():
    print("== %s ==" % k)
    for kk, vv in v.items():
        print("   %-22s %s" % (kk, vv if not isinstance(vv, str) else vv))
    print()

result = {
    "verdict": ("stage2.cpp L1085 allocates std::vector<uint32_t> src_idx(depth) per OUTPUT PIXEL "
                "inside the innermost loop; for a 1M-pixel output that is ~1M heap allocations (plus "
                "cal/supv per chunk and frames per tile). For a full-frame 32R mosaic (~16M px) it is "
                "~16M allocations of 128 B each. This is P2-01; static estimate only - real runtime "
                "profiling requires the Gaia-blocked 32R run."),
    "source_facts": facts,
    "cases": cases,
}
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
p = os.path.join(ROOT, "package", "12_performance", "stage2_alloc_churn_estimate.json")
os.makedirs(os.path.dirname(p), exist_ok=True)
open(p, "w").write(json.dumps(result, indent=2, ensure_ascii=False))
print("written stage2_alloc_churn_estimate.json")
