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
     * @param {string} eventName - the name of the event
     * @param {() => void} callback - the callback function
     * @returns {void}
     */
    on(eventName: string, callback: () => void): void;

    /**
     * @description turn off an event listener
     * @param {string} eventName - the name of the event
     * @param {() => void} callback - the callback function
     * @returns {void}
     */
    off(eventName: string, callback: () => void): void;

    /**
     * @description emit an event for an event listener
     * @param {string} eventName - the name of the event
     * @param {() => void} callback - the callback function
     * @returns {void}
     */
    emit(eventName: string, callback: () => void): void;

    /**
     * @description converts twip to a css px
     * @param {number} input - twip
     * @returns {number} css px
     */
    twipToPx(input: number): number;

    /**
     * @description posts a UNO command to the document
     * @param {string} command - the uno command to be posted
     * @param [{[name:string]:any}] - arguments for the uno command
     * @returns {void}
     */
    postUnoCommand(command: string, args?: {[name: string]: any}): void;

    /**
     * @description get the current parts name
     * @param {number} partId - the id of the part you are in
     * @returns {string} the parts name
     */
    getPartName(partId: number): string;

    /**
     * @description get the current parts hash
     * @param {number} partId - the id of the part you are in
     * @returns {string} the parts hash
     */
    getPartHash(partId: number): string;

    /**
     * @description posts a dialog event for the window with given id
     * @param {number} windowId - the id of the window to notify
     * @param {string} args - the arguments for the event
     * @returns {void}
     */
    sendDialogEvent(windowId: number, args: string): void;

    /**
     * @description sets the start or end of a text selection
     * @param {number} type - the text selection type
     * @param {number} x - the horizontal position in document coordinates
     * @param {number} y - the vertical position in document coordinates
     * @returns {void}
     */
    setTextSelection(type: number, x: number, y: number): void;

    /**
     * @description gets the currently selected text
     * @param {string} mimeType - the mime type for the selection
     * @returns {string[]} result[0] is the text selection result[1] is the used mime type
     */
    getTextSelection(mimeType: string): string[];

    /**
     * @description gets the type of the selected content and possibly its text
     * @param {string} mimeType - the mime type of the selection
     * @return{{selectionType: number;text: string;usedMimeType: string;}} the selection type, the text of the selection and the used mime type
     */
    getSelectionTypeAndText(mimeType: string): {
      selectionType: number;
      text: string;
      usedMimeType: string;
    };

    /**
     * @description gets the content on the clipboard for the current view as a series of
     * binary streams
     * @param{string[]} mimeTypes - the array of mimeTypes corresponding to each item in the clipboard
     * @returns {ClipboardItem[]} an array of clipboard items
     */
    getClipboard(mimeTypes: string[]): ClipboardItem[];

    /**
     * @description populates the clipboard for this view with multiple types of content
     * @param {ClipboardItem[]} clipboardData - array of clipboard items used to populate the clipboard
     * @returns {boolean} whether the operation was successful
     */
    setClipboard(clipboardData: ClipboardItem[]): boolean;

    /**
     * @description pastes content at the current cursor position
     * @param {string} mimeType - the mime type of the data to paste
     * @param {string} data - the data to be pasted
     * @returns {boolean} if the paste was successful or not
     */
    paste(mimeType: string, data: string): boolean;

    /**
     * @description adjusts the graphic selection
     * @param {number} type - the graphical selection type
     * @param {number} x - the horizontal position in document coordinates
     * @param {number} y - the horizontal position in document coordinates
     * @returns {void}
     */
    setGraphicsSelection(type: number, x: number, y: number): void;

    /**
     * @description gets rid of any text or graphic selection
     * @returns {void}
     */
    resetSelection(): void;

    /**
     * @description returns a json mapping of the possible values for the given command
     * e.g. {commandName: ".uno:StyleApply", commandValues: {"familyName1" :
     * ["list of style names in the family1"], etc.}}
     * @param {string} command - the UNO command for which possible values are requested
     * @returns {{commandName: string, commandValues: {[name:string]:any}}} the command object with possible values
     */
    getCommandValues(command: string): {
      commandName: string;
      commandValues: {[name: string]: any};
    };

    /**
     * @description show/hide a single row/column header outline for Calc documents
     * @param {boolean} column - if we are dealingg with a column or row group
     * @param {number} level - the level to which the group belongs
     * @param {number} index - the group entry index
     * @param {boolean} hidden - the new group state (collapsed/expanded)
     * @returns {void}
     */
    setOutlineState(
      column: boolean,
      level: number,
      index: number,
      hidden: boolean,
    ): void;

    /**
     * @description set the language tag of the window with the specified id
     * @param {number} id - a view ID
     * @param {string} language - Bcp47 languageTag, like en-US or so
     * @returns {void}
     */
    setViewLanguage(id: number, language: string): void;

    /**
     * @description set a part's selection mode
     * @param {number} part - the part you want to select
     * @param {0|1|2} select - 0 to deselect, 1 to select, and 2 to toggle
     * @returns {void}
     */
    selectPart(part: number, select: 0 | 1 | 2): void;

    /**
     * @description moves the selected pages/slides to a new position
     * @param {number} position - the new position where the selection should go
     * @param {boolean} duplicate - when true will copy instead of move
     * @returns {void}
     */
    moveSelectedParts(position: number, duplicate: boolean): void;

    /**
     * @description for deleting many characters all at once
     * @param {number} windowId - the window id to post the input event to.
     * If windowId is 0 the event is posted into the document
     * @param {number} before - the characters to be deleted before the cursor position
     * @param {number} after - the charactes to be deleted after teh cursor position
     * @returns {void}
     */
    removeTextContext(windowId: number, before: number, after: number): void;

    /**
     * @description select the Calc function to be pasted into the formula input box
     * @param {string} functionName - the function name to be completed
     * @returns {void}
     */
    completeFunction(functionName: string): void;

    /**
     * @description posts an event for the form field at the cursor position
     * @param {string} arguments - the arguments for the event
     * @returns {void}
     */
    sendFormFieldEvent(arguments: string): void;

    /**
     * @description posts an event for the content control at the cursor position
     * @param {{[name:string]:any}} arguments - the arguments for the event
     * @returns {void}
     */
    sendContentControlEvent(arguments: {[name: string]: any}): void;

    /**
     * @description gets the page rect objects for the document
     * @returns {Rect[]} - the page rect objects
     */
    pageRects(): Rect[];

    /**
     * @description gets the document size
     * @returns {Size} - the size object of the document
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
     * @param {string} eventName - the name of the event
     * @param {() => void} callback - the callback function
     * @returns {void}
     */
    on(eventName: string, callback: () => void): void;

    /**
     * @description turn off an event listener
     * @param {string} eventName - the name of the event
     * @param {() => void} callback - the callback function
     * @returns {void}
     */
    off(eventName: string, callback: () => void): void;

    /**
     * @description emit an event for an event listener
     * @param {string} eventName - the name of the event
     * @param {() => void} callback - the callback function
     * @returns {void}
     */
    emit(eventName: string, callback: () => void): void;

    /**
     * @description returns details of filter types
     * @returns {{[name: string]: {[name: string]: string}}} the details of the filter types
     */
    getFilterTypes(): {[name: string]: {[name: string]: string}};

    /**
     * @description set password required for loading or editing a document
     * @param {string} url - the URL of the document, as sent to the callback
     * @param [string] password - the password, undefined indicates no password
     * @returns {void}
     */
    setDocumentPassword(url: string, password?: string): void;

    /**
     * @description get version information of the LOKit process
     * @returns {{[name:string]:any}} the version info in JSON format
     */
    getVersionInfo(): {[name: string]: any};

    /**
     * @description posts a dialog event for the window with given id
     * @param {number} windowId - the id of the window to notify
     * @param {string} args - the arguments for the event
     * @returns {void}
     */
    sendDialogEvent(windowId: number, args: string): void;

    /**
     * @description loads a given document
     * @param {string} path - the document path
     * @returns {DocumentClient} a handle to the document client
     */
    loadDocument(path: string): DocumentClient;

    /**
     * @description run a macro
     * @param {string} url - the url for the macro (macro:// URI format)
     * @returns {boolean} success
     */
    runMacro(url: string): boolean;
  }
}
