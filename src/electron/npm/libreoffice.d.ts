declare const libreoffice: LibreOffice.OfficeClient;

interface HTMLLibreOfficeEmbed extends HTMLEmbedElement {
  updateScroll(position: { x: number; y: number }): void;
  renderDocument(doc: LibreOffice.DocumentClient): void;
}

declare namespace LibreOffice {
  type ClipboardItem = {
    mimeType: string;
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
  interface TwipsRect extends Rect {}

  type EventPayload<T> = {
    payload: T;
  };

  type StateChangedValue =
    | string
    | { commandId: string; value: any; viewId?: number };

  type DocumentEvents = {
    document_size_changed: EventPayload<TwipsRect>;
    invalidate_visible_cursor: EventPayload<TwipsRect>;
    cursor_visible: EventPayload<boolean>;
    set_part: EventPayload<number>;
    ready: StateChangedValue[];

    // TODO: document these types
    hyperlink_clicked: any;
    text_selection_start: any;
    text_selection: any;
    text_selection_end: any;
    state_changed: EventPayload<StateChangedValue>;
    window: any;
  };

  type DocumentEventHandler<Event extends keyof DocumentEvents> = (
    arg: DocumentEvents[Event]
  ) => void;

  interface DocumentClient {
    /**
     * add an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    on<K extends keyof DocumentEvents = keyof DocumentEvents>(
      eventName: K,
      callback: DocumentEventHandler<K>
    ): void;

    /**
     * turn off an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    off<K extends keyof DocumentEvents = keyof DocumentEvents>(
      eventName: K,
      callback: DocumentEventHandler<K>
    ): void;

    /**
     * emit an event for an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    emit<K extends keyof DocumentEvents = keyof DocumentEvents>(
      eventName: K,
      callback: DocumentEventHandler<K>
    ): void;

    /**
     * posts a UNO command to the document
     * @param command - the uno command to be posted
     * @param args - arguments for the uno command
     */
    postUnoCommand(command: string, args?: { [name: string]: any }): void;

    /**
     * get the current parts name
     * @param partId - the id of the part you are in
     * @returns the parts name
     */
    getPartName(partId: number): string;

    /**
     * get the current parts hash
     * @param partId - the id of the part you are in
     * @returns the parts hash
     */
    getPartHash(partId: number): string;

    /**
     * posts a dialog event for the window with given id
     * @param windowId - the id of the window to notify
     * @param args - the arguments for the event
     */
    sendDialogEvent(windowId: number, args: string): void;

    /**
     * sets the start or end of a text selection
     * @param type - the text selection type
     * @param x - the horizontal position in document coordinates
     * @param y - the vertical position in document coordinates
     */
    setTextSelection(type: number, x: number, y: number): void;

    /**
     * gets the currently selected text
     * @param mimeType - the mime type for the selection
     * @returns result[0] is the text selection result[1] is the used mime type
     */
    getTextSelection(mimeType: string): string[];

    /**
     * gets the type of the selected content and possibly its text
     * @param mimeType - the mime type of the selection
     * @returns the selection type, the text of the selection and the used mime type
     */
    getSelectionTypeAndText(mimeType: string): {
      selectionType: number;
      text: string;
      usedMimeType: string;
    };

    /**
     * gets the content on the clipboard for the current view as a series of
     * binary streams
     * @param mimeTypes - the array of mimeTypes corresponding to each item in the clipboard
     * the mimeTypes should include the charset if you are going to pass them in for filtering the clipboard data ex.) text/plain;charset=utf-8
     * @returns an array of clipboard items
     */
    getClipboard(mimeTypes?: string[]): ClipboardItem[];

    /**
     * populates the clipboard for this view with multiple types of content
     * @param clipboardData - array of clipboard items used to populate the clipboard
     * for setting the clipboard data you will NOT want to include the charset in the mimeType. ex.) text/plain
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
    getCommandValues(command: string): {
      commandName: string;
      commandValues: { [name: string]: any };
    };

    /**
     * sets the cursor to a given outline node
     * @param id - the id of the node to go to
     * @returns the rect of the node where the cursor is brought to
     */
    gotoOutline(id: number): {
      destRect: string;
    };

    /**
     * show/hide a single row/column header outline for Calc documents
     * @param column - if we are dealingg with a column or row group
     * @param level - the level to which the group belongs
     * @param index - the group entry index
     * @param hidden - the new group state (collapsed/expanded)
     */
    setOutlineState(
      column: boolean,
      level: number,
      index: number,
      hidden: boolean
    ): void;

    /**
     * set the language tag of the window with the specified id
     * @param id - a view ID
     * @param language - Bcp47 languageTag, like en-US or so
     */
    setViewLanguage(id: number, language: string): void;

    /**
     * set a part's selection mode
     * @param part - the part you want to select
     * @param select - 0 to deselect, 1 to select, and 2 to toggle
     */
    selectPart(part: number, select: 0 | 1 | 2): void;

    /**
     * moves the selected pages/slides to a new position
     * @param position - the new position where the selection should go
     * @param duplicate - when true will copy instead of move
     */
    moveSelectedParts(position: number, duplicate: boolean): void;

    /**
     * for deleting many characters all at once
     * @param windowId - the window id to post the input event to.
     * If windowId is 0 the event is posted into the document
     * @param before - the characters to be deleted before the cursor position
     * @param after - the charactes to be deleted after teh cursor position
     */
    removeTextContext(windowId: number, before: number, after: number): void;

    /**
     * select the Calc function to be pasted into the formula input box
     * @param functionName - the function name to be completed
     */
    completeFunction(functionName: string): void;

    /**
     * posts an event for the form field at the cursor position
     * @param arguments - the arguments for the event
     */
    sendFormFieldEvent(arguments: string): void;

    /**
     * posts an event for the content control at the cursor position
     * @param arguments - the arguments for the event
     * @returns whether sending the event was successful
     */
    sendContentControlEvent(arguments: { [name: string]: any }): boolean;

    /**
     * if the document is ready
     * @returns {boolean}
     */
    isReady(): boolean;

    as: import('./lok_api').text.GenericTextDocument['as'];
  }

  interface OfficeClient {
    /**
     * add an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    on(eventName: string, callback: () => void): void;

    /**
     * turn off an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    off(eventName: string, callback: () => void): void;

    /**
     * emit an event for an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    emit(eventName: string, callback: () => void): void;

    /**
     * returns details of filter types
     * @returns the details of the filter types
     */
    getFilterTypes(): { [name: string]: { [name: string]: string } };

    /**
     * set password required for loading or editing a document
     * @param url - the URL of the document, as sent to the callback
     * @param password - the password, undefined indicates no password
     */
    setDocumentPassword(url: string, password?: string): void;

    /**
     * get version information of the LOKit process
     * @returns the version info in JSON format
     */
    getVersionInfo(): { [name: string]: any };

    /**
     * posts a dialog event for the window with given id
     * @param windowId - the id of the window to notify
     * @param args - the arguments for the event
     */
    sendDialogEvent(windowId: number, args: string): void;

    /**
     * loads a given document
     * @param path - the document path
     * @returns a handle to the document client
     */
    loadDocument(path: string): DocumentClient;

    /**
     * run a macro
     * @param url - the url for the macro (macro:// URI format)
     * @returns success
     */
    runMacro(url: string): boolean;

    api: typeof import('./lok_api');
  }
}
