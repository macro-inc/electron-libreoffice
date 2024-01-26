declare enum KeyEventType {
  Down,
  Up,
  Press,
}
declare enum MouseEventType {
  Down,
  Move,
  Up,
  Click,
}
declare enum MouseButton {
  Left,
  Middle,
  Right,
  Back,
  Forward,
}

// the majority of these mimic the interactions of OfficeWebPlugin with Chromium
declare function resizeEmbed(width: number, height: number): void;
declare function updateFocus(focused: boolean, fromScript?: boolean): void;
/** resolves when an invalidation event is emitted */
declare function invalidate(doc: LibreOffice.DocumentClient): Promise<void>;
/** resolves when the ready event is emitted after rendering */
declare function ready(doc: LibreOffice.DocumentClient): Promise<void>;

declare function getEmbed(): HTMLLibreOfficeEmbed;
declare function loadEmptyDoc(): Promise<LibreOffice.DocumentClient>;
declare function sendMouseEvent(
  type: MouseEventType,
  button: MouseButton,
  x: number,
  y: number,
  modifiers?: string
): void;
declare function sendKeyEvent(type: KeyEventType, key: string): void;
declare function canUndo(): boolean;
declare function canRedo(): boolean;

/** resolves when the thread runner is idle, allowing IO events to resolve first */
declare function idle(): Promise<void>;

/**
  returns a URL to be used for a temporary file
  @param extension - the extension for the temporary file name, ex: '.docx', '.pdf'
*/
declare function tempFileURL(extension: string): string;
/** returns true if a file exists, false otherwise */
declare function fileURLExists(): boolean;
/** resolves when the plugin paints */
declare function painted(): Promise<void>;
/** destroyes the current embed and replaces it with a new one */
declare function remountEmbed(): void;
