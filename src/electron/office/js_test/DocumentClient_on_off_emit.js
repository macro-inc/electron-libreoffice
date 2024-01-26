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

  // unknown event types should warn, but still function
  docClient.on('invalid_event', callback);
  docClient.emit('invalid_event', []);
  assert(fired);

  fired = false;
  docClient.off('invalid_event', callback);
  docClient.emit('invalid_event', []);
  assert(!fired);
}

testDocumentClientOnOffEmit();
