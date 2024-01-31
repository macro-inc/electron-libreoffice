const { app, BrowserWindow, session } = require('electron');

let win = null;

app.whenReady().then(() => {
  session.defaultSession.webRequest.onHeadersReceived((details, callback) => {
    const headers = details.responseHeaders;

    // Adjust the CSP to allow framing from specific origins
    const cspHeader = headers['content-security-policy'];
    if (cspHeader) {
      const updatedCsp = cspHeader.map((csp) => {
        return csp.replace(
          "frame-ancestors 'none';",
          ''
        );
      });
      headers['content-security-policy'] = updatedCsp;
      headers['x-frame-options'] = '';
    }

    callback({ responseHeaders: headers });
  });

  win = new BrowserWindow({
    webPreferences: {
      sandbox: false, // required for LOK
    },
  });
  win.webContents.setWindowOpenHandler((details) => {
    const allow = {
      action: 'allow',
      overrideBrowserWindowOptions: {
        frame: false,
      },
    };

    console.log(JSON.stringify(details));
    if (details.frameName === 'bug_dialog') {
      return allow;
    } else if (
      details.url === 'https://github.com/coparse-inc/electron-libreoffice'
    ) {
      return {
        action: 'allow',
      };
    }

    return { action: 'deny' };
  });

  win.loadFile('index.html');
  win.webContents.session.setPermissionCheckHandler(
    (webContents, permission, requestingOrigin, details) => {
      if (permission === 'clipboard-read') {
        // Add logic here to determine if permission should be given to allow HID selection
        return true;
      }
      return false;
    }
  );
});

app.on('window-all-closed', () => {
  app.quit();
});
