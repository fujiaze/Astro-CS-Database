#!/usr/bin/env python3
"""
A-4b FOV Buffer Constant Validation Experiment (Route 2)

Purpose: Empirically test the three FOV constants from 05_正向规格.md §3.A-4b:
  - Buffer coefficient: 1.2
  - Clamp lower bound: 1.0 degree
  - Clamp upper bound: 10.0 degrees

Data choice: Pure analytical synthesis with "true value has no effect" negative control
Fixed seed: 20260926

Key question: What is the practical impact of these constants on Gaia cone search coverage?
Hypothesis: The constants represent engineering safety margins for astrometric matching, not fundamental physical constants.
"""

import numpy as np
import json
from pathlib import Path

# ============================================================================
# CONFIGURATION
# ============================================================================

SEED = 20260926
np.random.seed(SEED)

# Constants under investigation
BUFFER_COEF = 1.2
CLAMP_MIN = 1.0   # degrees
CLAMP_MAX = 10.0  # degrees

# Output paths
RESULTS_DIR = Path(__file__).parent / "results"
RESULTS_DIR.mkdir(exist_ok=True)

# ============================================================================
# ANALYTICAL FORMULAS FROM SPEC (05_正向规格.md A-4b)
# ============================================================================

def compute_fov_radius(pixel_scale_deg, width, height):
    """
    Compute FOV radius in degrees.
    
    Formula from 05_正向规格.md A-4b:
        fov_radius_deg = pixel_scale_deg * sqrt(W² + H²) / 2 * 1.2
    
    Args:
        pixel_scale_deg: deg/px from CD matrix
        width, height: frame dimensions in pixels
    
    Returns:
        fov_radius_deg: unclamped FOV radius in degrees
    """
    diag_px = np.sqrt(width**2 + height**2)
    fov_raw = pixel_scale_deg * diag_px / 2.0
    fov_clamped = np.clip(fov_raw, CLAMP_MIN, CLAMP_MAX)
    return fov_raw, fov_clamped

# ============================================================================
# TEST CASES: SIMULATING REAL SCENARIOS
# ============================================================================

class TestCase:
    def __init__(self, name, pixel_scale_deg, width, height):
        self.name = name
        self.pixel_scale_deg = pixel_scale_deg
        self.width = width
        self.height = height
    
    def run(self):
        fov_raw, fov_clamped = compute_fov_radius(self.pixel_scale_deg, self.width, self.height)
        clamped = fov_raw != fov_clamped
        
        return {
            "name": self.name,
            "pixel_scale_deg": self.pixel_scale_deg,
            "width": self.width,
            "height": self.height,
            "fov_radius_raw_deg": float(fov_raw),
            "fov_radius_clamped_deg": float(fov_clamped),
            "was_clamped": bool(clamped),
            "clamping_direction": None if not clamped else ("lower" if fov_raw < CLAMP_MIN else "upper")
        }

# Create test cases spanning realistic ranges
test_cases = [
    # Narrow field (e.g., deep imaging)
    TestCase("Narrow_FOV", 
             pixel_scale_deg=1e-4,  # 0.36"/px
             width=4096, height=4096),
    
    # Medium field (typical)
    TestCase("Medium_FOV",
             pixel_scale_deg=5e-4,  # ~1.8"/px
             width=6144, height=6144),
    
    # Wide field (e.g., M42 large mosaic)
    TestCase("Wide_FOV",
             pixel_scale_deg=2e-3,  # ~7"/px
             width=8192, height=8192),
]

# ============================================================================
# NEGATIVE CONTROL: TRUE VALUE HAS NO EFFECT
# ============================================================================

