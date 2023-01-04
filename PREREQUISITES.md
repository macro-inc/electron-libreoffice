- Node.js v16 (use [fnm](https://github.com/Schniz/fnm) or [nvm](https://github.com/nvm-sh/nvm))
  - _v18+ will cause the build to fail_

---

_macOS_:

- macOS 12.0 or newer
- [Xcode 13.4.1](https://developer.apple.com/services-account/download?path=/Developer_Tools/Xcode_13.4.1/Xcode_13.4.1.xip)
  - Before you run any commands, use `export DEVELOPER_DIR=/Applications/Xcode13.4.1.app/Contents/Developer` where `/Applications/Xcode13.4.1.app` is the path to the extracted Xcode

_Ubuntu/popOS_

```shell
sudo apt-get install build-essential clang libdbus-1-dev libgtk-3-dev \
  libnotify-dev libasound2-dev libcap-dev libcups2-dev libxtst-dev \
  libxss1 libnss3-dev gcc-multilib g++-multilib curl gperf bison \
  python3-dbusmock openjdk-8-jre
```

_Windows_:

- Windows 10 / Server 2012 R2 or higher
- Visual Studio 2019 / Windows 11 SDK 10.0.22621.0 / Windows 10 SDK 10.015063.4368 / Windows 10 SDK 10.0.20348.0
  - Can be quickly installed by running this in an Admin PowerShell `Windows Key+X > Windows PowerShell (admin)`:
    ``` powershell
    Set-ExecutionPolicy Unrestricted
    .\scripts\toolchain\install.ps1
    Set-ExecutionPolicy Restricted
    ```

- [Git](https://git-scm.com/download/win)

- Exclude source tree from Windows Security
  - Windows Security doesn't like one of the files in the Chromium source code (see https://crbug.com/441184), so it will constantly delete it, causing `gclient sync` issues. You can exclude the source tree from being monitored by Windows Security by [following these instructions](https://support.microsoft.com/en-us/windows/add-an-exclusion-to-windows-security-811816c0-4dfd-af4a-47e4-c301afe13b26).
