"""声明的NESTED/FITS朝向，经独立HPX WCSLIB和外库叶坐标对照。

从HPX投影的线性face参数自行推导FITS header，不调用ACSD读写器。
不声称执行了第三方真实HiPS生产者或旧ACSD生产E2E。
"""
import hashlib
import json
from pathlib import Path
import astropy.units as u
from astropy.io.fits import Header
from astropy.wcs import WCS
from astropy_healpix import HEALPix
import numpy as np


def spread(x):
    result=0
    for k in range(30):
        result+=((int(x)>>k)&1)<<(2*k)
    return result


def vector(degrees):
    ra,dec=np.radians(np.asarray(degrees)).T
    return np.stack((np.cos(dec)*np.cos(ra),np.cos(dec)*np.sin(ra),np.sin(dec)),axis=-1)


def main():
    jr=[2]*4+[3]*4+[4]*4
    jp=[1,3,5,7,0,2,4,6,1,3,5,7]
    rng=np.random.default_rng(1117010)
    records=[]
    for nside,width in ((8,8),(512,512),(262144,512)):
        hp=HEALPix(nside=nside,order='nested')
        for face in range(12):
            origin=(0,0) if nside==width else tuple(int(x)*width for x in rng.integers(0,nside//width,2))
            # HPX x=45(Lf+s−t), y=45(2−Jf+s+t)。row=W−1−ix,col=iy。
            # 分面本地无经度中断的线性offset；WCSLIB负责HPX真实球面分支。
            # 赤道face6跨原生±180中断，旋转投影中心到180而不改数组原点。
            center_ra=180. if face==6 else 0.
            x0=45*(jp[face]+(origin[0]-origin[1]+width-1)/nside)-center_ra
            # HPX原生经度选[-180,180]表达，避免不必要的超投影范围。
            if x0>180:
                x0-=360
            y0=45*(2-jr[face]+(origin[0]+origin[1]+width)/nside)
            cd=45/nside*np.array([[-1.,-1.],[1.,-1.]])
            crpix=np.ones(2)-np.linalg.solve(cd,[x0,y0])
            h=Header({'NAXIS':2,'NAXIS1':width,'NAXIS2':width,'CTYPE1':'RA---HPX','CTYPE2':'DEC--HPX',
                      'RADESYS':'ICRS','CUNIT1':'deg','CUNIT2':'deg','CRVAL1':center_ra,'CRVAL2':0.,
                      'CRPIX1':crpix[0],'CRPIX2':crpix[1],'CD1_1':cd[0,0],'CD1_2':cd[0,1],
                      'CD2_1':cd[1,0],'CD2_2':cd[1,1]})
            points=np.vstack(([[0,0],[width-1,0],[0,width-1],[width-1,width-1]],rng.integers(0,width,(96,2))))
            sky=WCS(h).all_pix2world(points,0)
            lonlat=[]
            for col,row in points:
                ix=origin[0]+width-1-int(row);iy=origin[1]+int(col)
                index=face*nside*nside+spread(ix)+2*spread(iy)
                lon,lat=hp.healpix_to_lonlat(index)
                lonlat.append((lon.to_value(u.deg),lat.to_value(u.deg)))
            a,b=vector(sky),vector(lonlat)
            error=np.arctan2(np.linalg.norm(np.cross(a,b),axis=1),np.sum(a*b,axis=1))
            records.append({'face':face,'nside':nside,'tile_width':width,'origin_ix_iy':origin,
                            'samples':len(points),'max_angle_rad':float(np.max(error)),
                            'nonfinite':int(np.sum(~np.isfinite(error))),'hpx_center_ra_deg':center_ra})
    output={'source_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'seed':1117010,
            'definition':'row=W-1-local_ix; column=local_iy; independent RA---HPX FITS WCS versus external NESTED centers',
            'claim_limit':'Declared array/header geometry only, not third-party HiPS producer or legacy writer E2E',
            'records':records}
    out=Path(__file__).resolve().parents[1]/'evidence/hips_orientation.json'
    out.write_text(json.dumps(output,indent=2),encoding='utf-8')
    print('samples',sum(x['samples'] for x in records),'nonfinite',sum(x['nonfinite'] for x in records),
          'max_angle_rad',max(x['max_angle_rad'] for x in records))


if __name__=='__main__':
    main()
