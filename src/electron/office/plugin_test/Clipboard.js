async function testClipboard() {
  const x = await loadEmptyDoc();
  assert(x != null);

  let resolveClipChanged;
  let clipChanged = false;
  const clipChangePromise = new Promise((resolve) => {
    resolveClipChanged = resolve;
  });
  x.on('clipboard_changed', (response) => {
    if (!response?.payload?.sw) {
      return;
    }
    log(JSON.stringify(response));
    resolveClipChanged();
    clipChanged = true;
  });

  await x.initializeForRendering();
  getEmbed().renderDocument(x);
  await ready(x);

  const testString = 'hello world';

  // prepare formatted text
  x.postUnoCommand('.uno:Bold');
  const xTextDoc = x.as('text.XTextDocument');
  const xText = xTextDoc.getText();
  xText.setString(testString);
  const rects = getEmbed().pageRects;
  assert(rects != null);
  assert(rects.length === 1);

  // select all
  sendKeyEvent(KeyEventType.Press, 'mod+a');
  await idle();

  // the current selection should be 'hello world'
  const xController = xTextDoc.getCurrentController();
  assert(xController != null);
  const xSupplier = xController.as('text.XTextViewCursorSupplier');
  assert(xSupplier != null);
  const xViewCursor = xSupplier.getViewCursor();
  assert(xViewCursor != null);
  assert(xViewCursor.getString() === testString);

  // clipboard event should fire
  sendKeyEvent(KeyEventType.Press, 'mod+c');
  await idle();
  await clipChangePromise;
  assert(clipChanged);

  // clipboard should contain the copied text
  const content = x.getClipboard(['text/plain', 'text/html']);
  assert(content.length === 2);
  assert(
    content[0].mimeType === 'text/plain' && content[0].text === testString
  );
  assert(
    content[1].mimeType === 'text/html' && content[1].text.includes(testString)
  );

  // clear the document
  sendKeyEvent(KeyEventType.Press, 'mod+a');
  sendKeyEvent(KeyEventType.Press, 'backspace');
  await idle();

  // smallest valid png
  const testPng = new Uint8Array([
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
    0x0a, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x00, 0x01, 0x00, 0x00,
    0x05, 0x00, 0x01, 0x0d, 0x0a, 0x2d, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x49,
    0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
  ]);
  x.setClipboard([{
    mimeType: 'image/png',
    buffer: testPng.buffer
  }]);

  // clipboard should contain the png
  const pngContent = x.getClipboard(['image/png']);
  assert(pngContent.length === 1);
  assert(pngContent[0].mimeType === 'image/png');
  /** @type ArrayBuffer */
  const buf = pngContent[0].buffer;
  assert(buf.byteLength === testPng.byteLength);
  const bufArray = new Uint8Array(buf);
  assert(bufArray.every((b, idx) => b == testPng[idx]));

  // pasting to the document and copying should result in the same
  x.setClipboard([{
    mimeType: 'image/png',
    buffer: testPng.buffer
  }]);
  sendKeyEvent(KeyEventType.Press, 'mod+v');
  sendKeyEvent(KeyEventType.Press, 'mod+a');
  sendKeyEvent(KeyEventType.Press, 'mod+c');
  await idle();
  const pngContent2 = x.getClipboard(['image/png']);
  assert(pngContent2.length === 1);
  assert(pngContent2[0].mimeType === 'image/png');
  /** @type ArrayBuffer */
  const buf2 = pngContent2[0].buffer;
  assert(buf2.byteLength === testPng.byteLength);
  const bufArray2 = new Uint8Array(buf2);
  assert(bufArray2.every((b, idx) => b == testPng[idx]));
}

testClipboard();
