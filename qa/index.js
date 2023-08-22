/// <reference path="../src/electron/npm/libreoffice.d.ts" />
/// <reference path="../qa/wrapper.js" />
const picker = document.getElementById('el-picker');
/** @type OfficeDoc */
const embed = document.getElementById('el-embed');
/** @type OfficeDoc */
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

window.addEventListener('beforeunload', () => {
  libreoffice._beforeunload();
});

let globalDoc;
let zoom = 1.0;
let uri;

const definitionsMap = new Map();
const definitionReferencesMap = new Map();

picker.onchange = async () => {
  if (picker.files.length === 1) {
    uri = encodeURI('file:///' + picker.files[0].path.replace(/\\/g, '/'));
    const doc = await libreoffice.loadDocument(uri);
    globalDoc = doc;
    runColorizeWorker();
    embed.renderDocument(doc);
    thumb.renderDocument(doc);
    thumb.setZoom(0.2);
    thumb.debounceUpdates(300);
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

function saveToMemory() {
  const buffer = globalDoc.saveToMemory();
  console.log('saveToMemory', { buffer });
  const newDoc = libreoffice.loadDocumentFromArrayBuffer(buffer);
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

async function runColorizeWorker() {
  const start = performance.now();
  const buffer = await globalDoc.saveToMemory();
  console.log(
    `save to memory took ${performance.now() - start}ms`,
    buffer.byteLength
  );
  const worker = new Worker(fn2workerURL(colorizeWorker));
  worker.postMessage({ type: 'load', file: uri, data: buffer }, [buffer]);
  worker.onmessage = (event) => {
    console.log(event.data);
  };
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
    globalDoc
      .as('beans.XPropertySet')
      .setPropertyValue('RecordChanges', true);
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

async function parseXmlTerms() {
  const f = await fetch('./testing/example_preprocess.json');
  const json = JSON.parse(await f.text());
  const xml = json.defs;

  const terms = new DOMParser()
    .parseFromString(xml, 'application/xml')
    .getElementsByTagName('term');
  const sortedTerms = Array.from(terms)
    .sort((a, b) => {
      const aId = a.getAttribute('id') || '';
      const bId = b.getAttribute('id') || '';
      return parseInt(aId) - parseInt(bId);
    })
    .map((termNode) => {
      const name = termNode.getAttribute('name') || '';
      const references = termNode
        .getElementsByTagName('references')[0]
        ?.getElementsByTagName('reference');
      const numRefs =
        termNode
          .getElementsByTagName('references')[0]
          ?.getElementsByTagName('reference')?.length ?? 0;
      const referenceHexes = [];

      for (let i = 0; i < references.length; i++) {
        referenceHexes.push({
          referenceStartHex:
            references[i].getAttribute('referenceStartHex') ?? '',
          referenceEndHex: references[i].getAttribute('referenceEndHex') ?? '',
        });
      }

      return {
        name,
        numRefs,
        references: referenceHexes,
        termStartHex: termNode.getAttribute('termStartHex') ?? '',
        termEndHex: termNode.getAttribute('termEndHex') ?? '',
      };
    });

  return sortedTerms;
}

async function applyDefinitions() {
  const definitions = await parseXmlTerms();

  // <string, Definition>
  const definitionsColorMap = new Map();
  // <string,{referenceStartHex: string;referenceEndHex: string;termStartHex: string;}>
  const referencesColorMap = new Map();

  for (let i = 0; i < definitions.length; i++) {
    const definition = definitions[i];
    if (definitionsColorMap.has(definition.termStartHex)) {
      console.log('duplicate definition: swapping termStartHex to termEndHex', {
        termStartHex: definition.termStartHex,
        first: definitionsColorMap.get(definition.termStartHex),
        second: {
          termEndHex: definition.termEndHex,
          index: i,
          name: definition.name,
        },
      });

      definition.termStartHex = definition.termEndHex;
    }

    // update references map
    if (definition.numRefs > 0) {
      const references = definition.references;
      for (const ref of references) {
        referencesColorMap.set(ref.referenceStartHex, {
          ...ref,
          termStartHex: definition.termStartHex,
        });
      }
    }

    definitionsColorMap.set(definition.termStartHex, definition);
  }

  console.log('created definitions color map', {
    nEntries: definitionsColorMap.size,
  });

  const xTxtDoc = globalDoc.as('text.XTextDocument');
  if (!xTxtDoc) {
    return;
  }
  const start = performance.now();
  xTxtDoc.startBatchUpdate();
  const text = xTxtDoc.getText();
  createOverlayLinks(
    text,
    definitionsColorMap,
    definitionsMap,
    referencesColorMap,
    definitionReferencesMap
  );
  xTxtDoc.finishBatchUpdate();
  console.log(`Created overlay links in ${performance.now() - start}ms`);
}

function gotoOverlay() {
  const xTxtDoc = globalDoc.as('text.XTextDocument');

  if (!xTxtDoc) {
    return;
  }

  const supplier = xTxtDoc
    .getCurrentController()
    .as('text.XTextViewCursorSupplier');

  if (!supplier) {
    return;
  }

  // The supplier comes back as an XInterface so we need to explicitly cast it here
  const cursor = supplier.getViewCursor();
  if (!cursor) {
    return;
  }

  // Set cursor to TextRange of definiton
  cursor.gotoRange(definitionsMap.get('012800'), false);
}

function createOverlayLinks(
  text,
  definitionsColorMap,
  definitionsMap,
  referencesColorMap,
  definitionReferencesMap
) {
  let definitionCountMatches = 0;
  let referenceCountMatches = 0;
  // RGB is 24-bit (0xFFFFFF = (1 << 24) - 1;
  const MAX_COLOR = 0xffffff;
  // right-most bits, max # words is 2**WORD_BITS
  const WORD_BITS = 9;
  // how much each paragraph increments
  const PARAGRAPH_SECTION_INCREMENT = 1 << WORD_BITS;
  // the mask of bits used for the word index
  const WORD_SECTION_MASK = PARAGRAPH_SECTION_INCREMENT - 1;
  // the mask of the bits used for the paragraph index (starts with PARAGRAPH_SECTION_INCREMENT)
  const PARAGRAPH_SECTION_MASK = MAX_COLOR - WORD_SECTION_MASK;

  const paragraphAccess = text.as('container.XEnumerationAccess');
  if (!paragraphAccess || !paragraphAccess.hasElements()) return;

  const paragraphIter = paragraphAccess.createEnumeration();

  if (!paragraphIter) return;

  let color = PARAGRAPH_SECTION_INCREMENT;

  // Definitions
  let definitionTextRange = undefined;
  // The end color of a definition
  let definitionEndColor = '';
  // The definition itself
  let definition = undefined;

  // References
  let referenceTextRange = undefined;
  // The end color of a definition
  let referenceEndColor = '';
  let reference = undefined;

  while (paragraphIter.hasMoreElements()) {
    let rawWordCount = 0;
    const el = paragraphIter.nextElement();
    const table = el.as('text.XTextTable');
    if (table) {
      // TODO: re-enable after reason for crash
      // visitTextTable(table, colorizeCell, cancelable);
      continue;
    }
    const paragraphTextRange = el.as('text.XTextRange');
    if (!paragraphTextRange) {
      continue;
    }

    const wordCursor = text
      .createTextCursorByRange(paragraphTextRange)
      .as('text.XWordCursor');
    const rangeCompare = text.as('text.XTextRangeCompare');
    if (!wordCursor || !rangeCompare) continue;

    do {
      // select the word
      wordCursor.gotoStartOfWord(false);
      wordCursor.gotoEndOfWord(true);

      const hexColor = color.toString(16).padStart(6, '0');

      if (definitionsColorMap.has(hexColor) && definitionEndColor === '') {
        definitionTextRange = wordCursor.getStart();

        definition = definitionsColorMap.get(hexColor);
        definitionEndColor = definition?.termEndHex ?? '';
      }

      if (hexColor === definitionEndColor) {
        if (!definitionTextRange || !definition) {
          console.error(
            'end of definiton reached without all necessary variables'
          );
          continue;
        }

        // Create new text cursor
        const definitionTextCursor = text.createTextCursor();

        // Set text cursor to be at the start of the range
        definitionTextCursor.gotoRange(definitionTextRange, false);

        // Send text cursor to end of current word
        definitionTextCursor.gotoRange(wordCursor.getEnd(), true);

        const props = definitionTextCursor.as('beans.XPropertySet');
        if (props) {
          props.setPropertyValue(
            'HyperLinkURL',
            `term://${definition.termStartHex}`
          );
          props.setPropertyValue(
            'VisitedCharStyleName',
            'Visited Internet Link'
          );
          props.setPropertyValue('UnvisitedCharStyleName', 'Internet Link');
        }

        definitionTextRange = definitionTextCursor.as('text.XTextRange');
        if (!definitionTextRange) {
          console.error('definitionTextCursor could not be cast as XTextRange');
          continue;
        }

        definitionsMap.set(definition.termStartHex, definitionTextRange);

        // Reset variables
        definitionTextRange = undefined;
        definition = undefined;
        definitionEndColor = '';
        definitionCountMatches++;
      }

      if (referencesColorMap.has(hexColor) && referenceEndColor === '') {
        referenceTextRange = wordCursor.getStart();

        reference = referencesColorMap.get(hexColor);

        if (!reference) {
          continue;
        }

        referenceEndColor = reference?.referenceEndHex ?? '';

        // initialize reference map value with empty array
        if (!definitionReferencesMap.has(reference.termStartHex)) {
          definitionReferencesMap.set(reference.termStartHex, []);
        }
      }

      // End a definition
      // definition might be 1 word so this could be done within the same loop as the block above
      if (hexColor === referenceEndColor) {
        if (!referenceTextRange || !reference) {
          console.error(
            'end of reference reached without all necessary variables'
          );
          continue;
        }

        // Create new text cursor
        const referenceTextCursor = text.createTextCursor();

        // Set text cursor to be at the start of the range
        referenceTextCursor.gotoRange(referenceTextRange, false);

        // Send text cursor to end of current word
        referenceTextCursor.gotoRange(wordCursor.getEnd(), true);

        const props = referenceTextCursor.as('beans.XPropertySet');
        if (props) {
          props.setPropertyValue(
            'HyperLinkURL',
            `termref://${reference.termStartHex}`
          );
        }

        referenceTextRange = referenceTextCursor.as('text.XTextRange');
        if (!referenceTextRange) {
          console.error('definitionTextCursor could not be cast as XTextRange');
          continue;
        }

        // Add referenceTextRange to the definitions references list
        // definitionReferencesMap
        //   .get(reference.termStartHex)
        //   ?.push(referenceTextRange);

        // Reset variables
        referenceTextRange = undefined;
        reference = undefined;
        referenceEndColor = '';
        referenceCountMatches++;
      }

      rawWordCount++;
      if ((++color & WORD_SECTION_MASK) == 0) {
        throw (
          'Ran out of word colors: ' +
          rawWordCount +
          '\n' +
          wordCursor?.getString()
        );
      }
      // despite what the documentation says, this will get stuck on a single word and return TRUE,
      // for example, in some cases if it precedes a table it will just repeatedly provide the same word
      wordCursor.gotoNextWord(false);
    } while (
      isWordBeforeEndOfParagraph(rangeCompare, wordCursor, paragraphTextRange)
    );

    color = (color + PARAGRAPH_SECTION_INCREMENT) & PARAGRAPH_SECTION_MASK;
    if (color == 0) {
      throw 'Ran out of colors';
    }
  }

  console.log(`Results`, { definitionCountMatches, referenceCountMatches });
  return color;
}

function isWordBeforeEndOfParagraph(
  rangeCompare,
  wordCursor,
  paragraphTextRange
) {
  return (
    rangeCompare.compareRegionStarts(
      wordCursor.getStart(),
      paragraphTextRange.getEnd()
    ) == 1
  );
}

function saveAsOverlays() {
  const start = Date.now();
  globalDoc.postUnoCommand('.uno:SaveAs', {
    URL: {
      type: 'string',
      value: `${uri}.fixed.docx`,
    },
    FilterName: {
      type: 'string',
      value: 'MS Word 2007 XML',
    },
  });
  console.log(`took ${Date.now() - start}ms to save`);
}
function saveOverlays() {
  const start = Date.now();
  globalDoc.postUnoCommand('.uno:Save');
  console.log(`took ${Date.now() - start}ms to save`);
}

/// <reference path="../src/electron/npm/libreoffice.d.ts" />
function colorizeWorker() {
  libreoffice.on('status_indicator_set_value', (x) => {
    self.postMessage({ type: 'load_progress', percent: x });
  });
  let doc;
  let shouldStop = false;
  function colorize(text) {
    // RGB is 24-bit (0xFFFFFF = (1 << 24) - 1;
    const MAX_COLOR = 0xffffff;
    // right-most bits, max # words is 2**WORD_BITS
    const WORD_BITS = 9;
    // how much each paragraph increments
    const PARAGRAPH_SECTION_INCREMENT = 1 << WORD_BITS;
    // the mask of bits used for the word index
    const WORD_SECTION_MASK = PARAGRAPH_SECTION_INCREMENT - 1;
    // the mask of the bits used for the paragraph index (starts with PARAGRAPH_SECTION_INCREMENT)
    const PARAGRAPH_SECTION_MASK = MAX_COLOR - WORD_SECTION_MASK;

    const paragraphAccess = text.as('container.XEnumerationAccess');
    if (!paragraphAccess || !paragraphAccess.hasElements()) return;

    const paragraphIter = paragraphAccess.createEnumeration();

    if (!paragraphIter) return;

    let color = PARAGRAPH_SECTION_INCREMENT;

    while (paragraphIter.hasMoreElements()) {
      let rawWordCount = 0;
      const el = paragraphIter.nextElement();
      const table = el.as('text.XTextTable');
      if (table) {
        // TODO: re-enable after reason for crash
        // visitTextTable(table, colorizeCell, cancelable);
        continue;
      }
      const paragraphTextRange = el.as('text.XTextRange');
      if (!paragraphTextRange) {
        continue;
      }

      const wordCursor = text
        .createTextCursorByRange(paragraphTextRange)
        .as('text.XWordCursor');
      const rangeCompare = text.as('text.XTextRangeCompare');
      if (!wordCursor || !rangeCompare) continue;

      do {
        // select the word
        wordCursor.gotoStartOfWord(false);
        wordCursor.gotoEndOfWord(true);

        // color it
        const props = wordCursor.as('beans.XPropertySet');
        if (!props) continue;
        props.setPropertyValue('CharColor', color);

        rawWordCount++;
        if ((++color & WORD_SECTION_MASK) == 0) {
          throw (
            'Ran out of word colors: ' +
            rawWordCount +
            '\n' +
            wordCursor?.getString()
          );
        }
        // despite what the documentation says, this will get stuck on a single word and return TRUE,
        // for example, in some cases if it precedes a table it will just repeatedly provide the same word
        wordCursor.gotoNextWord(false);
      } while (
        isWordBeforeEndOfParagraph(rangeCompare, wordCursor, paragraphTextRange)
      );

      color = (color + PARAGRAPH_SECTION_INCREMENT) & PARAGRAPH_SECTION_MASK;
      if (color == 0) {
        throw 'Ran out of colors';
      }
    }

    console.log('Last color was', '0x' + color.toString(16));

    return color;
  }

  function isWordBeforeEndOfParagraph(
    rangeCompare,
    wordCursor,
    paragraphTextRange
  ) {
    return (
      rangeCompare.compareRegionStarts(
        wordCursor.getStart(),
        paragraphTextRange.getEnd()
      ) == 1
    );
  }
  self.addEventListener(
    'message',
    async function (e) {
      var data = e.data;
      switch (data.type) {
        case 'load':
          const timeStart = performance.now();
          self.postMessage({ type: 'loading', ...data });
          doc = libreoffice.loadDocumentFromArrayBuffer(data.data);
          console.log(`Loaded document in ${performance.now() - timeStart}ms`);
          self.postMessage({ type: 'loaded', file: data.file });
          const xDoc = doc;
          xDoc.startBatchUpdate();
          const text = xDoc.getText();
          colorize(text, shouldStop);
          // Skip calling finishBatchUpdate because the document will be saved as a PDF and discarded
          xDoc.as('frame.XStorable').storeToURL(data.file + '.pdf', {
            FilterName: 'writer_pdf_Export',
          });
          xDoc.as('util.XCloseable').close(true);
          self.postMessage({
            type: 'finished',
            time: (performance.now() - timeStart) / 1000,
          });
          self.close();
          break;
        case 'stop':
          shouldStop = true;
          self.postMessage({ type: 'stopped' });
          self.close(); // Terminates the worker.
          break;
        default:
          console.error('unknown type', data.type);
      }
    },
    false
  );
}

function fn2workerURL(fn) {
  const blob = new Blob([`(${fn.toString()})()`], { type: 'text/javascript' });
  return URL.createObjectURL(blob);
}
