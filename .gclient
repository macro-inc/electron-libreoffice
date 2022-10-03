solutions = [
  { "name"        : '.',
    "url"         : 'https://github.com/coparse-inc/electron-libreoffice',
    "deps_file"   : 'src/electron/DEPS',
    "managed"     : False,
    "custom_deps" : {
      "src/electron": None,
      "src/third_party/angle/third_party/VK_GL-CTS/src": None,
      "src/ios/chrome": None,
      "src/android_webview": None,
      "src/third_party/android_rust_toolchain": None,
      "src/chrome/test/data/xr/webvr_info": None,
    },
    "custom_vars": {},
  },
]
