declare const libreoffice: LibreOffice.OfficeClient;

interface HTMLLibreOfficeEmbed<Client = LibreOffice.DocumentClient>
  extends HTMLEmbedElement {
  /**
   * updates the scroll to the yPosition in pixels
   * @param yPosition the position in CSS pixels: [0, the height of document in CSS pixels]
   */
  updateScroll(yPosition: number): void;
  /**
   * renders a LibreOffice.DocumentClient
   * @param doc the DocumentClient to be rendered
   * @param options options for rendering the document
   * @returns a unique restore key to restore the tiles if the embed is destroyed, empty if rendering failed
   */
  renderDocument(
    doc: Client,
    options?: {
      /** the initial zoom level */
      zoom?: number;
      /** disable input **/
      disableInput?: boolean;
      /** restore key from a previous call to renderDocument **/
      restoreKey?: string;
    }
  ): string;
  /**
   * description converts twip to a css px
   * @param input - twip
   * @returns css px
   */
  twipToPx(input: number): number;
  invalidateAllTiles(): void;
  /** The rectangles for the bounds of each page in the document, units are CSS pixels */
  get pageRects(): LibreOffice.PageRect[];
  /** The rectangles for the bounds of each page in the document, units are CSS pixels */
  get documentSize(): LibreOffice.Size;
  /** Sets the current zoom level
   * @param scale the scale where 1.0 is the base zoom level (100% zoom): (0,5]
   **/
  setZoom(scale: number): number;
  /** Gets the current zoom
   * @returns the current zoom
   **/
  getZoom(): number;

  /** Debounces updates that cause paints at a provided interval
   * @param interval in ms to debounce
   **/
  debounceUpdates(interval: number): void;
}

declare namespace LibreOffice {
  type ClipboardItem =
    | {
        mimeType: 'text/plain' | 'text/html';
        text: string;
      }
    | {
        mimeType: 'image/png';
        buffer: ArrayBuffer;
      };

  /** Size in CSS pixels */
  type Size = {
    width: number;
    height: number;
  };

  /** Page Rect in CSS pixels */
  type PageRect = {
    x: number;
    y: number;
  } & Size;

  /** Rect in CSS pixels */
  type Rect = [
    /** x */
    x: number,
    /** y */
    y: number,
    /** width */
    width: number,
    /** height */
    height: number
  ];

  /** Rect in twips */
  type TwipsRect = Rect & {};

  type EventPayload<T> = {
    payload: T;
  };

  type StateChangedValue =
    | string
    | { commandId: string; value: any; viewId?: number };

  type ContextMenuSeperator = { type: 'separator' };
  type ContextMenuCommand<Commands> = {
    type: 'command';
    text: string;
    enabled: 'false' | 'true';
    command: Commands;
  };
  type ContextMenu<Commands> = {
    type: 'menu';
    menu: Array<
      | ContextMenuCommand<Commands>
      | ContextMenuSeperator
      | ContextMenu<Commands>
    >;
  };

  interface DocumentEvents<
    Commands extends string | number = keyof UnoCommands
  > {
    document_size_changed: EventPayload<TwipsRect>;
    invalidate_visible_cursor: EventPayload<TwipsRect>;
    cursor_visible: EventPayload<boolean>;
    set_part: EventPayload<number>;
    ready: StateChangedValue[];
    state_changed: EventPayload<StateChangedValue>;
    context_menu: EventPayload<ContextMenu<Commands>>;
    clipboard_changed:
      | null
      | EventPayload<null>
      | EventPayload<{ sw: true }>
      | EventPayload<{ mimeType: string }>;

    // TODO: document these types
    hyperlink_clicked: any;
    text_selection_start: any;
    text_selection: any;
    text_selection_end: any;
    window: any;
    comment: any;
    redline_table_entry_modified: any;
    redline_table_size_changed: any;
    invalidate_tiles: any;
  }

  type DocumentEventHandler<
    Events extends DocumentEvents = DocumentEvents,
    Event extends keyof Events = keyof Events
  > = (arg: Events[Event]) => void;

  export type NumberString<T extends number = number> = `${T}`;

  export type UnoString = { type: 'string'; value: string };
  export type UnoBoolean = { type: 'boolean'; value: boolean };
  export type UnoLong = { type: 'long'; value: number };
  export type UnoFloat = { type: 'float'; value: NumberString };
  export type UnoStringifiedNumber = { type: 'string'; value: NumberString };

  interface UnoCommands {
    '.uno:SetPageColor': { ColorHex: UnoString };
    '.uno:FontColor': { FontColor: UnoLong };
    '.uno:Highlight': { BackColor: UnoLong };
  }

  type CommandValueResult<R = { [name: string]: any }> = {
    commandValues: R;
  };

  interface GetCommands {
    '.uno:PageColor': string;
    '.uno:TrackedChangeAuthors': {
      authors: Array<{
        index: number;
        name: string;
      }>;
    };
  }

