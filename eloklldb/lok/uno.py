# /mnt/data/cppu.py

import lldb
from typing import Dict, Set
from ..type_utils import Category, cleanup_name


class TypeClass:
    VOID = 0
    CHAR = 1
    BOOLEAN = 2
    BYTE = 3
    SHORT = 4
    UNSIGNED_SHORT = 5
    LONG = 6
    UNSIGNED_LONG = 7
    HYPER = 8
    UNSIGNED_HYPER = 9
    FLOAT = 10
    DOUBLE = 11
    STRING = 12
    TYPE = 13
    ANY = 14
    ENUM = 15
    TYPEDEF = 16
    STRUCT = 17
    EXCEPTION = 19
    SEQUENCE = 20
    INTERFACE = 22
    SERVICE = 23
    MODULE = 24
    INTERFACE_METHOD = 25
    INTERFACE_ATTRIBUTE = 26
    UNKNOWN = 27
    PROPERTY = 28
    CONSTANT = 29
    CONSTANTS = 30
    SINGLETON = 31


PRIMITIVE_TO_CPP = {
    TypeClass.VOID: "void",
    TypeClass.CHAR: "char",
    TypeClass.BOOLEAN: "sal_Bool",
    TypeClass.BYTE: "sal_Int8",
    TypeClass.SHORT: "sal_Int16",
    TypeClass.UNSIGNED_SHORT: "sal_uInt16",
    TypeClass.LONG: "sal_Int32",
    TypeClass.UNSIGNED_LONG: "sal_uInt32",
    TypeClass.HYPER: "sal_Int64",
    TypeClass.UNSIGNED_HYPER: "sal_uInt64",
    TypeClass.FLOAT: "float",
    TypeClass.DOUBLE: "double",
    TypeClass.STRING: "rtl::OUString",
    TypeClass.TYPE: "com::sun::star::uno::Type",
    TypeClass.ANY: "com::sun::star::uno::Any",
}
UNO_TO_CPP = {
    TypeClass.ENUM,
    TypeClass.STRUCT,
    TypeClass.EXCEPTION,
    TypeClass.INTERFACE,
}

CSSU_TYPE = "com::sun::star::uno::Type"
TYPE_DESC = "_typelib_TypeDescription"
TYPE_DESCS = (
    TYPE_DESC,
    "_typelib_CompoundTypeDescription",
    "_typelib_StructTypeDescription",
    "_typelib_IndirectTypeDescription",
    "_typelib_EnumTypeDescription",
    "_typelib_InterfaceMemberTypeDescription",
    "_typelib_InterfaceMethodTypeDescription",
    "_typelib_InterfaceAttributeTypeDescription",
    "_typelib_InterfaceTypeDescription",
)
TYPE_DESC_REF = "_typelib_TypeDescriptionReference"

# signature, Type(typecclass, tag)
# tag = UNO name of type
# typename = C++ name of the class
# typeclass value of TypeClass


def unoToCpp(uno: str) -> str:
    return uno.replace(".", "::")[1:-1]


class TypeEntry:
    def __init__(
        self,
        type_class: int,
        uno_type: str,
        cpp_type: str,
        element_type: object | None = None,
    ):
        self.type_class = type_class
        self.uno_type = uno_type
        self.cpp_type = cpp_type
        self.element_type = element_type


unresolved_type_cache: Set[int] = set()
resolved_type_cache: Dict[int, TypeEntry] = {}


def resolve_uno_type(valobj: lldb.SBValue) -> TypeEntry | None:
    global uno_type_cache
    global cpp_type_cache
    global unresolved_type_cache
    address = valobj.GetLoadAddress()
    if address in unresolved_type_cache:
        return None

    if address in resolved_type_cache:
        return resolved_type_cache[address]

    type = valobj.GetType().GetCanonicalType()

    val = valobj
    if type.name == CSSU_TYPE:
        pvalue = valobj.GetChildMemberWithName("_pType")
        unresolved_type_cache.add(address)
        if not pvalue:
            print("Could not retrieve CSSU._pType")
            return None
        val = pvalue.Dereference()
        type = val.GetType().GetCanonicalType()

    while type.name == TYPE_DESC_REF:
        pvalue = valobj.GetChildMemberWithName("pType")
        if not pvalue:
            print("Could not retrieve TypeDesc.pType")
            return None
        val = pvalue.Dereference()
        type = val.GetType().GetCanonicalType()

    if type.GetName() not in TYPE_DESCS:
        unresolved_type_cache.add(address)
        return None

    full_val: lldb.SBValue = val
    if type.GetName() != TYPE_DESC:
        while val.GetChildMemberWithName("aBase"):
            val = val.GetChildMemberWithName("aBase")
    type_class = val.GetChildMemberWithName("eTypeClass").GetValueAsUnsigned()
    name = val.GetChildMemberWithName("pTypeName").value

    if type_class in PRIMITIVE_TO_CPP:
        entry = TypeEntry(type_class, name, PRIMITIVE_TO_CPP[type_class])
        resolved_type_cache[address] = entry
        return entry
    elif type_class in UNO_TO_CPP:
        entry = TypeEntry(type_class, name, unoToCpp(name))
        resolved_type_cache[address] = entry
        return entry
    elif (
        type_class == TypeClass.INTERFACE_ATTRIBUTE
        or type_class == TypeClass.INTERFACE_METHOD
    ):
        (interface, _delim, member) = name.partition("::")
        entry = TypeEntry(type_class, name, unoToCpp(interface) + "::*" + member)
        resolved_type_cache[address] = entry
        return entry
    elif type_class == TypeClass.SEQUENCE:
        target: lldb.SBTarget = full_val.GetTarget()
        seq_type_desc = target.FindFirstType("_typelib_IndirectTypeDescription")
        if not seq_type_desc:
            print("Could not resolve IndirectTypeDescription")
            unresolved_type_cache.add(address)
            return None
        pElem: lldb.SBValue = full_val.Cast(seq_type_desc).GetChildMemberWithName(
            "pType"
        )
        if not pElem:
            print("Could not resolve SequenceElementType")
            unresolved_type_cache.add(address)
            return None
        elem = resolve_uno_type(pElem.Dereference())
        if not elem:
            print("Could not resolve Element")
            unresolved_type_cache.add(address)
            return None
        else:
            entry = TypeEntry(
                type_class,
                name,
                f"com::sun::star::uno::Sequence<{elem.cpp_type}>",
                elem,
            )
            resolved_type_cache[address] = entry
            return entry

    unresolved_type_cache.add(address)
    return None


