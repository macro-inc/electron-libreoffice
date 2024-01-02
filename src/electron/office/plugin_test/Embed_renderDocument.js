async function testRenderDocument() {
  const x = await loadEmptyDoc();
  assert(x != null);

  await x.initializeForRendering();

  let resolveReady;
  let called = false;
  const ready = new Promise((resolve) => {
    resolveReady = resolve;
  });
  x.on('ready', () => {
    resolveReady();
    called = true;
  });

  const restoreKey = getEmbed().renderDocument(x);
  assert(typeof restoreKey === 'string' && restoreKey.length > 0);
  await ready;
  assert(called === true);
}

testRenderDocument();
