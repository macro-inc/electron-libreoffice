import lldb
from ..type_utils import Category

def cppu_threadpool_threadpool_summary(valobj: lldb.SBValue, dict) -> str:
    typename = valobj.GetType().GetName()
    return f"{typename}@{valobj.GetAddress()}"

def register_formatters(debugger: lldb.SBDebugger):
    cateogry = Category(debugger, "uno", __name__)
    cateogry.addSummary("_uno_Any", uno_any_summary)
    cateogry.addSummary("com::sun::star::uno::Any", uno_any_summary)
    cateogry.addSummary("com::sun::star::uno::Reference", uno_reference_summary)
    cateogry.addSummary("com::sun::star::uno::Sequence", uno_sequence_summary)
    cateogry.addSummary("com::sun::star::uno::Type", uno_type_summary)

