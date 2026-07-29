"""Diagnostic script: Why are all masters missing?"""
import csv
import json
from pathlib import Path

REPO_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
P10_003_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-003"
P10_004_DIR = REPO_ROOT / "engineering_v1.2/evidence/P10-004"

# Load filter alias map
with open(P10_004_DIR / "FILTER_ALIAS_MAP.json", "r", encoding="utf-8") as f:
    fmap = json.load(f)


def normalize_filter(fmap, alias):
    if not alias:
        return None
    key = alias.strip()
    if not key:
        return None
    a2c = fmap.get("alias_to_canonical", {})
    if key in a2c:
        return a2c[key]
    upper = key.upper()
    if upper in a2c:
        return a2c[upper]
    return None


# Load masters
masters = []
csv_path = P10_003_DIR / "CALIBRATION_MASTER_INVENTORY.csv"
with open(csv_path, "r", encoding="utf-8") as f:
    reader = csv.DictReader(f)
    print("CSV columns:", reader.fieldnames)
    for row in reader:
        row["canonical_filter"] = normalize_filter(fmap, row.get("filter_from_header") or row.get("filter_from_filename") or "")
        try:
            row["exposure_s"] = float(row.get("exposure_from_header") or 0.0)
        except (ValueError, TypeError) as e:
            print(f"  ERR exposure parse: {row.get('exposure_from_header')!r} -> {e}")
            row["exposure_s"] = 0.0
        try:
            row["bin_int"] = int(row.get("bin_from_header") or 1)
        except (ValueError, TypeError) as e:
            print(f"  ERR bin parse: {row.get('bin_from_header')!r} -> {e}")
            row["bin_int"] = 1
        masters.append(row)

print(f"\nLoaded {len(masters)} masters")
print(f"\nFirst master keys: {list(masters[0].keys())}")
print(f"\nFirst master: device_id={masters[0]['device_id']!r}, type={masters[0]['master_type']!r}, bin_int={masters[0]['bin_int']!r}, image_size={masters[0]['image_size_from_header']!r}")

# Look at T4 Bias master
t4_bias = [m for m in masters if m["master_type"] == "Bias" and m["device_id"] == "T4"]
print(f"\nT4 Bias masters: {len(t4_bias)}")
for m in t4_bias:
    print(f"  device_id={m['device_id']!r} bin_int={m['bin_int']!r} image_size={m['image_size_from_header']!r}")

# Now simulate the match: device_id="T4", bin_int=1, image_size="4500x3600"
device_id = "T4"
bin_int = 1
image_size = "4500x3600"
print(f"\nSimulating match_bias(device_id={device_id!r}, bin_int={bin_int!r}, image_size={image_size!r})")

matches = [m for m in masters
           if m["master_type"] == "Bias"
           and m["device_id"] == device_id
           and m["bin_int"] == bin_int
           and m["image_size_from_header"] == image_size]

print(f"Matches: {len(matches)}")

# Try removing each condition to find which one fails
for cond_name, cond_fn in [
    ("master_type == Bias", lambda m: m["master_type"] == "Bias"),
    ("device_id == T4", lambda m: m["device_id"] == device_id),
    ("bin_int == 1", lambda m: m["bin_int"] == bin_int),
    ("image_size == 4500x3600", lambda m: m["image_size_from_header"] == image_size),
]:
    count = sum(1 for m in masters if cond_fn(m))
    print(f"  Condition '{cond_name}': {count} matches in masters")
    # Show what doesn't match
    for m in masters:
        if not cond_fn(m) and m["master_type"] == "Bias":
            print(f"    FAIL: {m['file_name']} - {cond_name} - actual: {m.get('device_id')!r}, {m.get('bin_int')!r}, {m.get('image_size_from_header')!r}")
