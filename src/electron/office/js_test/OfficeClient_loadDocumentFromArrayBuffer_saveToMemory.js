async function testLoadDocumentFromArrayBuffer() {
  const docClient = await libreoffice.loadDocument('private:factory/swriter');
  const inMemory = await docClient.saveToMemory();
  assert(inMemory.byteLength !== 0);
  assert((await libreoffice.loadDocumentFromArrayBuffer(inMemory)) != null);
  const inMemoryPDF = await docClient.saveToMemory('writer_pdf_Export');
  assert(inMemoryPDF.byteLength !== 0);

  const invalid = await docClient.saveToMemory('not-a-real-format');
  assert(invalid == null);

  const emptyBuffer = new ArrayBuffer(0);
  assert((await libreoffice.loadDocumentFromArrayBuffer(emptyBuffer)) == null);
}

testLoadDocumentFromArrayBuffer();
