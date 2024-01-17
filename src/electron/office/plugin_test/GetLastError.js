async function testGetLastError() {
  assert(libreoffice.getLastError() === "");

  const x = await loadEmptyDoc();
  await x.saveToMemory("not-a-real-format");
  assert(libreoffice.getLastError().includes("Code:26"));
}

testGetLastError();