def uno_any_summary(valobj: lldb.SBValue) -> str:
    typename = cleanup_name(valobj.GetType().GetName())
    type_desc = valobj.GetChildMemberWithName("pType")
    if not type_desc.IsValid():
        return f"{typename}(invalid)"
    type = resolve_uno_type(type_desc.Dereference())
    if not type:
        return f"{typename}(invalid)"
    if (
        type_desc.Dereference()
        .GetChildMemberWithName("eTypeClass")
        .GetValueAsUnsigned()
        == TypeClass.VOID
    ):
        return f"{typename}({type.uno_type})"
    else:
        ptr: lldb.SBValue = valobj.GetChildMemberWithName("pData")
        if not ptr.IsValid():
            return f"{typename}(invalid)"
        target = ptr.GetTarget()
        res_type = target.FindFirstType(type.cpp_type)
        if not res_type or not res_type.IsValid():
            return f"{typename}(unresolved)"
        return f"{typename}({type.uno_type}: {ptr.Cast(res_type.GetPointerType()).Dereference()})"


def uno_reference_summary(valobj: lldb.SBValue) -> str:
    typename = cleanup_name(valobj.GetType().GetName())
    iface = valobj.GetChildMemberWithName("_pInterface")
    if iface and iface.IsValid():
        iface.SetPreferDynamicValue(lldb.eDynamicCanRunTarget)
        try:
            return f"{typename} to ({iface.GetTypeName()}) {iface}"
        except:
            return f"{typename} to (XInterface) {iface}"
    else:
        return f"empty {typename}"


class UnoReferenceSynthProvider:
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
            iface = self.valobj.GetChildMemberWithName("_pInterface")
            if iface and iface.IsValid():
                iface.SetPreferDynamicValue(lldb.eDynamicCanRunTarget)
                return iface.Dereference()
            return None
        return None


def uno_sequence_summary(valobj: lldb.SBValue) -> str:
    typename = cleanup_name(valobj.GetType().GetName())
    ptr: lldb.SBValue = valobj.GetChildMemberWithName("_pSequence")
    if ptr and ptr.IsValid():
        impl: lldb.SBValue = ptr.Dereference()
        size = impl.GetChildMemberWithName("nElements").GetValueAsUnsigned(0)
        return f"{typename} [{size}]"
    else:
        return f"uninitialized {typename}"


class UnoSequenceSynthProvider:
    def __init__(self, valobj: lldb.SBValue):
        self.valobj = valobj
        self.update()  # initialize this provider

    def num_children(self):
        return self.length

    def get_child_index(self, name: str):
        try:
            return int(name.lstrip("[").rstrip("]"))
        except:
            return -1

    def get_child_at_index(self, index: int):
        if index < 0 or index >= self.num_children():
            return None
        offset = index * self.type_size
        return self.data.CreateChildAtOffset(
            "[" + str(index) + "]", offset, self.data_type
        )

    def update(self):
        self.ptr = self.valobj.GetChildMemberWithName("_pSequence")
        self.length = 0
        if not self.ptr:
            return
        self.data_type = self.valobj.GetType().GetTemplateArgumentType(0)
        self.type_size = self.data_type.GetByteSize()
        impl = self.ptr.Dereference()
        self.data = impl.GetChildMemberWithName("elements").Cast(
            self.data_type.GetPointerType()
        )
        self.length = impl.GetChildMemberWithName("nElements").GetValueAsUnsigned(0)
        assert self.type_size != 0


def uno_type_summary(valobj: lldb.SBValue) -> str:
    typename = cleanup_name(valobj.GetType().GetName())
    uno = resolve_uno_type(valobj)
    if uno:
        return f"{typename} {uno.uno_type}"
    else:
        return f"invalid {typename}"


def register_formatters(debugger: lldb.SBDebugger):
    category = Category(debugger, "uno", __name__)
    category.addSummary("_uno_Any", uno_any_summary)
    category.addSummary("com::sun::star::uno::Any", uno_any_summary)
    category.addSummary(
        "^com::sun::star::uno::Reference<.+>$",
        uno_reference_summary,
        is_regex=True,
        expand_children=True,
    )
    category.addSynthetic(
        "^com::sun::star::uno::Reference<.+>$",
        UnoReferenceSynthProvider,
        is_regex=True,
    )
    category.addSummary(
        "^com::sun::star::uno::Sequence<.+>$",
        uno_sequence_summary,
        is_regex=True,
        expand_children=True,
    )
    category.addSynthetic(
        "^com::sun::star::uno::Sequence<.+>$",
        UnoSequenceSynthProvider,
        is_regex=True,
    )
    category.addSummary("com::sun::star::uno::Type", uno_type_summary)
