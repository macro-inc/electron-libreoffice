"use strict";

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

    const shadow = this.attachShadow({ mode: "open" });
    const style = document.createElement("style");
    style.textContent = styleCSS;
    const container = document.createElement("div");
    container.className = "container";

    this.embed = document.createElement("embed");
    this.embed.setAttribute("type", "application/x-libreoffice");
    this.embed.style.display = "none";

    this.sizer = document.createElement("div");
    this.sizer.className = "sizer";

    this.scroller = document.createElement("div");
    this.scroller.className = "scroll";

    this.pages = document.createElement("div");
    this.pages.className = "pages";

    this.cursor = document.createElement("div");
    this.cursor.className = "cursor";
    this.sizer.appendChild(this.cursor);

    this.scroller.appendChild(this.sizer);
    this.scroller.appendChild(this.embed);
    this.scroller.appendChild(this.pages);

    this.scroller.addEventListener("scroll", this._handleScroll.bind(this), {
      passive: true
    });

    container.appendChild(this.scroller);

    shadow.appendChild(style);
    shadow.appendChild(container);
  }

  connectedCallback() {
    // mounted
  }
  disconnectedCallback() {
    // unmounted
  }

  /**
   * @param {any} doc DocumentClient object to render
   */
  renderDocument(doc) {
    this.embed.style.display = "block";
    this.embed.renderDocument(doc);
    this.doc = doc;
    this._refreshSize();
    doc.on('document_size_changed', this._refreshSize);
    const logit = (x) => {console.log(x)};
    doc.on('invalidate_visible_cursor', ({payload}) => {
      const [x,y,width,height] = payload.map(n => Math.max(doc.twipToPx(n), 1));
      this.cursor.style.transform = `translate(${x + 1.067}px, ${y}px)`;
      this.cursor.style.width = `${width}px`;
      this.cursor.style.height = `${height}px`;
      this.cursor.classList.add('blink');
    });
    doc.on('text_selection_start', logit);
    doc.on('text_selection_end', logit);
    doc.on('text_selection', logit);
    doc.on('hyperlink_clicked', logit);
    doc.on('cursor_visible', logit);
    doc.on('set_part', logit);
  }

  focus() {
    this.embed.focus();
  }

  _handleScroll() {
    this.embed.updateScroll({x: this.scroller.scrollLeft, y: this.scroller.scrollTop});
  }

  _refreshSize = () => {
    if (!this.doc) {
      console.error('Doc is not set!')
      return;
    }
    const doc = this.doc;
    const { width, height } = doc.size;
    this._setDimensions(width, height);

    this._pageRects = doc.pageRects;
    this._pageNodes = this._pageRects.map(rect => {
      const node = document.createElement('div');
      node.className = 'page';
      node.style.top = `${rect.y}px`;
      node.style.left = `${rect.x}px`;
      node.style.width = `${rect.width}px`;
      node.style.height = `${rect.height}px`;

      return node;
    });
    this.pages.replaceChildren(...this._pageNodes);
  }

  _scrollToPage(index) {
    if (!this.doc || !this._pageRects) {
      console.error('Doc is not set!')
      return;
    }

    if (this._pageRects.length < index) {
      console.error('Invalid page index!')
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
    const w = `${width}px`;
    this.embed.style.width = w;
    this.pages.style.width = w;
    this.pages.style.height = `${height}px`;
  }
}

if (!customElements.get("office-doc"))
  customElements.define("office-doc", OfficeDoc);
