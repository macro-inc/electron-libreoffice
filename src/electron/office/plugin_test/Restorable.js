async function testRestorable() {
  const x = await loadEmptyDoc();
  assert(x != null);

  await x.initializeForRendering();

  const restoreKey = getEmbed().renderDocument(x, {
    restoreKey: undefined
  });
  log('restore key' + restoreKey);
  await ready(x);

  const firstInvalidate = invalidate(x);
  sendKeyEvent(KeyEventType.Press, 'a');
  await idle();
  await firstInvalidate;

  const nextRestoreKey = getEmbed().renderDocument(x, {
    restoreKey
  });
  // since the plugin wasn't destroyed, the key should be the same
  assert(restoreKey  === nextRestoreKey);
  log('next restore key' + nextRestoreKey);

  const secondInvalidate = invalidate(x);
  sendKeyEvent(KeyEventType.Press, 'b');
  await idle();
  await secondInvalidate;
}

testRestorable();
