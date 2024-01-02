async function testDocumentClientSetAuthor() {
  const docClient = await libreoffice.loadDocument('private:factory/swriter');
  const targetAuthor = 'hutch';

  docClient.setAuthor(targetAuthor);
  const result = await docClient.getCommandValues('.uno:TrackedChangeAuthors');

  assert(result.authors?.find(author => author.name === targetAuthor));
}

testDocumentClientSetAuthor();
