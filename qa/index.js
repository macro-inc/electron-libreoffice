const picker = document.getElementById('el-picker');
const embed = document.getElementById('el-embed');
const thumb = document.getElementById('el-thumb');

// libreoffice.on('status_indicator_set_value', (x) => {
//   console.log(x);
// });

// libreoffice.on('status_changed', (x) => {
//   console.log('lo', x);
// });

// libreoffice.on('window', (x) => {
//   console.log('lo', x);
// });

let globalDoc;
let zoom = 1.0;
let uri;
picker.onchange = () => {
  if (picker.files.length === 1) {
    uri = encodeURI(
      'file:///' + picker.files[0].path.replace(/\\/g, '/')
    );
    runColorizeWorker();
    const doc = libreoffice.loadDocument(uri);
    globalDoc = doc;

    embed.renderDocument(doc);
    thumb.renderDocument(doc);
    thumb.setZoom(0.1);
    embed.focus();
  }
};

function zoomIn() {
  zoom += 0.1;
  embed.setZoom(zoom);
}
function zoomOut() {
  if (zoom < 0.2) return;
  zoom -= 0.1;
  embed.setZoom(zoom);
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

function runColorizeWorker() {
  const worker = new Worker(fn2workerURL(colorizeWorker));
  worker.postMessage({type: 'load', file: uri });
  worker.onmessage = (event) => {
    console.log(event.data);
  }
}

function trackChangesWindow() {
  globalDoc.postUnoCommand('.uno:AcceptTrackedChanges');
}
function toggleTrackChanges() {
  globalDoc.postUnoCommand('.uno:TrackChanges');
}
function acceptTrackChange() {
  globalDoc.postUnoCommand('.uno:AcceptTrackedChange');
}
function rejectTrackChange() {
  globalDoc.postUnoCommand('.uno:RejectTrackedChange');
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

function colorizeWorker() {
  const colorizePalette = [ 0x333333, 0xA47E3B, 0x0F3460, 0xE94560];
  libreoffice.on('status_indicator_set_value', (x) => {
    self.postMessage({ type: 'load_progress', percent: x });
  });
  let doc;
  let shouldStop = false;
  self.addEventListener('message', function(e) {
    var data = e.data;
    switch (data.type) {
      case 'load':
        const timeStart = performance.now();
        self.postMessage({ type: 'loading', file: data.file });
        doc = libreoffice.loadDocument(data.file);
        self.postMessage({ type: 'loaded', file: data.file });
        const old = doc.as('text.XTextDocument');
        delete old;
        const xDoc = old.createHiddenClone();
        xDoc.startBatchUpdate();
        // FormatQuick and FormatLine respect the SwTextFrame::IsLocked, how do we engage that while formatting?
        const text = xDoc.getText();
        const cursor = text.createTextCursor();
        cursor.gotoStart(false);
        const wordCursor = cursor.as('text.XWordCursor');
        let i = 0;
        do {
          wordCursor.gotoEndOfWord(true);
          const props = wordCursor.as('beans.XPropertySet');
          props.setPropertyValue("CharColor", colorizePalette[i++ % colorizePalette.length]);
        } while (!shouldStop && wordCursor.gotoNextWord(false));
        // Skip calling finishBatchUpdate because the document will be saved as a PDF and discarded
        xDoc.as('frame.XStorable').storeToURL(data.file + ".pdf", { FilterName: 'writer_pdf_Export' });
        xDoc.as('util.XCloseable').close(true);
        self.postMessage({ type: 'finished', time: (performance.now() - timeStart)/1000 });
        self.close();
        break;
      case 'stop':
        shouldStop = true;
        self.postMessage({ type: 'stopped'});
        self.close(); // Terminates the worker.
        break;
      default:
        console.error('unknown type', data.type);
    };
  }, false);
}

function fn2workerURL(fn) {
  const blob = new Blob([`(${fn.toString()})()`], { type: "text/javascript" });
  return URL.createObjectURL(blob);
}
