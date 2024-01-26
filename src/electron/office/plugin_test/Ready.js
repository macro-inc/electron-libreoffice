async function testReady() {
  const x = await loadEmptyDoc();
  assert(x != null);

  assert(!x.isReady);
  await x.initializeForRendering();
  getEmbed().renderDocument(x);
  await ready(x);
  assert(x.isReady);
}

testReady();
