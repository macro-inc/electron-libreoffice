import lldb
from .lok import uno, string, cppu

def __lldb_init_module(debugger: lldb.SBDebugger, _internal_dict):
    print("Loading LOK formatters...")
    for formatter in {uno, string, cppu}:
        formatter.register_formatters(debugger)
