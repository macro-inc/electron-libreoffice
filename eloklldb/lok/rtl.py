import lldb
from ..type_utils import Category, cleanup_name


def rtl_reference_summary(valobj: lldb.SBValue) -> str:
    typename = cleanup_name(valobj.GetType().GetName())
    iface = valobj.GetChildMemberWithName("m_pBody")
    if iface and iface.IsValid():
        iface.SetPreferDynamicValue(lldb.eDynamicCanRunTarget)
        return f"{typename} to ({iface.GetTypeName()}) {iface}"
    else:
        return f"empty {typename}"


class RtlReferenceSynthProvider:
    def __init__(self, valobj: lldb.SBValue):
        self.valobj = valobj

    def num_children(self):
        return 0

    def get_child_index(self, name: str):
        if name == "$$dereference$$":
            return 0
        return -1

    def get_child_at_index(self, index: int):
        if index == 0:
            iface = self.valobj.GetChildMemberWithName("m_pBody")
            if iface and iface.IsValid():
                iface.SetPreferDynamicValue(lldb.eDynamicCanRunTarget)
                return iface.Dereference()
            return None
        return None


def cppu_threadpool_thread_pool_summary(valobj: lldb.SBValue):
    """
    Prints cppu_threadpool::ThreadPool objects
    Prevents infinite recursion when accessing rtl::Reference due to a circular reference
    """
    return f"{valobj.GetTypeName()}@{valobj.GetLoadAddress():02x}"


def register_formatters(debugger: lldb.SBDebugger):
    cppu = Category(debugger, "cppu", __name__)
    cppu.addSummary("cppu_threadpool::ThreadPool", cppu_threadpool_thread_pool_summary)
    rtl = Category(debugger, "rtl", __name__)
    rtl.addSummary(
        "^rtl::Reference<.+>$",
        rtl_reference_summary,
        is_regex=True,
        expand_children=True,
    )
    rtl.addSynthetic(
        "^rtl::Reference<.+>$",
        RtlReferenceSynthProvider,
        is_regex=True,
    )
