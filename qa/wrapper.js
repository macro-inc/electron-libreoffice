/// <reference path="../src/electron/npm/libreoffice.d.ts" />
'use strict';

function css(x) {
  return x;
}

const styleCSS = css`
  :host {
    display: flex;
    flex-direction: column;
    height: 100%;
    width: 100%;
    overflow: hidden;
  }

  .container {
    display: flex;
    flex: 1;
    overflow: hidden;
    position: relative;
    background: #ccc;
  }

  @keyframes blink-kf {
    to {
      visibility: hidden;
    }
  }
  .cursor {
    z-index: 2;
    display: block;
    background: #000;
    position: relative;
    pointer-events: none;
  }

  .blink {
    animation: blink-kf 1s steps(5, start) infinite;
  }

  .scroll {
    display: flex;
    overflow: auto;
    position: relative;
    top: 0;
    height: 100%;
    width: 100%;
    justify-content: center;
  }

  .pages {
    z-index: 3;
    position: absolute;
    pointer-events: none;
  }
  .pages .page {
    position: absolute;
    border: 1px solid red;
    margin: -1px 0 0 -1px;
  }
  .page .overlay {
    position: absolute;
    inset: 0;
  }

  embed {
    z-index: 1;
    position: sticky;
    top: 0;
    height: 100%;
  }
`;

class OfficeDoc extends HTMLElement {
  constructor() {
    super();

    const shadow = this.attachShadow({ mode: 'open' });
    const style = document.createElement('style');
    style.textContent = styleCSS;
    const container = document.createElement('div');
    container.className = 'container';

    /** @type HTMLLibreOfficeEmbed */
    this.embed = document.createElement('embed');
    this.embed.setAttribute('type', 'application/x-libreoffice');
    this.embed.style.display = 'none';

    this.sizer = document.createElement('div');
    this.sizer.className = 'sizer';

    this.scroller = document.createElement('div');
    this.scroller.className = 'scroll';

    this.pages = document.createElement('div');
    this.pages.className = 'pages';

    this.cursor = document.createElement('div');
    this.cursor.className = 'cursor';
    this.sizer.appendChild(this.cursor);

    this.scroller.appendChild(this.sizer);
    this.scroller.appendChild(this.embed);
    this.scroller.appendChild(this.pages);

    this.scroller.addEventListener('scroll', this._handleScroll.bind(this), {
      passive: true,
    });

    container.appendChild(this.scroller);

    shadow.appendChild(style);
    shadow.appendChild(container);
    this.log = true;
  }

  connectedCallback() {
    // mounted
  }
  disconnectedCallback() {
    // unmounted
  }

  _setCursor(payload) {
    const [x, y, width, height] = payload.map((n) =>
      Math.max(this.embed.twipToPx(n), 1)
    );
    this.cursor.style.transform = `translate(${x + 1.067}px, ${y}px)`;
    this.cursor.style.width = `${width}px`;
    this.cursor.style.height = `${height}px`;
    this.cursor.classList.add('blink');
  }

  twipToPx(in_) {
    return this.embed.twipToPx(in_);
  }

  silenceLogIt() {
    this.log = false;
  }

  setZoom(zoom) {
    const old_zoom = this.embed.getZoom ? this.embed.getZoom() : 1.0;
    const old_scroll = this.scroller.scrollTop;
    this.embed.setZoom(zoom);
    this._refreshSize();
    this.scroller.scrollTop = (zoom / old_zoom) * old_scroll;
    if (this._cursor_payload) this._setCursor(this._cursor_payload);
  }

  invalidateAllTiles() {
    this.embed.invalidateAllTiles();
  }

  debounceUpdates(interval) {
    this.embed.debounceUpdates(interval);
  }

  /** @param {LibreOffice.DocumentClient} doc */
  async setClipboard(doc) {
    const items = await navigator.clipboard.read();
    const blobs = {};
    let typesAvailable = 0;

    for (const clipboardItem of items) {
      for (const type of clipboardItem.types) {
        if (
          ![
            'text/plain',
            'text/html',
            'image/png',
            'web text/x-macro-lastupdate',
          ].includes(type)
        )
          continue;
        const blob = await clipboardItem.getType(type);
        blobs[type] = blob;

        if (!type.startsWith('web ')) typesAvailable++;
      }
    }
    const lastUpdate = blobs['web text/x-macro-lastupdate'];
    // setting the clipboard if theres a newer update, or no update metadata
    // ensures that both internal and external clipboard data is handled as expected
    if (
      !this.lastClipboardUpdate ||
      !lastUpdate ||
      (await lastUpdate.text()) > this.lastClipboardUpdate
    ) {
      const items = [];
      for (const blobType in blobs) {
        if (blobType.startsWith('web ')) continue;
        if (blobType.startsWith('text/plain') && typesAvailable > 1) continue;

        items.push({
          mimeType: blobType,
          buffer: await blobs[blobType].arrayBuffer(),
        });
      }
      doc.setClipboard(items);
    }
  }