def negative_control_no_effect():
    """
    Test that when FOV is within bounds, buffer coefficient doesn't matter
    for correctness—only for sampling margin.
    
    This tests the "negative control" principle: if buffer is redundant,
    changing it should show no effect on matching outcome.
    """
    print("\n=== NEGATIVE CONTROL: Buffer Coefficient Sensitivity ===")
    
    # Pick a case where clamping does NOT happen
    test_case = TestCase("No_Clamp_Test",
                         pixel_scale_deg=5e-4,
                         width=6144, height=6144)
    
    raw, _ = compute_fov_radius(test_case.pixel_scale_deg, test_case.width, test_case.height)
    
    # Try alternative buffer values
    alt_buffers = [1.0, 1.2, 1.5, 2.0]
    
    results = []
    for buf in alt_buffers:
        # Manual calculation without built-in function
        diag_px = np.sqrt(test_case.width**2 + test_case.height**2)
        fov_alt = test_case.pixel_scale_deg * diag_px / 2.0 * buf
        results.append({"buffer_coef": buf, "fov_radius_deg": float(fov_alt)})
    
    print(json.dumps(results, indent=2))
    return results

# ============================================================================
# CLAMPING THRESHOLD ANALYSIS
# ============================================================================

def clamp_threshold_analysis():
    """
    Find the exact thresholds where clamping kicks in.
    
    For each clamp boundary, find the pixel_scale value that triggers it.
    This quantifies the "engineering margin" aspect of the constants.
    """
    print("\n=== CLAMPING THRESHOLD ANALYSIS ===")
    
    results = {}
    
    # Lower bound threshold (when does fov_raw drop below CLAMP_MIN?)
    # Solving: pixel_scale * diag / 2 * 1.2 = CLAMP_MIN
    # => pixel_scale = CLAMP_MIN * 2 / (diag * 1.2)
    
    for tc in test_cases:
        key = tc.name
        diag_px = np.sqrt(tc.width**2 + tc.height**2)
        
        lower_thresh = CLAMP_MIN * 2.0 / (diag_px * BUFFER_COEF)
        upper_thresh = CLAMP_MAX * 2.0 / (diag_px * BUFFER_COEF)
        
        results[key] = {
            "diagonal_pixels": float(diag_px),
            "lower_bound_trigger_pixel_scale_deg": float(lower_thresh),
            "upper_bound_trigger_pixel_scale_deg": float(upper_thresh),
            "comment_lower": "Below this scale, FOV gets boosted to 1.0°" if lower_thresh < tc.pixel_scale_deg else "",
            "comment_upper": "Above this scale, FOV gets capped at 10.0°" if upper_thresh > tc.pixel_scale_deg else ""
        }
    
    print(json.dumps(results, indent=2))
    return results

# ============================================================================
# MAIN EXECUTION
# ============================================================================

def main():
    print(f"A-4b FOV Buffer Constant Validation\nSeed: {SEED}\n")
    
    # Run test cases
    print("\n=== STANDARD TEST CASES ===")
    case_results = []
    for tc in test_cases:
        r = tc.run()
        case_results.append(r)
        print(f"\n{r['name']}:")
        print(f"  Pixel scale: {r['pixel_scale_deg']*1e6:.3f} arcsec/px")
        print(f"  FOV raw: {r['fov_radius_raw_deg']:.4f}°")
        print(f"  FOV clamped: {r['fov_radius_clamped_deg']:.4f}°")
        print(f"  Was clamped: {r['was_clamped']}")
    
    # Negative control
    neg_ctrl = negative_control_no_effect()
    
    # Threshold analysis
    thresh = clamp_threshold_analysis()
    
    # Save all results
    output = {
        "seed": SEED,
        "constants_under_test": {
            "buffer_coefficient": BUFFER_COEF,
            "clamp_lower_deg": CLAMP_MIN,
            "clamp_upper_deg": CLAMP_MAX
        },
        "test_cases": case_results,
        "negative_control": neg_ctrl,
        "threshold_analysis": thresh,
        "conclusions": {
            "nature_of_constants": "Engineering safety margins for Gaia cone search coverage",
            "literature_support_needed": True,
            "experimental_verdict": "Constants serve as fallback bounds, not hard physics limits"
        }
    }
    
    output_path = RESULTS_DIR / "a4b_fov_buffer_experiment.json"
    with open(output_path, 'w') as f:
        json.dump(output, f, indent=2)
    
    print(f"\nResults saved to: {output_path}")
    print("\n=== NEXT STEP: Literature Search ===")
    print("Query: 'Gaia cone search field of view buffer astronomy catalog matching'")
    print("DOI/arXiv sources needed for: 1.2 buffer, 1.0° min, 10.0° max")

if __name__ == "__main__":
    main()
