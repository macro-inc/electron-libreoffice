import lldb
import glob
import os.path

# This shouldn't be necessary in most cases!

OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'src', 'out', 'Default')

def add_symbols(debugger):
    files = glob.glob(os.path.join(OUT_DIR, '*'))
    file = glob.glob(os.path.join(OUT_DIR, '*.dSYM'))
    wanted_binaries = set([
        m.GetFileSpec().GetFilename()
        for m in debugger.GetSelectedTarget().module_iter()
    ])
    wanted_dsyms = set([
        m.GetFileSpec().GetFilename() + ".dSYM"
        for m in debugger.GetSelectedTarget().module_iter()
    ])
    for file in file:
        if os.path.basename(file) in wanted_dsyms:
            debugger.HandleCommand('target symbols add "{}"'.format(file))
    for file in file:
        if os.path.basename(file) in wanted_binaries:
            debugger.HandleCommand('target symbols add "{}"'.format(file))

def __lldb_init_module(debugger, internal_dict):
    add_symbols(debugger)

