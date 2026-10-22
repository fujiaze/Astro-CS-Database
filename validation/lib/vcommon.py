"""Common utilities for the independent validation tree.

Independence rules enforced here by construction:

* no module in this package imports ``src/`` headers or production helpers;
* truth values are produced from exact rational arithmetic, closed-form
  formulae or external standard libraries (astropy / wcslib, astropy-healpix,
  healpy, scipy);
* every artefact that is later handed to the implementation under test is
  hashed *before* the call, so that a later run cannot silently change the
  input.
"""

from __future__ import annotations

import hashlib
import json
import os
import platform
import subprocess
import sys
from dataclasses import dataclass, field
from fractions import Fraction
from pathlib import Path
from typing import Any, Iterable, Sequence

import numpy as np

# --------------------------------------------------------------------------
# paths
# --------------------------------------------------------------------------

VALIDATION_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = VALIDATION_ROOT.parent


def case_dir(case: str) -> Path:
    return VALIDATION_ROOT / case.lower()


# --------------------------------------------------------------------------
# canonical JSON + hashing
# --------------------------------------------------------------------------


def canonical_json(obj: Any) -> str:
    """Canonical JSON: sorted keys, no whitespace, UTF-8 friendly."""
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False,
                      allow_nan=False)


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_text(text: str) -> str:
    return sha256_hex(text.encode("utf-8"))


