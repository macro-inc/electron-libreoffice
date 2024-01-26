async function testRestorable() {
  const x = await loadEmptyDoc();
  assert(x != null);

  await x.initializeForRendering();

  const restoreKey = getEmbed().renderDocument(x, {
    restoreKey: undefined
  });
  await ready(x);

  sendKeyEvent(KeyEventType.Press, 'a');
  await idle();
  await painted();

  remountEmbed();
  await idle();
  const finalRestoreKey = getEmbed().renderDocument(x, {
    restoreKey
  });
  assert(finalRestoreKey !== restoreKey);
  await painted();

  sendKeyEvent(KeyEventType.Press, 'b');
  await idle();
  await painted();
}

testRestorable();