  interface DocumentClient<
    Events extends DocumentEvents = DocumentEvents,
    Commands extends string | number = keyof UnoCommands,
    CommandMap extends { [K in Commands]?: any } = UnoCommands,
    GCV extends GetCommands = GetCommands
  > {
    /**
     * add an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    on<K extends keyof Events = keyof Events>(
      eventName: K,
      callback: DocumentEventHandler<Events, K>
    ): void;

    /**
     * turn off an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    off<K extends keyof Events = keyof Events>(
      eventName: K,
      callback: DocumentEventHandler<Events, K>
    ): void;

    /**
     * emit an event for an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    emit<K extends keyof Events = keyof Events>(
      eventName: K,
      callback: DocumentEventHandler<Events, K>
    ): void;

    /**
     * Sets the author of the document
     * @param author - the new authors name
     */
    setAuthor(author: string): void;

    /**
     * posts a UNO command to the document
     * @param command - the uno command to be posted
     * @param args - arguments for the uno command
     * @param notifyWhenFinished - whether an UNO command result event should be sent for the result
     */
    postUnoCommand<K extends Commands>(
      command: K,
      args?: K extends keyof NonNullable<CommandMap>
        ? NonNullable<CommandMap>[K]
        : never,
      notifyWhenFinished?: boolean
    ): void;

    /**
     * sets the start or end of a text selection
     * @param type - the text selection type
     * @param x - the horizontal position in document coordinates
     * @param y - the vertical position in document coordinates
     */
    setTextSelection(type: number, x: number, y: number): void;

    /**
     * gets the content of the clipboard for the current view
     * @param mimeTypes - desired MIME types from the clipboard
     * @returns an array of clipboard items
     */
    getClipboard(
      mimeTypes?: Array<ClipboardItem['mimeType']>
    ): Array<ClipboardItem | undefined>;

    /**
     * populates the clipboard for the current view with multiple types of content
     * @param clipboardData - array of clipboard items used to populate the clipboard
     * @returns whether the operation was successful
     */
    setClipboard(clipboardData: ClipboardItem[]): boolean;

    /**
     * pastes content at the current cursor position
     * @param mimeType - the mime type of the data to paste
     * @param data - the data to be pasted
     * @returns whether the paste was successful
     */
    paste(mimeType: string, data: string): boolean;

    /**
     * adjusts the graphic selection
     * @param type - the graphical selection type
     * @param x - the horizontal position in document coordinates
     * @param y - the horizontal position in document coordinates
     */
    setGraphicsSelection(type: number, x: number, y: number): void;

    /**
     * gets rid of any text or graphic selection
     */
    resetSelection(): void;

    /**
     * returns a json mapping of the possible values for the given command
     * e.g. {commandName: ".uno:StyleApply", commandValues: {"familyName1" :
     * ["list of style names in the family1"], etc.}}
     * @param command - the UNO command for which possible values are requested
     * @returns the command object with possible values
     */
    getCommandValues<K extends keyof GCV = keyof GCV>(
      command: K
    ): Promise<GCV[K]>;

    /**
     * sets the cursor to a given outline node
     * @param id - the id of the node to go to
     * @returns the rect of the node where the cursor is brought to
     */
    gotoOutline(id: number):
      | {
          destRect: string;
        }
      | undefined;

    /**
     * saves the document to memory
     * @param [format] - the optional format the document saves to, when omitted docx is used
     * @returns an array buffer with the bytes of the document or undefiend if saving failed
     */
    saveToMemory(format?: string): Promise<ArrayBuffer | undefined>;

    /**
     * saves the document to memory
     * Stores the document's persistent data to a URL and continues to be a representation of the old URL.
     *
     * @param url the location where to store the document
     * @param [format] the format to use while exporting, when omitted, then deducted from the URL's file extension
     * @param [filter] options for the export filter
     * @returns true if the save succeeded, false otherwise
     */
    saveAs(
      url: string,
      format?:
        | 'doc'
        | 'docm'
        | 'docx'
        | 'html'
        | 'odt'
        | 'ott'
        | 'pdf'
        | 'txt'
        | 'png',
      filter?: string
    ): Promise<boolean>;

    /**
     * if the document is ready
     * @returns {boolean}
     */
    isReady(): boolean;

    /**
     * run immediately after loading a document for documents that will be rendered
     */
    initializeForRendering(): Promise<void>;

    /**
     * returns a new DocumentClient with a seperate LOK view
     **/
    newView(): DocumentClient<Events, Commands, CommandMap, GCV>;

    as: import('./lok_api').text.GenericTextDocument['as'];
  }

  interface OfficeClient {
    /**
     * set password required for loading or editing a document
     * @param url - the URL of the document, as sent to the callback
     * @param password - the password, null indicates no password
     *
     * In response to a `document_password` request event:
     * - a valid password will continue loading the document
     * - an invalid password will result in another 'document_password' request event,
     * - a `null` password will abort loading the document.
     *
     * In response to `document_password_to_modify`:
     * - a valid password will continue loading the document
     * - an invalid password will result in another `document_password_to_modify` request
     * - a `null` password will continue loading the document in read-only mode
     */
    // TODO: [MACRO-1899] fix setDocumentPassword in LOK, then re-enable
    // setDocumentPassword(url: string, password: string | null): Promise<void>;

    /**
     * loads a given document
     * @param path - the document path
     * @returns a Promise of the document client if the load succeeded, undefined if the load failed
     */
    loadDocument<C = DocumentClient>(path: string): Promise<C | undefined>;

    /**
     * loads a given document from an ArrayBuffer
     * @param buffer - the array buffer of the documents contents
     * @returns a DocumentClient created from the ArrayBuffer
     */
    loadDocumentFromArrayBuffer<C = DocumentClient>(
      buffer: ArrayBuffer
    ): Promise<C | undefined>;

    /** gets the last error thrown by LOK */
    getLastError(): string;
  }
}
