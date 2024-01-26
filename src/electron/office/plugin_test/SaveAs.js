async function testSaveAs() {
  const x = await loadEmptyDoc();
  assert(x != null);

  await x.initializeForRendering();
  getEmbed().renderDocument(x);
  await ready(x);

  let caught = false;
  try {
    // should throw without a path
    x.saveAs();
  } catch {
    caught = true;
  }

  // can save as docx based on extension
  const docxURL = tempFileURL(".docx");
  assert(await x.saveAs(docxURL));
  assert(fileURLExists(docxURL));
  assert(docxURL !== x.as('frame.XStorable').getLocation());

  // can save as docx pdf on extension
  const pdfURL = tempFileURL(".pdf");
  assert(await x.saveAs(pdfURL));
  assert(fileURLExists(pdfURL));
  assert(pdfURL !== x.as('frame.XStorable').getLocation());

  // can save as docx based on format
  const docxNoExtensionURL = tempFileURL("");
  assert(await x.saveAs(docxNoExtensionURL, "docx"));
  assert(fileURLExists(docxNoExtensionURL));
  assert(docxNoExtensionURL !== x.as('frame.XStorable').getLocation());

  // can save as pdf based on format
  const pdfNoExtensionURL = tempFileURL("");
  assert(await x.saveAs(pdfNoExtensionURL, "pdf"));
  assert(fileURLExists(pdfNoExtensionURL));
  assert(pdfNoExtensionURL !== x.as('frame.XStorable').getLocation());

  // can save and replace the current document
  const docxURL2 = tempFileURL(".docx");
  assert(await x.saveAs(docxURL2, "docx", "TakeOwnership"));
  assert(fileURLExists(docxURL2));
  assert(docxURL2 === x.as('frame.XStorable').getLocation());
}

testSaveAs();
