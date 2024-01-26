async function testUndoRedo() {
  const x = await loadEmptyDoc();
  assert(x != null);

  let resolveUndo;
  let resolveRedo;
  let undoSet = false;
  const undoPromise = new Promise((resolve) => (resolveUndo = resolve));
  const redoPromise = new Promise((resolve) => (resolveRedo = resolve));
  x.on('state_changed', ({ payload }) => {
    if (payload.includes('.uno:Undo')) {
      undoSet = true;
      resolveUndo();
    }
    if (undoSet && payload.includes('.uno:Redo=enabled')) {
      resolveRedo();
    }
  });

  await x.initializeForRendering();
  getEmbed().renderDocument(x);
  await ready(x);

  assert(!canUndo());
  assert(!canRedo());
  sendKeyEvent(KeyEventType.Press, 'a');
  sendKeyEvent(KeyEventType.Press, 'b');
  sendKeyEvent(KeyEventType.Press, 'c');
  await undoPromise;

  const xTxtDoc = x.as('text.XTextDocument');
  assert(canUndo());
  assert(!canRedo());
  sendKeyEvent(KeyEventType.Press, 'mod+z');
  await idle();
  await redoPromise;
  assert(canRedo());
}

testUndoRedo();
