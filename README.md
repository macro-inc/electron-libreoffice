## What is this?

This project embeds LibreOfficeKit into Electron as a Blink WebPlugin

## How do I set this up locally?

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

## How do I build it?

You must sync the code first (as stated above) and let it finish first.

For local testing: `scripts/e build`

For release builds: `IS_RELEASE=true scripts/e build`

## How do I run this build?

```shell
# To build and run with the included manual QA page (qa/index.html), use:
scripts/e run

# To build and run with another file use scripts/e run ..., just as you would use `electron` normally
# For example:
scripts/e run ~/my-electron-libreoffice-app/index.html
```

This basically runs `src/out/Default/electron` with the `--no-sandbox` flag.

`--no-sandbox` is currently required for LibreOfficeKit, but hopefully in the future this won't be necessary.

## How do I debug it?

```shell
# Build with debug symbols (uses an additional ~20 GiB!) and run the QA testing ground
IS_DEBUG=true scripts/e run --renderer-startup-dialog qa/index.html


# On Linux/Mac, it will report the PID of the renderer,
# then you can start GDB with that PID. Assuming the renderer's PID is 1163:
gdb -p 1163 -ex 'signal SIGUSR1'

# On Windows, you can setup Visual Studio to support the project by running
scripts/e vs_devenv

# When that's finished, you can use the Visual Studio debugger by opening `Debug > Attach to Process...`,
# then selecting the PID that's displayed in the dialog box opened by Electron.
# Press OK in the dialog box after the process is attached.

```

## How do I pull the upstream changes from Electron?

Run `scripts/pull-upstream-changes.sh`

## How do I use a local build of [`libreofficekit`](https://github.com/coparse-inc/libreofficekit)?

This assumes you have already synced.

Do this once:

```shell
# move the old source for libreofficekit to libreofficekit.old
mv src/third_party/libreofficekit{,.old}
```

Linux/MacOs
```bash
# link src/third_party/libreofficekit to your local libreofficekit/libreoffice-core
ln -s ../libreofficekit/libreoffice-core src/third_party/libreofficekit
```

Windows
```bash
# note you must use an administrator command prompt, and the source and target are flipped
mklink /D ".\src\third_party\libreofficekit" "..\libreofficekit\libreoffice-core"
```


When you make a new local build of `libreofficekit`, remove the old build in electron-libreoffice's output:
```shell
rm -rf src/out/Default/libreofficekit
```

Then make a new build as stated in [How do I build it?](#how-do-i-build-it)

# Fixing CI Failing

- In the event the mac CI fails you will need to manually run "Clean Mac Runner" job available in the actions tab of this github repo
