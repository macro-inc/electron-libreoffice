async function testPostUnoCommand() {
  let docClient = await libreoffice.loadDocument('private:factory/swriter');
  docClient.postUnoCommand('.uno:Bold');

  let xTextDoc = docClient.as('text.XTextDocument');
  let xText = xTextDoc.getText();
  xText.setString("hello world");

  let xTextCursor = xText.createTextCursor();
  xTextCursor.gotoStart(false);
  xTextCursor.gotoEnd(true);

  let xCursorProp = xTextCursor.as('beans.XPropertySet');

  assert(xCursorProp.getPropertyValue("CharWeight") === 150); // 150% is bold

  // test with undefined as a param, which should be the same behavior
  docClient = await libreoffice.loadDocument('private:factory/swriter');
  docClient.postUnoCommand('.uno:Bold', undefined);

  xTextDoc = docClient.as('text.XTextDocument');
  xText = xTextDoc.getText();
  xText.setString("hello world");

  xTextCursor = xText.createTextCursor();
  xTextCursor.gotoStart(false);
  xTextCursor.gotoEnd(true);

  xCursorProp = xTextCursor.as('beans.XPropertySet');

  assert(xCursorProp.getPropertyValue("CharWeight") === 150); // 150% is bold

  // test with non-JSON-compatible, which should be the same behavior
  docClient = await libreoffice.loadDocument('private:factory/swriter');
  docClient.postUnoCommand('.uno:Bold', new ArrayBuffer(0));

  xTextDoc = docClient.as('text.XTextDocument');
  xText = xTextDoc.getText();
  xText.setString("hello world");

  xTextCursor = xText.createTextCursor();
  xTextCursor.gotoStart(false);
  xTextCursor.gotoEnd(true);

  xCursorProp = xTextCursor.as('beans.XPropertySet');

  assert(xCursorProp.getPropertyValue("CharWeight") === 150); // 150% is bold
}

testPostUnoCommand();
