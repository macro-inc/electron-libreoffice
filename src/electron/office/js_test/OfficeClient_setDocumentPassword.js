async function testOfficeClientSetDocumentPassword() {
  const docClient = await libreoffice.loadDocument('private:factory/swriter');
  /* TODO: [MACRO-1899] fix setDocumentPassword in LOK, currently crashes with
   EXC_BAD_ACCESS (code=1, address=0x2e006e007500d3)
   frame #0: 0x000000011300a004 libsofficeapp.dylib`LOKInteractionHandler::SetPassword(this=0x002e006e00750073, pPassword=<unavailable>
   */
  // await libreoffice.setDocumentPassword('private:factory/swriter', null);
}

testOfficeClientSetDocumentPassword();
