const { app, BrowserWindow } = require('electron');

let win = null;

app.whenReady().then(() => {
  win = new BrowserWindow({
    webPreferences: {
      sandbox: false // required for LOK
    }
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
  app.quit()
})
