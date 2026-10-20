"""Independent external-point validation and legacy-cone falsification.

The forward interval algorithm does not call the inverse-chart solver. External
HEALPix points and Astropy forward WCS are validation, never bound primitives.
"""
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from astropy import units as u
from astropy.wcs import WCS
from astropy_healpix import HEALPix
from mpmath import mp

import detector_independent as detector


def vector_from_world(world):
    lon,lat = np.deg2rad(np.asarray(world).T)
    return np.stack((np.cos(lat)*np.cos(lon),np.cos(lat)*np.sin(lon),np.sin(lat)),axis=-1)


def mp_vector(model,xy):
    (x,y,z),norm = model.vector_interval((xy[0],xy[0],xy[1],xy[1]))
    return [(mp.mpf(v.a)+mp.mpf(v.b))/2/( (mp.mpf(norm.a)+mp.mpf(norm.b))/2) for v in (x,y,z)]


def separation_mp(a,b):
    cross = [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
    return mp.atan2(mp.sqrt(sum(t*t for t in cross)),sum(x*y for x,y in zip(a,b)))


def main():
    rng = np.random.default_rng(111)
    point_failures,vector_checks = [],[]
    point_count = 0
    for nside in (1,8,262144):
        hp = HEALPix(nside=nside,order="nested")
        for face in range(12):
            for _ in range(8):
                pixel = face*nside*nside+int(rng.integers(nside*nside))
                dx,dy = rng.uniform(.1,.9,size=2)
                lon,lat = hp.healpix_to_lonlat(pixel,dx=dx,dy=dy)
                model = detector.DetectorModel(detector.make_header(lon.deg,lat.deg,.001))
                candidates = detector.interval_floor_candidates(model,(0.,0.,0.,0.),nside)
                external = int(hp.lonlat_to_healpix(lon,lat))
                point_count += 1
                if pixel not in candidates or external not in candidates:
                    point_failures.append({"nside":nside,"face":face,"pixel":pixel,"candidates":candidates,"external":external})
    # Include exact poles and all four FITS LONPOLE rotations. At the pole RA
    # itself is undefined; compare unit vectors, never raw RA differences.
    with mp.workdps(70):
        for lat in (-90.,-89.9,0.,89.9,90.):
            for pole in (0.,90.,180.,270.):
                header = detector.make_header(20.,lat,.001,sip=True)
                header["LONPOLE"] = pole
                model = detector.DetectorModel(header)
                for point in ((0.,0.),(.1,.2),(-.3,.2)):
                    reference = vector_from_world(model.wcs.all_pix2world([point],0))[0]
                    value = mp_vector(model,point)
                    difference = float(separation_mp(value,list(map(mp.mpf,reference))))
                    vector_checks.append({"crval_lat_deg":lat,"lonpole_deg":pole,"detector_point":point,"astropy_vector_separation_rad":difference})
    # Reproduce the actual omitted member, with the exact header coefficients
    # in the cross-solver failure. Tiny formatting changes can alter the legacy
    # query's numerical result, which is itself a reason not to certify with it.
    header = detector.make_header(359.99999,89.9,1/3600,sip=True)
    for key,value in {"CD1_1":-.000262644048777588,"CD1_2":9.043559846032131e-5,
                      "CD2_1":9.043559846032131e-5,"CD2_2":.000262644048777588,"RADESYS":"ICRS"}.items():
        header[key] = value
    model = detector.DetectorModel(header)
    rect = (.175,.35,-.175,0.)
    center = ((rect[0]+rect[1])/2,(rect[2]+rect[3])/2)
    witness = (.35,0.)
    lon,lat,radius = model.cap(rect,3e-14)
    hp = HEALPix(nside=262144,order="nested")
    legacy = tuple(map(int,hp.cone_search_lonlat(lon*u.deg,lat*u.deg,radius*u.rad)))
    sky = model.wcs.all_pix2world([witness],0)[0]
    external_member = int(hp.lonlat_to_healpix(sky[0]*u.deg,sky[1]*u.deg))
    with mp.workdps(70):
        distance = separation_mp(mp_vector(model,center),mp_vector(model,witness))
        ratio = float(distance/mp.mpf(radius))
        distance_text = mp.nstr(distance,60)
    fixed_candidates = detector.interval_floor_candidates(model,rect,262144)
    probe = {"header":dict(header),"detector_rect":rect,"detector_center":center,"witness":witness,
             "cap_lon_deg":lon,"cap_lat_deg":lat,"cap_radius_rad":radius,
             "high_precision_model_separation_rad":distance_text,"separation_over_cap_radius":ratio,
             "legacy_cone_candidates":legacy,"external_witness_member":external_member,
             "fixed_interval_candidates":fixed_candidates,
             "witness_inside_cap":ratio<1,"legacy_misses_witness_member":external_member not in legacy,
             "fixed_includes_witness_member":external_member in fixed_candidates}
    spill = detector.solve_overlap(header,(0.,0.),.7,262144,max_nodes=1,max_candidate_combinations=1)
    out = {"source_sha256":hashlib.sha256(Path(detector.__file__).read_bytes()).hexdigest(),
           "seed":111,"all_12_faces_resolutions":[1,8,262144],"interior_point_count":point_count,
           "interior_point_candidate_omissions":point_failures,
           "forward_vectors":vector_checks,
           "maximum_Astropy_unit_vector_separation_rad":max(c["astropy_vector_separation_rad"] for c in vector_checks),
           "legacy_cone_completeness_counterexample":probe,"allsky_spill_case":spill,
           "qualification":"New interval classification is the bound primitive. External point membership and Astropy vectors are numerical validation. Legacy cone interval claims are falsified, not weakened into looser bounds."}
    path = Path(__file__).resolve().parent.parent/"evidence"/"detector_interval_validation.json"
    path.write_text(json.dumps(out,indent=2),encoding="utf-8")
    print("points",point_count,"omissions",len(point_failures),"max vector distance",out["maximum_Astropy_unit_vector_separation_rad"])
    print("legacy misses",probe["legacy_misses_witness_member"],"fixed includes",probe["fixed_includes_witness_member"],"distance/radius",ratio)
    print("spill",spill["unresolved_coarse_candidates"])


if __name__ == "__main__":
    main()
