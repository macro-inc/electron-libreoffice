// this is intended to check that when provided invalid arguments, it throws instead of crashes
async function test() {
  let caught = false;
  try {
    const docClient = await libreoffice.loadDocument(null);
  } catch {
    caught = true;
  }
  assert(caught);

  caught = false;

  try {
    const docClient = await libreoffice.loadDocument(undefined);
  } catch {
    caught = true;
  }
  assert(caught);

  caught = false;

  try {
    const docClient = await libreoffice.loadDocument(123);
  } catch {
    caught = true;
  }
  assert(caught);
}
