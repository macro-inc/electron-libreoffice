async function testKeyEvents() {
  const x = await loadEmptyDoc();
  assert(x != null);
  await x.initializeForRendering();
  getEmbed().renderDocument(x);
  await ready(x);

  updateFocus(true);
  sendKeyEvent(KeyEventType.Press, 'mod+b');
  await idle();
  sendKeyEvent(KeyEventType.Press, 'a');
  sendKeyEvent(KeyEventType.Press, 'b');
  sendKeyEvent(KeyEventType.Press, 'c');
  await idle();

  // check that the text matches the normal key presses
  const xTextDoc = x.as('text.XTextDocument');
  const xText = xTextDoc.getText();
  assert(xText.getString() === 'abc');

  // check that the text is bold
  const xTextCursor = xText.createTextCursor();
  xTextCursor.gotoStart(false);
  xTextCursor.gotoEnd(true);

  const xCursorProp = xTextCursor.as('beans.XPropertySet');

  assert(xCursorProp.getPropertyValue("CharWeight") === 150); // 150% is bold
}

testKeyEvents();
