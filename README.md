## What is this?
This project embeds LibreOfficeKit into Electron as a Blink WebPlugin

## How do I set this up locally?
Install the [Electron prerequisites](https://www.electronjs.org/docs/latest/development/build-instructions-gn#platform-prerequisites) for your operating system

Sync the code (including the git cache, uses ~42 GiB of space):

``` bash
git clone https://github.com/coparse-inc/electron-libreoffice
cd electron-libreoffice
scripts/e sync
```

## How do I build it?

For local testing: `scripts/e build`

For release builds: `IS_RELEASE=true scripts/e build`

For compiling for Apple Silicon: `FOR_APPLE_SILICON=true scripts/e build`

## How do I run this build?

``` bash
# To run with the included manual QA page, use:
scripts/e run

# To run with another file use scripts/e run ..., just as you would use `electron` normally
# For example:
scripts/e run ~/my-electron-libreoffice-app/index.html
```

This basically runs `src/out/Default/electron` with the `--no-sandbox` flag.

`--no-sandbox` is currently required for LibreOfficeKit, but hopefully in the future this won't be necessary.

## How do I debug it?

```bash
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
