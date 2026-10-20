"""Pointwise diagnostic of external cone candidate over-inclusion at high nside.

Not a proof of a global precision floor. No geometry solver status is changed.
"""
import json
from pathlib import Path

import astropy_healpix
from astropy_healpix import HEALPix
from astropy import units as u


if __name__ == "__main__":
    pixel,nside = 68719213311,262144
    hp = HEALPix(nside=nside,order="nested")
    radii = [0.,1e-16,1e-12,1e-10]
    rows = []
    for dx in [.5,.1,.01,.003,.001,.0001,1e-5]:
        lon,lat = hp.healpix_to_lonlat(pixel,dx=dx,dy=.5)
        rows.append({"dx":dx,"dy":.5,"lon_deg":lon.deg,"lat_deg":lat.deg,
                     "roundtrip_member":int(hp.lonlat_to_healpix(lon,lat)),
                     "cone_candidates_by_radius_rad":{str(r):list(map(int,hp.cone_search_lonlat(lon,lat,r*u.rad))) for r in radii}})
    out = {"nside":nside,"nested_pixel":pixel,"astropy_healpix":astropy_healpix.__version__,"points":rows,
           "observation":"At positive interior face offsets .003 and smaller, zero-radius and tiny-radius external cone queries return two candidates although external point membership returns the generating pixel. This demonstrates conservative extra candidates at these sampled points.",
           "qualification":"Pointwise external-library diagnostic only. It is not an area bound, not a proof of global precision floor, and not permission to discard any candidate."}
    target = Path(__file__).resolve().parent.parent/"evidence"/"detector_external_cone_diagnostic.json"
    target.write_text(json.dumps(out,indent=2),encoding="utf-8")
    print(out["observation"])
