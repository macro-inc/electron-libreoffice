declare global {
  const libreoffice: LibreOffice.OfficeClient;
}

export namespace LibreOffice {
  type ClipboardItem = {
    mimeType: string;
    buffer: ArrayBuffer;
  };

  type Rect = {
    [name: string]: any;
  };

  type Size = {
    [name: string]: any;
  };

  export interface DocumentClient {
    /**
     * @description add an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    on(eventName: string, callback: () => void): void;

    /**
     * @description turn off an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    off(eventName: string, callback: () => void): void;

    /**
     * @description emit an event for an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    emit(eventName: string, callback: () => void): void;

    /**
     * @description converts twip to a css px
     * @param input - twip
     * @returns css px
     */
    twipToPx(input: number): number;

    /**
     * @description posts a UNO command to the document
     * @param command - the uno command to be posted
     * @param args - arguments for the uno command
     */
    postUnoCommand(command: string, args?: { [name: string]: any }): void;

    /**
     * @description get the current parts name
     * @param partId - the id of the part you are in
     * @returns the parts name
     */
    getPartName(partId: number): string;

    /**
     * @description get the current parts hash
     * @param partId - the id of the part you are in
     * @returns the parts hash
     */
    getPartHash(partId: number): string;

    /**
     * @description posts a dialog event for the window with given id
     * @param windowId - the id of the window to notify
     * @param args - the arguments for the event
     */
    sendDialogEvent(windowId: number, args: string): void;

    /**
     * @description sets the start or end of a text selection
     * @param type - the text selection type
     * @param x - the horizontal position in document coordinates
     * @param y - the vertical position in document coordinates
     */
    setTextSelection(type: number, x: number, y: number): void;

    /**
     * @description gets the currently selected text
     * @param mimeType - the mime type for the selection
     * @returns result[0] is the text selection result[1] is the used mime type
     */
    getTextSelection(mimeType: string): string[];

    /**
     * @description gets the type of the selected content and possibly its text
     * @param mimeType - the mime type of the selection
     * @returns the selection type, the text of the selection and the used mime type
     */
    getSelectionTypeAndText(mimeType: string): {
      selectionType: number;
      text: string;
      usedMimeType: string;
    };

    /**
     * @description gets the content on the clipboard for the current view as a series of
     * binary streams
     * @param mimeTypes - the array of mimeTypes corresponding to each item in the clipboard
     * the mimeTypes should include the charset if you are going to pass them in for filtering the clipboard data ex.) text/plain;charset=utf-8
     * @returns an array of clipboard items
     */
    getClipboard(mimeTypes?: string[]): ClipboardItem[];

    /**
     * @description populates the clipboard for this view with multiple types of content
     * @param clipboardData - array of clipboard items used to populate the clipboard
     * for setting the clipboard data you will NOT want to include the charset in the mimeType. ex.) text/plain
     * @returns whether the operation was successful
     */
    setClipboard(clipboardData: ClipboardItem[]): boolean;

    /**
     * @description pastes content at the current cursor position
     * @param mimeType - the mime type of the data to paste
     * @param data - the data to be pasted
     * @returns whether the paste was successful
     */
    paste(mimeType: string, data: string): boolean;

    /**
     * @description adjusts the graphic selection
     * @param type - the graphical selection type
     * @param x - the horizontal position in document coordinates
     * @param y - the horizontal position in document coordinates
     */
    setGraphicsSelection(type: number, x: number, y: number): void;

    /**
     * @description gets rid of any text or graphic selection
     */
    resetSelection(): void;

    /**
     * @description returns a json mapping of the possible values for the given command
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
     * @description sets the cursor to a given outline node
     * @param id - the id of the node to go to
     * @returns the rect of the node where the cursor is brought to
     */
    gotoOutline(id: number): {
      destRect: string;
    };

    /**
     * @description show/hide a single row/column header outline for Calc documents
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
     * @description set the language tag of the window with the specified id
     * @param id - a view ID
     * @param language - Bcp47 languageTag, like en-US or so
     */
    setViewLanguage(id: number, language: string): void;

    /**
     * @description set a part's selection mode
     * @param part - the part you want to select
     * @param select - 0 to deselect, 1 to select, and 2 to toggle
     */
    selectPart(part: number, select: 0 | 1 | 2): void;

    /**
     * @description moves the selected pages/slides to a new position
     * @param position - the new position where the selection should go
     * @param duplicate - when true will copy instead of move
     */
    moveSelectedParts(position: number, duplicate: boolean): void;

    /**
     * @description for deleting many characters all at once
     * @param windowId - the window id to post the input event to.
     * If windowId is 0 the event is posted into the document
     * @param before - the characters to be deleted before the cursor position
     * @param after - the charactes to be deleted after teh cursor position
     */
    removeTextContext(windowId: number, before: number, after: number): void;

    /**
     * @description select the Calc function to be pasted into the formula input box
     * @param functionName - the function name to be completed
     */
    completeFunction(functionName: string): void;

    /**
     * @description posts an event for the form field at the cursor position
     * @param arguments - the arguments for the event
     */
    sendFormFieldEvent(arguments: string): void;

    /**
     * @description posts an event for the content control at the cursor position
     * @param arguments - the arguments for the event
     * @returns whether sending the event was successful
     */
    sendContentControlEvent(arguments: { [name: string]: any }): boolean;

    /**
     * @description gets the page rect objects for the document
     * @returns the page rect objects
     */
    pageRects(): Rect[];

    /**
     * @description gets the document size
     * @returns the size object of the document
     */
    size(): Size;

    /**
     * @description if the document is ready
     * @returns {boolean}
     */
    isReady(): boolean;
  }

  export interface OfficeClient {
    /**
     * @description add an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    on(eventName: string, callback: () => void): void;

    /**
     * @description turn off an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    off(eventName: string, callback: () => void): void;

    /**
     * @description emit an event for an event listener
     * @param eventName - the name of the event
     * @param callback - the callback function
     */
    emit(eventName: string, callback: () => void): void;

    /**
     * @description returns details of filter types
     * @returns the details of the filter types
     */
    getFilterTypes(): { [name: string]: { [name: string]: string } };

    /**
     * @description set password required for loading or editing a document
     * @param url - the URL of the document, as sent to the callback
     * @param password - the password, undefined indicates no password
     */
    setDocumentPassword(url: string, password?: string): void;

    /**
     * @description get version information of the LOKit process
     * @returns the version info in JSON format
     */
    getVersionInfo(): { [name: string]: any };

    /**
     * @description posts a dialog event for the window with given id
     * @param windowId - the id of the window to notify
     * @param args - the arguments for the event
     */
    sendDialogEvent(windowId: number, args: string): void;

    /**
     * @description loads a given document
     * @param path - the document path
     * @returns a handle to the document client
     */
    loadDocument(path: string): DocumentClient;

    /**
     * @description run a macro
     * @param url - the url for the macro (macro:// URI format)
     * @returns success
     */
    runMacro(url: string): boolean;

    api: typeof import('./lok_api');
  }
}