  /**
   * @param {LibreOffice.DocumentClient} doc DocumentClient object to render
   */
  renderDocument(doc, options) {
    const embed = this.embed;
    embed.style.display = 'block';
    this.restoreKey = embed.renderDocument(doc, options);
    console.log('restore key', this.restoreKey);
    this.doc = doc;
    this._refreshSize();
    doc.on('document_size_changed', this._refreshSize);
    let lastTime = Date.now();
    const logit = (x) => {
      if (!this.log) return;
      const now = Date.now();
      console.log(now - lastTime, Date.now(), x);
      lastTime = now;
    };
    doc.on('invalidate_visible_cursor', ({ payload }) => {
      this._cursor_payload = payload;
      this._setCursor(payload);
    });
    // doc.on('text_selection_start', logit);
    // doc.on('text_selection_end', logit);
    // doc.on('text_selection', logit);
    doc.on('hyperlink_clicked', logit);
    doc.on('cursor_visible', logit);
    doc.on('set_part', logit);
    doc.on('context_menu', logit);
    // doc.on('state_changed', logit);
    doc.on('window', logit);
    doc.on('jsdialog', logit);
    doc.on('uno_command_result', logit);
    doc.on('redline_table_size_changed', logit);
    doc.on('redline_table_entry_modified', logit);
    doc.on('macro_overlay', logit);
    doc.on('macro_colorizer', logit);
    doc.on('mouse_pointer', ({ payload }) => {
      embed.style.cursor = payload;
    });

    doc.on('clipboard_changed', async (response) => {
      if (!response?.payload?.sw) {
        return;
      }
      console.log('clipboard changed');

      const clip = doc.getClipboard(['image/png', 'text/plain', 'text/html']);
      // empty clipboard is likely an accidental clear from LOK
      if (!clip.some((x) => x)) return;

      const blobs = {};
      for (const type of clip) {
        if (!type) continue;

        blobs[type.mimeType] = new Blob(
          [type.mimeType === 'image/png' ? type.buffer : type.text],
          { type: type.mimeType }
        );
      }
      this.lastClipboardUpdate = Date.now().toString();
      blobs['web text/x-macro-lastupdate'] = new Blob(
        [this.lastClipboardUpdate],
        {
          type: 'web text/x-macro-lastupdate',
        }
      );
      await navigator.clipboard.write([new ClipboardItem(blobs)]);
    });

    embed.addEventListener('keypress', async (e) => {
      if (e.code === 'KeyV' && (e.ctrlKey || e.metaKey)) {
        if (e.shiftKey) {
          const plainText = await navigator.clipboard.readText();
          doc.paste('text/plain;charset=utf-8', plainText);
        } else {
          await this.setClipboard(doc);
          doc.postUnoCommand('.uno:Paste');
        }
      }
    });
  }

  remount() {
    const { width, height } = this.embed.documentSize;
    const { scrollTop } = this.scroller;
    const pages = this.scroller.removeChild(this.pages);
    this.scroller.removeChild(this.embed);

    this.embed = document.createElement('embed');
    this.embed.setAttribute('type', 'application/x-libreoffice');
    this.embed.style.display = 'block';
    this._setDimensions(width, height);

    this.scroller.appendChild(this.embed);
    this.scroller.appendChild(pages);

    this.ignoreScroll = true;
    this.renderDocument(this.doc, {
      restoreKey: this.restoreKey,
    });

    this.scroller.scrollTop = scrollTop;
  }

  focus() {
    this.embed.focus();
  }

  _handleScroll() {
    if (this.ignoreScroll && this.scroller.scrollTop == 0) {
      this.ignoreScroll = false;
      return;
    }
    this.embed.updateScroll(this.scroller.scrollTop);
  }

  _refreshSize = () => {
    if (!this.doc) {
      console.error('Doc is not set!');
      return;
    }
    const embed = this.embed;
    const { width, height } = embed.documentSize;
    console.log({ width, height });
    this._setDimensions(width, height);

    this._pageRects = embed.pageRects;
    this._pageNodes = this._pageRects.map((rect) => {
      const node = document.createElement('div');
      node.className = 'page';
      node.style.top = `${rect.y}px`;
      node.style.left = `${rect.x}px`;
      node.style.width = `${rect.width}px`;
      node.style.height = `${rect.height}px`;

      return node;
    });
    this.pages.replaceChildren(...this._pageNodes);
  };

  _scrollToPage(index) {
    if (!this.doc || !this._pageRects) {
      console.error('Doc is not set!');
      return;
    }

    if (this._pageRects.length < index) {
      console.error('Invalid page index!');
      return;
    }

    this.scroller.scrollTop = this._pageRects[index].y;
  }

  /**
   * Sets the dimensions as reported by the plugin
   * @param {number} width
   * @param {number} height
   */
  _setDimensions(width, height) {
    console.log(`W: ${width} H: ${height}`);
    const w = `${width}px`;
    this.embed.style.width = w;
    this.pages.style.width = w;
    this.pages.style.height = `${height}px`;
  }
}

if (!customElements.get('office-doc'))
  customElements.define('office-doc', OfficeDoc);