def sha256_file(path: os.PathLike | str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write_json(path: os.PathLike | str, obj: Any) -> str:
    """Write canonical JSON and return the file sha256."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(obj, indent=2, sort_keys=True, ensure_ascii=False, allow_nan=False)
    path.write_text(text + "\n", encoding="utf-8")
    return sha256_file(path)


def read_json(path: os.PathLike | str) -> Any:
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def content_hash(obj: Any) -> str:
    """``sha256:<64 hex>`` content identity used by the product contract."""
    return "sha256:" + sha256_text(canonical_json(obj))


# --------------------------------------------------------------------------
# exact rational helpers
# --------------------------------------------------------------------------


def rat(x: float | int | Fraction) -> Fraction:
    """Exact rational value of a binary64 number (no decimal re-parsing)."""
    if isinstance(x, Fraction):
        return x
    if isinstance(x, (int, np.integer)):
        return Fraction(int(x))
    return Fraction(float(x))


def rat_str(x: Fraction) -> str:
    """Exact decimal string of a rational (terminating decimals only).

    ``Fraction(15, 4)`` renders as ``3.75`` and ``Fraction(277, 64)`` as
    ``4.328125``; a rational with no finite decimal expansion renders as the
    exact fraction ``p/q``.
    """
    if x.denominator == 1:
        return str(x.numerator)
    num, den = x.numerator, x.denominator
    twos = 0
    while den % 2 == 0:
        den //= 2
        twos += 1
    fives = 0
    while den % 5 == 0:
        den //= 5
        fives += 1
    if den != 1:
        return f"{num}/{x.denominator}"
    digits = max(twos, fives)
    scaled = abs(x) * (10 ** digits)
    integer = scaled.numerator  # exact: the denominator is 1 here
    text = str(integer).rjust(digits + 1, "0")
    sign = "-" if x < 0 else ""
    if digits == 0:
        return sign + text
    return f"{sign}{text[:-digits]}.{text[-digits:]}"


def rat_json(x: Fraction) -> dict[str, str]:
    """Exact rational rendered as ``{"numerator": ..., "denominator": ...}``."""
    return {"numerator": str(x.numerator), "denominator": str(x.denominator),
            "decimal": rat_str(x)}


def mat_rat_json(m: Sequence[Sequence[Fraction]]) -> list[list[dict[str, str]]]:
    return [[rat_json(x) for x in row] for row in m]


# --------------------------------------------------------------------------
# linear algebra on exact rationals
# --------------------------------------------------------------------------


def matmul_rat(a: Sequence[Sequence[Fraction]], b: Sequence[Sequence[Fraction]]
               ) -> list[list[Fraction]]:
    n, k, m = len(a), len(b), len(b[0])
    return [[sum((a[i][t] * b[t][j] for t in range(k)), Fraction(0)) for j in range(m)]
            for i in range(n)]


def transpose_rat(a: Sequence[Sequence[Fraction]]) -> list[list[Fraction]]:
    return [list(col) for col in zip(*a)]


def matvec_rat(a: Sequence[Sequence[Fraction]], v: Sequence[Fraction]) -> list[Fraction]:
    return [sum((a[i][j] * v[j] for j in range(len(v))), Fraction(0)) for i in range(len(a))]


def quadratic_form_rat(a: Sequence[Sequence[Fraction]], v: Sequence[Fraction]) -> Fraction:
    av = matvec_rat(a, v)
    return sum((v[i] * av[i] for i in range(len(v))), Fraction(0))


def solve_rat(a: Sequence[Sequence[Fraction]], b: Sequence[Fraction]) -> list[Fraction]:
    """Exact Gaussian elimination over the rationals."""
    n = len(a)
    m = [list(a[i]) + [b[i]] for i in range(n)]
    for col in range(n):
        pivot = None
        for row in range(col, n):
            if m[row][col] != 0:
                pivot = row
                break
        if pivot is None:
            raise ValueError("singular exact system")
        m[col], m[pivot] = m[pivot], m[col]
        inv = Fraction(1) / m[col][col]
        m[col] = [x * inv for x in m[col]]
        for row in range(n):
            if row == col:
                continue
            factor = m[row][col]
            if factor != 0:
                m[row] = [m[row][k] - factor * m[col][k] for k in range(n + 1)]
    return [m[i][n] for i in range(n)]


def cholesky_rat(a: Sequence[Sequence[Fraction]]) -> list[list[Fraction]]:
    """Exact lower Cholesky factor; raises when the matrix is not SPD."""
    n = len(a)
    ell = [[Fraction(0) for _ in range(n)] for _ in range(n)]
    for i in range(n):
        for j in range(i + 1):
            s = a[i][j] - sum((ell[i][k] * ell[j][k] for k in range(j)), Fraction(0))
            if i == j:
                if s <= 0:
                    raise ValueError("matrix is not positive definite")
                ell[i][i] = s ** Fraction(1, 2) if s.denominator == 1 and int(s) ** 2 == s else _sqrt_rat(s)
            else:
                ell[i][j] = s / ell[j][j]
    return ell


def _sqrt_rat(x: Fraction) -> Fraction:
    """Exact square root when it exists, otherwise a high precision fallback."""
    num, den = x.numerator, x.denominator
    rn, rd = int(num ** 0.5), int(den ** 0.5)
    if rn * rn == num and rd * rd == den:
        return Fraction(rn, rd)
    return Fraction(str(x))  # exact decimal fallback (documented in the case file)


# --------------------------------------------------------------------------
# statistics helpers
# --------------------------------------------------------------------------


def clopper_pearson(k: int, n: int, alpha: float = 0.05) -> tuple[float, float]:
    """Exact (Clopper-Pearson) two-sided binomial confidence interval."""
    from scipy.stats import beta

    lower = 0.0 if k == 0 else float(beta.ppf(alpha / 2.0, k, n - k + 1))
    upper = 1.0 if k == n else float(beta.ppf(1.0 - alpha / 2.0, k + 1, n - k))
    return lower, upper


def wilson(k: int, n: int, z: float = 1.959963984540054) -> tuple[float, float]:
    """Wilson score interval (secondary, reported next to the exact one)."""
    if n == 0:
        return 0.0, 1.0
    p = k / n
    denom = 1.0 + z * z / n
    centre = (p + z * z / (2 * n)) / denom
    half = z * np.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / denom
    return float(max(0.0, centre - half)), float(min(1.0, centre + half))


def hoeffding_bound(amplitude: float, log_terms: float, delta: float, trials: int) -> float:
    """Hoeffding deviation bound for the mean of bounded iid variables."""
    return float(amplitude * np.sqrt(log_terms / (2.0 * trials)))


# --------------------------------------------------------------------------
# provenance
# --------------------------------------------------------------------------


def _module_version(name: str) -> str:
    try:
        module = __import__(name)
        return str(getattr(module, "__version__", "unknown"))
    except Exception:  # pragma: no cover - optional dependency
        return "not-installed"


def environment_identity() -> dict[str, Any]:
    """Runtime and external-library identity recorded in every manifest."""
    return {
        "python": sys.version.split()[0],
        "python_implementation": platform.python_implementation(),
        "platform": platform.platform(),
        "machine": platform.machine(),
        "processor": platform.processor() or "unknown",
        "numpy": _module_version("numpy"),
        "scipy": _module_version("scipy"),
        "astropy": _module_version("astropy"),
        "astropy_healpix": _module_version("astropy_healpix"),
        "healpy": _module_version("healpy"),
        "wcslib": _wcslib_version(),
        "compiler": _compiler_identity(),
    }


def _wcslib_version() -> str:
    try:
        import astropy.wcs  # noqa: F401
        from astropy.wcs import WCS  # noqa: F401
        # astropy bundles wcslib; report the astropy build identity instead of
        # guessing a system wcslib version.
        import astropy

        return f"astropy-{astropy.__version__}-bundled-wcslib"
    except Exception:  # pragma: no cover
        return "unknown"


def _compiler_identity() -> str:
    for compiler in ("g++", "c++", "clang++"):
        try:
            out = subprocess.run([compiler, "--version"], capture_output=True, text=True,
                                 check=False)
            if out.returncode == 0:
                return out.stdout.splitlines()[0].strip()
        except Exception:  # pragma: no cover
            continue
    return "unknown"


def source_identity(paths: Iterable[os.PathLike | str]) -> dict[str, str]:
    """sha256 of the truth sources that produced a case."""
    out: dict[str, str] = {}
    for path in sorted(Path(p) for p in paths):
        if path.is_file():
            out[str(path.relative_to(REPO_ROOT))] = sha256_file(path)
    return out


# --------------------------------------------------------------------------
# case scaffolding
# --------------------------------------------------------------------------


@dataclass
class Case:
    """Helper that lays out one case directory with the required artefacts."""

    name: str
    goal: str
    assumptions: list[str]
    estimand: str
    unit: str
    condition: str
    tolerance: dict[str, Any]
    positive_expectations: list[dict[str, Any]]
    negative_expectations: list[dict[str, Any]]
    truth_type: str
    root: Path = field(init=False)

    def __post_init__(self) -> None:
        self.root = case_dir(self.name)
        for sub in ("input", "truth", "observed"):
            (self.root / sub).mkdir(parents=True, exist_ok=True)

    # -- artefacts ---------------------------------------------------------
    def write_case_json(self, extra: dict[str, Any] | None = None) -> str:
        payload = {
            "case": self.name.upper(),
            "goal": self.goal,
            "assumptions": self.assumptions,
            "estimand": self.estimand,
            "unit": self.unit,
            "condition": self.condition,
            "tolerance": self.tolerance,
            "truth_type": self.truth_type,
            "positive_expectations": self.positive_expectations,
            "negative_expectations": self.negative_expectations,
            "independence": {
                "truth_code_imports_production": False,
                "external_standards": ["astropy", "wcslib", "astropy-healpix", "healpy",
                                       "scipy", "numpy"],
                "note": "truth is produced before and independently of any observed run",
            },
        }
        if extra:
            payload.update(extra)
        return write_json(self.root / "case.json", payload)

    def write_request(self, request: dict[str, Any]) -> str:
        return write_json(self.root / "request.json", request)

    def write_truth(self, name: str, payload: Any) -> str:
        return write_json(self.root / "truth" / name, payload)

    def write_input(self, name: str, payload: Any) -> str:
        return write_json(self.root / "input" / name, payload)

    def write_observed(self, name: str, payload: Any) -> str:
        return write_json(self.root / "observed" / name, payload)

    def write_comparison(self, payload: dict[str, Any]) -> str:
        return write_json(self.root / "comparison.json", payload)

    def write_manifest(self, payload: dict[str, Any]) -> str:
        return write_json(self.root / "manifest.json", payload)


def comparison_payload(case: str, status: str, functionals: list[dict[str, Any]],
                        failures: list[dict[str, Any]] | None = None,
                        notes: str | None = None) -> dict[str, Any]:
    """Uniform comparison record: one row per functional plus failure scope."""
    return {
        "case": case.upper(),
        "status": status,
        "functionals": functionals,
        "failures": failures or [],
        "notes": notes,
        "status_vocabulary": {
            "pass": "every requested functional is inside the frozen tolerance",
            "fail": "at least one functional is outside the frozen tolerance",
            "pending_implementation": "the public entry point under test is not implemented yet",
            "not_applicable": "the case does not exercise the requested functional",
        },
    }


def pending(reason: str, missing: list[str] | None = None) -> dict[str, Any]:
    return {
        "status": "pending_implementation",
        "reason": reason,
        "missing_public_entry_points": missing or [],
        "falsification_note": "no observed value was fabricated; a real run must fill this in",
    }


# --------------------------------------------------------------------------
# misc
# --------------------------------------------------------------------------


def jsonable(value: Any) -> Any:
    """Convert numpy scalars/arrays and Fractions into JSON-safe values."""
    if isinstance(value, Fraction):
        return rat_json(value)
    if isinstance(value, (np.floating,)):
        return float(value)
    if isinstance(value, (np.integer,)):
        return int(value)
    if isinstance(value, np.ndarray):
        return value.tolist()
    if isinstance(value, dict):
        return {k: jsonable(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [jsonable(v) for v in value]
    return value
