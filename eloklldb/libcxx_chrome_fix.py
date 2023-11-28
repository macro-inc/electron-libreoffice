# Copyright (c) 2022 Macro
# Use of this source code is governed by the MIT license that can be
# found in the LICENSE file.
# -*- coding: utf-8 -*-

"""\
This is a bit of nightmare!
Chrome uses a non-standard namespace std::Cr, where the standard picked up by
LLDB is std::__[0-9a-zA-Z]+. So we do our best to copy what we can find and
adapt.
"""
import lldb
import time

tries_after_breakpoint = 0


def breakpoint_callback(
    frame: lldb.SBFrame, bp_loc: lldb.SBBreakpointLocation, _dict
) -> bool:
    global tries_after_breakpoint
    # It might fire once, but never trust LLDB
    breakpoint = bp_loc.GetBreakpoint()
    target = breakpoint.GetTarget()
    target.BreakpointDelete(breakpoint.id)

    tries_after_breakpoint += 1
    debugger: lldb.SBDebugger = target.GetDebugger()
    frame.get_locals()
    frame.get_arguments()
    duplicate_and_modify_formatters(debugger, tries_after_breakpoint)
    time.sleep(0.5)
    # Access an instance variable to try force loading C++ plugin
    process = target.GetProcess()
    if process.GetState() == lldb.eStateStopped:
        process.Continue()

    return True  # False means it won't actually stop


def attempt_after_breakpoint(debugger: lldb.SBDebugger):
    """This should force the cplusplus plugin to load"""
    target: lldb.SBTarget = debugger.GetSelectedTarget()
    process: lldb.SBProcess = target.GetProcess()
    stopped = False
    if process.GetState() == lldb.eStateRunning:
        stopped = True
        process.Stop()

    # Break inside of the main loop, since it's frequent
    breakpoint: lldb.SBBreakpoint = target.BreakpointCreateByLocation(
        "message_pump_default.cc", 41
    )
    breakpoint.SetIgnoreCount(1)  # Don't break immediately, let the symbols load
    breakpoint.SetOneShot(True)  # -o true
    breakpoint.SetThreadName("electron")  # -T electron
    breakpoint.SetScriptCallbackFunction("libcxx_chrome_fix.breakpoint_callback")
    if stopped:
        process.Continue()


def crfix(
    debugger: lldb.SBDebugger,
    _command: str,
    _result: lldb.SBCommandReturnObject,
    _internal_dict: dict,
):
    duplicate_and_modify_formatters(debugger, 10)


def fallback_to_command(debugger: lldb.SBDebugger):
    debugger.HandleCommand(f"command script add -f {__name__}.crfix crfix")


def fix_name(name: str) -> str:
    return name.replace("std::__1", "std::Cr").replace("std::__[[:alnum:]]+", "std::Cr")


def duplicate_and_modify_formatters(debugger: lldb.SBDebugger, depth=0):
    """Takes the original C++ formatters and makes them work with std::Cr"""
    # This is the category with all the goodies inside, libcxx libstdcpp all that
    original_category: lldb.SBTypeCategory = debugger.GetCategory("cplusplus")

    if not original_category:
        if depth == 0:
            print("After stopping at a breakpoint, run this command: crfix")
            fallback_to_command(debugger)
        # elif depth == 0:
        #     print("Could not get C++ types, attempting after breakpoint")
        #     attempt_after_breakpoint(debugger)
        elif depth > 3:
            print("Command failed! Try using it after stopping at a breakpoint")
        else:
            print("Trying again...")
            attempt_after_breakpoint(debugger)
        return

    # Create or get the 'cpplusplus_chrome' category
    new_category = debugger.GetCategory("cpplusplus_chrome") or debugger.CreateCategory(
        "cpplusplus_chrome"
    )

    # Make mutated copies of summary formatters
    for i in range(original_category.GetNumSummaries()):
        summary = original_category.GetSummaryAtIndex(i)
        specifier = original_category.GetTypeNameSpecifierForSummaryAtIndex(i)
        if not specifier:
            continue
        name = str(specifier.name)
        if not name:
            continue
        modified_typename = fix_name(name)

        # didn't replace anything, don't want to accidentally override that
        if name == modified_typename:
            continue

        # Create a new typename specifier with the modified name
        new_typename_specifier = lldb.SBTypeNameSpecifier(
            modified_typename, specifier.IsRegex()
        )

        new_category.AddTypeSummary(new_typename_specifier, summary)

    # Make mutated copies of synthetic formatters
    for i in range(original_category.GetNumSynthetics()):
        synthetic: lldb.SBTypeSynthetic = original_category.GetSyntheticAtIndex(i)
        specifier: lldb.SBTypeNameSpecifier = (
            original_category.GetTypeNameSpecifierForSyntheticAtIndex(i)
        )
        if not specifier:
            continue
        name = str(specifier.name)
        if not name:
            continue

        modified_typename = fix_name(name)

        # didn't replace anything, don't want to accidentally override that
        if name == modified_typename:
            continue

        # Create a new typename specifier with the modified name
        new_typename_specifier = lldb.SBTypeNameSpecifier(
            modified_typename, specifier.IsRegex()
        )

        new_category.AddTypeSynthetic(new_typename_specifier, synthetic)

    new_category.AddLanguage(lldb.eLanguageTypeC_plus_plus)
    new_category.SetEnabled(True)
    print("Finished fixing formatters")


def __lldb_init_module(debugger: lldb.SBDebugger, internal_dict):
    print("Please wait while we fix C++ formatting")
    duplicate_and_modify_formatters(debugger)
