async function testDocumentSizeChange() {
  const x = await loadEmptyDoc();
  assert(x != null);

  await x.initializeForRendering();

  let sizeChangedResolve;
  const sizeChangedPromise = new Promise((resolve) => {
    sizeChangedResolve = resolve;
  });
  let sizeChanged = false;
  x.on('document_size_changed', () => {
    sizeChanged = true;
    sizeChangedResolve();
  });

  getEmbed().renderDocument(x);
  await ready(x);

  // insert page breaks
  sendKeyEvent(KeyEventType.Press, 'mod+enter');
  sendKeyEvent(KeyEventType.Press, 'mod+enter');
  await idle();
  await sizeChangedPromise;
  assert(sizeChanged);
}

testDocumentSizeChange();
