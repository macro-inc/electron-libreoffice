/// <reference path="../src/electron/npm/libreoffice.d.ts" />
/// <reference path="../qa/wrapper.js" />
const picker = document.getElementById('el-picker');
/** @type OfficeDoc */
const embed = document.getElementById('el-embed');
/** @type OfficeDoc */
const thumb = document.getElementById('el-thumb');

let globalDoc;
let zoom = 1.0;
let uri;

const definitionsMap = new Map();
const definitionReferencesMap = new Map();

picker.onchange = async () => {
  if (picker.files.length === 1) {
    uri = encodeURI('file:///' + picker.files[0].path.replace(/\\/g, '/'));
    const doc = await libreoffice.loadDocument(uri);
    await doc.initializeForRendering();
    globalDoc = doc;
    restoreKey = embed.renderDocument(doc);
    const thumbDoc = doc.newView();
    thumb.silenceLogIt();
    thumb.renderDocument(thumbDoc, { disableInput: true, zoom: 0.2 });
    thumb.debounceUpdates(300);
    embed.focus();
  }
};

async function loadEmpty() {
  const doc = await libreoffice.loadDocument('private:factory/swriter');
  await doc.initializeForRendering();
  globalDoc = doc;
  restoreKey = embed.renderDocument(doc);
  // embed.focus();
}

function zoomIn() {
  zoom += 0.1;
  embed.setZoom(zoom);
}

function zoomOut() {
  if (zoom < 0.2) return;
  zoom -= 0.1;
  embed.setZoom(zoom);
}

async function saveToMemory() {
  const buffer = await globalDoc.saveToMemory();
  console.log('saveToMemory', { buffer });
  const newDoc = await libreoffice.loadDocumentFromArrayBuffer(buffer);
  console.log('New doc', { text: newDoc.getText().getString() });
}

// Used as POC to show how to structure outline data
function constructBookmarkTree(outline) {
  const outlineTree = {};
  attachChildrenToNodes(outline, outlineTree);
  return outlineTree;
}

function attachChildrenToNodes(outline, outlineTree) {
  for (const node of outline) {
    outlineTree[node.id] = node;
    const children = outline.filter((o) => o.parent === node.id);
    outlineTree[node.id].children = children;
  }
}

function remount() {
  embed.remount();
  embed.focus();
}

function trackChangesWindow() {
  globalDoc.postUnoCommand('.uno:AcceptTrackedChanges');
}

function toggleTrackChanges() {
  embed.focus();
  globalDoc.postUnoCommand('.uno:TrackChanges');
}

function insertAnnotation() {
  globalDoc.postUnoCommand('.uno:InsertAnnotation', {
    Text: { type: 'string', value: 'TEST COMMENT' },
  });
}

function deleteFirstComment() {
  const tcEnabled = globalDoc
    .as('beans.XPropertySet')
    .getPropertyValue('RecordChanges');
  if (tcEnabled)
    globalDoc.as('beans.XPropertySet').setPropertyValue('RecordChanges', false);
  const { comments } = globalDoc.getCommandValues('.uno:ViewAnnotations');
  console.log('before', comments);
  if (comments.length) {
    embed.focus();
    globalDoc.postUnoCommand('.uno:DeleteCommentThread', {
      Id: { type: 'string', value: String(comments[0].id) },
    });
  }
  if (tcEnabled)
    globalDoc.as('beans.XPropertySet').setPropertyValue('RecordChanges', true);
  console.log(
    'after',
    globalDoc.getCommandValues('.uno:ViewAnnotations').comments
  );
}

function changeAuthor() {
  globalDoc.setAuthor('New author');
}

function getTrackChanges() {
  console.log(
    'TC INFO',
    globalDoc.getCommandValues('.uno:ViewTrackChangesInformation')
  );
}

function getComments() {
  console.log(
    'COMMENT INFO',
    globalDoc.getCommandValues('.uno:ViewAnnotations').comments
  );
}

function acceptTrackChange() {
  globalDoc.postUnoCommand('.uno:AcceptTrackedChange');
}

function rejectTrackChange() {
  globalDoc.postUnoCommand('.uno:RejectTrackedChange');
}

function invalidateAllTiles() {
  embed.invalidateAllTiles();
}

function gotoOutline() {
  const outline = doc.getCommandValues('.uno:GetOutline');

  if (outline.outline) {
    // CONSTRUCT Tree for bookmarks
    console.log('Outline tree', constructBookmarkTree(outline.outline));
  }
  console.log('OUTLINE', globalDoc.gotoOutline(1));
}
function insertTable() {
  globalDoc.postUnoCommand('.uno:CreateTable', {
    Row: { value: 4, type: 'long' },
    Col: { value: 3, type: 'long' },
  });
}
let winhandle;
function iframewin() {
  const url = 'https://github.com/coparse-inc/electron-libreoffice';
  winhandle = window.open('about:blank', 'bug_dialog', 'nodeIntegration=no');
  winhandle.focus();
  const iFrameStyle =
    'position:fixed; top:0; left:0; bottom:0; right:0; width:100%; height:100%; border:none; margin:0; padding:0; overflow:hidden; z-index:999999;';
  winhandle.document.write(
    `<iframe src="${url}" style="${iFrameStyle}"}></iframe>`
  );
}
function closeiframewin() {
  winhandle.close();
}
