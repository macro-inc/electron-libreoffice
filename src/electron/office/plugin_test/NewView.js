async function testNewView() {
  const x = await loadEmptyDoc();
  assert(x != null);

  await x.initializeForRendering();
  getEmbed().renderDocument(x);
  await ready(x);

  const testString = 'hello world';
  const newView = x.newView();
  newView.paste('text/plain;charset=utf-8', testString);

  assert(x.as('text.XTextDocument').getText().getString() === testString);
}

testNewView()
