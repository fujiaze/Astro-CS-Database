from __future__ import annotations

from dataclasses import dataclass


@dataclass(slots=True, frozen=True)
class FITSKeyword:
    """FITS 头关键字，对应 PCL FITSHeaderKeyword。

    Attributes
    ----------
    name : str
        关键字名，大写 ASCII。
    value : str
        关键字值，字符串表示。
    comment : str
        注释文本。
    """

    name: str
    value: str
    comment: str = ""


def make_keyword(
    header_key: str,
    header_value: object,
    header_comment: str = "",
) -> FITSKeyword:
    """从 astropy header 条目构造 FITSKeyword。

    Parameters
    ----------
    header_key : str
        关键字名。
    header_value : object
        关键字值，会转为字符串。
    header_comment : str
        注释文本。

    Returns
    -------
    FITSKeyword
    """
    return FITSKeyword(
        name=str(header_key).strip().upper(),
        value=str(header_value).strip(),
        comment=str(header_comment).strip(),
    )
