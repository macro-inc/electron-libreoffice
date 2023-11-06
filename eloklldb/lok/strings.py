import lldb
import json
from typing import Callable
from ..type_utils import Category


def make_string(valobj: lldb.SBValue, encoding: str = "utf-8", length=-1) -> str:
    if length == 0 or not valobj:
        return '""'
    max_length = 512  # arbitrary limit to prevent binary blobs from dumping
    truncated = length < max_length
    if length < 0:
        length = 0
        while (
            valobj.GetPointeeData(item_idx=length, item_count=1)
            and length <= max_length
        ):
            length += 1
            truncated = length < max_length
    else:
        length = min(max_length, length)

    data = valobj.GetPointeeData(item_count=length)
    e = lldb.SBError()
    if e.Fail():
        return f"<error: {e.description}>"

    # escape the string as necesary
    string = json.dumps(data.ReadRawData(e, 0, data.GetByteSize()).decode(encoding))
    if truncated:
        string += "…"
    return string


def make_string_summary(
    valid_path: str,
    data_path: str,
    length_path: str | None = None,
    encoding: str = "utf-8",
) -> Callable[[lldb.SBValue], str]:
    """
    Creates a string summary function

    :param valid_path: Expression path to determine if the SBValue is valid.
    :param data_path: Expression path to the data for string creation.
    :param length_path: Optional expression path to the length of the string. Defaults to None.
    :param encoding: String encoding to be used. Defaults to "utf-8".
    :return: A callable that takes an lldb.SBValue and returns its string summary.
    """
    return (
        lambda value: make_string(
            value.GetValueForExpressionPath(data_path),
            length=value.GetValueForExpressionPath(length_path).unsigned
            if length_path
            else -1,
            encoding=encoding,
        )
        if value.GetValueForExpressionPath(valid_path)
        else "<invalid string>"
    )


rtl_String_summary = make_string_summary(".buffer", ".buffer", ".length")
rtl_uString_summary = make_string_summary(".buffer", ".buffer", ".length", "utf-16le")
rtl_OString_summary = make_string_summary(".pData", ".pData->buffer", ".pData->length")
rtl_OUString_summary = make_string_summary(
    ".pData", ".pData->buffer", ".pData->length", "utf-16"
)
rtl_OUString_summary = make_string_summary(
    ".pData", ".pData->buffer", ".pData->length", "utf-16"
)


def register_formatters(debugger: lldb.SBDebugger):
    category = Category(debugger, "rtl", __name__)
    category.addSummary("_rtl_String", rtl_String_summary, skip_refs=True)
    category.addSummary("_rtl_uString", rtl_uString_summary, skip_refs=True)
    category.addSummary(
        "rtl::OString", rtl_String_summary, skip_refs=True, skip_pointers=True
    )
    category.addSummary(
        "rtl::OUString", rtl_uString_summary, skip_refs=True, skip_pointers=True
    )
    category.addSummary(
        "rtl::OStringBuffer", rtl_String_summary, skip_refs=True, skip_pointers=True
    )
    category.addSummary(
        "rtl::OUStringBuffer", rtl_uString_summary, skip_refs=True, skip_pointers=True
    )
