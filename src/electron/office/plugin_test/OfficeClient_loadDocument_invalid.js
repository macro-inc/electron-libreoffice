// this is intended to check that when provided invalid arguments, it throws instead of crashes
async function testLoadDocument() {
  let caught = false;
  try {
    const docClient = await libreoffice.loadDocument(null);
  } catch {
    caught = true;
  }
  await idle();
  assert(caught);

  caught = false;

  try {
    const docClient = await libreoffice.loadDocument(undefined);
  } catch {
    caught = true;
  }
  await idle();
  assert(caught);

  caught = false;
}

testLoadDocument();
