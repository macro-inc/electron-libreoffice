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

/** resolves when the thread runner is idle */
declare function idle(): Promise<void>;
