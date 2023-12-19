async function testDestructor() {
  let doc = await libreoffice.loadDocument('private:factory/swriter');
  doc = undefined;
}

testDestructor();
