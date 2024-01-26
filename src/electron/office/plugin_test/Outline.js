function getCurrentParagraphText(xTxtDoc) {
    // the current selection should be 'hello world'
  const xController = xTxtDoc.getCurrentController();
  assert(xController != null);
  const xSupplier = xController.as('text.XTextViewCursorSupplier');
  assert(xSupplier != null);
  const xViewCursor = xSupplier.getViewCursor();
  assert(xViewCursor != null);
  const xTxtCursor = xViewCursor.getText().createTextCursor();
  xTxtCursor.gotoRange(xViewCursor.getStart(), false);
  xTxtCursor.as('text.XParagraphCursor').gotoEndOfParagraph(true);
  return xTxtCursor.getString();
}

async function testOutline() {
  const x = await loadEmptyDoc();
  assert(x != null);

  await x.initializeForRendering();
  getEmbed().renderDocument(x);
  await ready(x);
  const html = `<!DOCTYPE html>
<html>
<body lang="en-US" link="#000080" vlink="#800000" dir="ltr"><h1 class="western">
Header 1</h1>
<p>Text</p>
<h2 class="western">Header 2</h2>
<p>Text</p>
<h1 class="western">Header 1 #2</h1>
<p>Text</p>
</body>
</html>`;
  const outlineSnapshot = {
    outline: [
      { id: 0, parent: -1, text: 'Header 1' },
      { id: 1, parent: 0, text: 'Header 2' },
      { id: 2, parent: -1, text: 'Header 1 #2' },
    ],
  };
  x.paste('text/html', html);

  const outline = await x.getCommandValues('.uno:GetOutline');
  assert(outline);
  assert(JSON.stringify(outline) === JSON.stringify(outlineSnapshot));

  assert(x.gotoOutline(1) != null);

  const xTxtDoc = x.as('text.XTextDocument');
  assert(getCurrentParagraphText(xTxtDoc) == outlineSnapshot.outline[1].text);

  assert(x.gotoOutline(2) != null);
  assert(getCurrentParagraphText(xTxtDoc) == outlineSnapshot.outline[2].text);

  assert(x.gotoOutline(0) != null);
  assert(getCurrentParagraphText(xTxtDoc) == outlineSnapshot.outline[0].text);

  assert(x.gotoOutline(5) == null);
  assert(x.gotoOutline(-5) == null);
}

testOutline();
