import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from astro_image_io import ImageReaderFactory, FITSWriter, ImageData, ImageGeometry
import numpy as np

factory = ImageReaderFactory()

import sys
path = sys.argv[1] if len(sys.argv) > 1 else None

if path:
    print(f"Reading: {path}")
    img = factory.read(path)
    print(f"  Size: {img.width}x{img.height}, channels={img.channels}")
    print(f"  Format: {img.source_format}")
    print(f"  Data dtype: {img.data.dtype}, range: [{img.data.min():.1f}, {img.data.max():.1f}]")

    if img.has_wcs:
        print(f"  WCS: RA={img.metadata.wcs.crval1:.6f}, Dec={img.metadata.wcs.crval2:.6f}")
        print(f"  Pixel scale: {img.pixel_scale_arcsec:.2f} arcsec/px")

    if img.metadata and img.metadata.observation:
        obs = img.metadata.observation
        print(f"  Object: {obs.object_name}")
        if obs.focallen:
            print(f"  Focal length: {obs.focallen:.1f} mm")

    if img.metadata and img.metadata.calibration:
        cal = img.metadata.calibration
        print(f"  Exposure: {cal.exptime:.1f}s, Filter: {cal.filter_name}")

    print(f"  Keywords: {len(img.keywords)}")
else:
    print("Usage: python demo.py <fits_or_xisf_file>")
