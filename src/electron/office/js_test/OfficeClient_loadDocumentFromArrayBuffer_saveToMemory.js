async function testLoadDocumentFromArrayBuffer() {
  const docClient = await libreoffice.loadDocument('private:factory/swriter');
  const inMemory = await docClient.saveToMemory();
  assert(inMemory.byteLength !== 0);
  assert(await libreoffice.loadDocumentFromArrayBuffer(inMemory) != null);
}

testLoadDocumentFromArrayBuffer();
