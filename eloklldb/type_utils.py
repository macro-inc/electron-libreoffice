import lldb
from typing import Callable, Any, Protocol, Optional


class SyntheticValueProvider(Protocol):
    def __init__(self, valobj: lldb.SBValue):
        ...

    def num_children(self) -> int:
        ...

    def get_child_index(self, name: str) -> Optional[int]:
        ...

    def get_child_at_index(self, index: int) -> Optional[lldb.SBValue]:
        ...

    ...


class Category:
    def __init__(self, debugger: lldb.SBDebugger, name: str, namespace: str):
        self.name = name
        self.category: lldb.SBTypeCategory = debugger.GetCategory(name)
        self.namespace = namespace
        if not self.category:
            self.category = debugger.CreateCategory(name)
            self.category.AddLanguage(lldb.eLanguageTypeC_plus_plus)
            self.category.SetEnabled(True)

    def addSummary(
        self,
        name: str,
        typeSummaryFunction: Callable[[lldb.SBValue], str],
        is_regex: bool = False,
        expand_children: bool = False,
        skip_pointers: bool = False,
        skip_refs: bool = False,
        cascade: bool = False,
    ):
        summary: lldb.SBTypeSummary = lldb.SBTypeSummary.CreateWithFunctionName(
            f"{self.namespace}.{typeSummaryFunction.__name__}"
        )
        options = (
            lldb.eTypeOptionNone if expand_children else lldb.eTypeOptionHideChildren
        )
        if skip_pointers:
            options |= lldb.eTypeOptionSkipPointers
        if skip_refs:
            options |= lldb.eTypeOptionSkipReferences
        if cascade:
            options |= lldb.eTypeOptionCascade

        summary.SetOptions(options)
        self.category.AddTypeSummary(lldb.SBTypeNameSpecifier(name, is_regex), summary)

    def addSynthetic(
        self,
        name: str,
        typeSyntheticClass: type[SyntheticValueProvider],
        is_regex: bool = False,
    ):
        self.category.AddTypeSynthetic(
            lldb.SBTypeNameSpecifier(name, is_regex),
            lldb.SBTypeSynthetic.CreateWithClassName(
                f"{self.namespace}.{typeSyntheticClass.__name__}"
            ),
        )


def cleanup_name(name: str) -> str:
    return name.replace("com::sun::star::", "")
