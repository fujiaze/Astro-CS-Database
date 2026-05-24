from __future__ import annotations

from typing import Union
from pathlib import Path
import numpy as np
import numpy.typing as npt

NDArrayFloat = npt.NDArray[np.float32]
NDArrayFloat64 = npt.NDArray[np.float64]
NDArrayInt = npt.NDArray[np.int32]
ImageArray = NDArrayFloat
PathLike = Union[str, Path]
