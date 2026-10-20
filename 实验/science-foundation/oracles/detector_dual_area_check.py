"""Reproduce an interval-certified counterexample to A = pixfrac² * Omega.

Bounds have the same external-library qualification as detector_independent.
"""
import hashlib
import json
import math
from pathlib import Path

import detector_independent as detector


if __name__ == "__main__":
    header = detector.make_header(45.,40.,4.,sip=True)
    center,pixfrac,nside = (.2,-.1),.37,1
    source = detector.solve_overlap(header,center,1.,nside,relative_tolerance=1e-5,max_nodes=10001)
    drop = detector.solve_overlap(header,center,pixfrac,nside,relative_tolerance=1e-5,max_nodes=10001)
    ratio = [math.nextafter(drop["total_area"]["lower_sr"]/source["total_area"]["upper_sr"],-math.inf),
             math.nextafter(drop["total_area"]["upper_sr"]/source["total_area"]["lower_sr"],math.inf)]
    out = {"parameters":{"header":str(header),"center_xy0":center,"pixfrac":pixfrac,"nside":nside},
           "source_omega":source,"drop_area":drop,"A_over_Omega_interval":ratio,"f_squared":pixfrac**2,
           "f_squared_excluded":not ratio[0] <= pixfrac**2 <= ratio[1],
           "detector_source_sha256":hashlib.sha256(Path(detector.__file__).read_bytes()).hexdigest(),
           "qualification":"Conditional numerical enclosure; not a proof of external WCSLIB/HEALPix rounding"}
    target = Path(__file__).resolve().parent.parent/"evidence"/"detector_dual_area.json"
    target.write_text(json.dumps(out,indent=2),encoding="utf-8")
    print(source["status"],drop["status"],ratio,"f_squared_excluded",out["f_squared_excluded"])
