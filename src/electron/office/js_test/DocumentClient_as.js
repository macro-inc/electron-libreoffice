libreoffice.loadDocument('private:factory/swriter').then(x => {
  assert(x != null);
  assert(x.as('uno.XInterface') != null);
  assert(x.as('text.XTextDocument')?.constructor.name === 'text.XTextDocument');
});
