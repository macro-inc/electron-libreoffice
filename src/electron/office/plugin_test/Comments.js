async function testComments() {
  const x = await loadEmptyDoc();
  assert(x != null);

  await x.initializeForRendering();
  getEmbed().renderDocument(x);
  await ready(x);

  const testComment = 'This is a comment';

  x.postUnoCommand('.uno:InsertAnnotation', {
    Text: {
      type: 'string',
      value: testComment
    }
  });
  await idle();
  /** @type unknown */
  let comments = await x.getCommandValues('.uno:ViewAnnotations');
  log(JSON.stringify(comments));
  assert(comments.comments);
  assert(comments.comments.length === 1);
  assert(comments.comments[0].author === 'Macro User');
  assert(comments.comments[0].text === testComment);
  assert(comments.comments[0].resolved === "false");

  x.postUnoCommand('.uno:ResolveComment', {
    Id: {
      type: 'string',
      value: String(comments.comments[0].id)
    },
  });
  comments = await x.getCommandValues('.uno:ViewAnnotations');
  assert(comments.comments);
  assert(comments.comments.length === 1);
  assert(comments.comments[0].author === 'Macro User');
  assert(comments.comments[0].text === testComment);
  assert(comments.comments[0].resolved === "true");

  const testAuthor = 'Robert Frost';
  x.setAuthor(testAuthor);
  const authorComment = 'Whose woods these are I think I know.';
  x.postUnoCommand('.uno:InsertAnnotation', {
    Text: {
      type: 'string',
      value: authorComment
    }
  });
  comments = await x.getCommandValues('.uno:ViewAnnotations');
  assert(comments.comments);
  assert(comments.comments.length === 2);
  assert(comments.comments[1].author === testAuthor);
  assert(comments.comments[1].text === authorComment);
  assert(comments.comments[1].resolved === "false");
}

testComments();
