# Macro Open Source Project: Electron-LibreOffice

This is a public project that forks Electron to embed LibreOffice directly as an HTML element with a TypeScript API. 

## Setup

Setup [the prerequisites](PREREQUISITES.md)

Sync the code (including the git cache, uses ~42 GiB of space):

```shell
git clone https://github.com/coparse-inc/electron-libreoffice
cd electron-libreoffice
scripts/e sync
```

Run before syncing on Windows:
```powershell
cmd /c call .\depot_tools\cipd_bin_setup.bat
cmd /c call .\depot_tools\bootstrap\win_tools.bat
```

## Building

You must sync the code first (as stated above) and let it finish first.

For local debugging: `scripts/e build`

For release builds: `IS_RELEASE=true scripts/e build`

## Running

```shell
# To build and run with the included manual QA page (qa/index.html), use:
scripts/e run

# To build and run with another file use scripts/e run ..., just as you would use `electron` normally
# For example:
scripts/e run ~/my-electron-libreoffice-app/index.html
```

This basically runs `src/out/Default/electron` with the `--no-sandbox` flag.

`--no-sandbox` is currently required for LibreOfficeKit, but hopefully in the future this won't be necessary.

## Using a local build of [`libreofficekit`](https://github.com/coparse-inc/libreofficekit)?

This assumes you have already synced.

Do this once:

```shell
# move the old source for libreofficekit to libreofficekit.old
mv src/third_party/libreofficekit{,.old}
```

**Linux/macOS**
```bash
# link src/third_party/libreofficekit to your local libreofficekit/libreoffice-core
ln -s ../libreofficekit/libreoffice-core src/third_party/libreofficekit
```

**Windows**
```bash
# note you must use an administrator command prompt, and the source and target are flipped
mklink /D ".\src\third_party\libreofficekit" "..\libreofficekit\libreoffice-core"
```

When you make a new local build of `libreofficekit`, make a new Electron build with `UPDATE_LOK=true`:
```shell
UPDATE_LOK=true scripts/e build
```

## Using a debug build in another project
```shell
# copy zipped runtime files to ~/another_project
INJECT_TO="~/another_project" scripts/e build

# this copies a fresh build of dist.zip, mksnapshot.zip, mksnapshot_args, and gen/v8/electron.S
# dist.zip can be expanded into node_modules/electron/dist
# mksnapshot.zip can be expanded into node_modules/electron-mksnapshot/bin
# all other files can be copied into node_modules/electron-mksnapshot/bin
```

## Debugging

```shell
# Run the QA testing ground
scripts/e run --renderer-startup-dialog qa/index.html

# On Linux/Mac, it will report the PID of the renderer,
# then you can start GDB with that PID. Assuming the renderer's PID is 1163:
gdb -p 1163 -ex 'signal SIGUSR1'

# On Windows, you can setup Visual Studio to support the project by running
scripts/e vs_devenv

# When that's finished, you can use the Visual Studio debugger by opening `Debug > Attach to Process...`,
# then selecting the PID that's displayed in the dialog box opened by Electron.
# Press OK in the dialog box after the process is attached.

```

## Pulling upstream changes from Electron

Run `scripts/pull-upstream-changes.sh`

## Testing and coverage

There are two kinds of tests:
- C++
  - Use [GTest](https://google.github.io/googletest/primer.html)
  - Located in `src/electron/office/*test.cc`
  - Unit tests for `something.cc` are located in the same location and named `something_unittest.cc`
- JavaScript
  - Use a simple test harness using `assert(condition: boolean)` for tests
  - Located in `src/electron/office/js_test/*.js`

``` bash
# run all tests
scripts/e test

# run tests and get test coverage
scripts/e coverage

# get usage for test command
scripts/e test --help

# list all available tests
scripts/e test --gtest_list_tests

# only run tests starting with Foo
scripts/e test --gtest_filter=Foo*
```
