async function testPostUnoCommand() {
  const docClient = await libreoffice.loadDocument('private:factory/swriter');
  docClient.postUnoCommand('.uno:Bold');

  const xTextDoc = docClient.as('text.XTextDocument');
  const xText = xTextDoc.getText();
  xText.setString("hello world");

  const xTextCursor = xText.createTextCursor();
  xTextCursor.gotoStart(false);
  xTextCursor.gotoEnd(true);

  const xCursorProp = xTextCursor.as('beans.XPropertySet');

  assert(xCursorProp.getPropertyValue("CharWeight") === 150); // 150% is bold
}

testPostUnoCommand();
