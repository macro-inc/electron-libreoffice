async function testDocumentClientOnOffEmit() {
  const docClient = await libreoffice.loadDocument('private:factory/swriter');
  let fired = false;

  docClient.emit('ready', []);
  assert(!fired);

  const callback = () => { fired = true };
  docClient.on('ready', callback);
  docClient.emit('ready', []);
  assert(fired);

  fired = false;
  docClient.off('ready', callback);
  docClient.emit('ready', []);
  assert(!fired);
}

testDocumentClientOnOffEmit();
