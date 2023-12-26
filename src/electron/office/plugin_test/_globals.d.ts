declare enum KeyEventType {
  Down,
  Up,
  Press
}
declare enum MouseEventType {
  Down,
  Move,
  Up,
  Click
}
declare enum MouseButton {
  Left,
  Middle,
  Right,
  Back,
  Forward
}

declare function waitForInvalidate(): Promise<void>; 
declare function getEmbed(): HTMLLibreOfficeEmbed; 
declare function loadEmptyDoc(): Promise<LibreOffice.DocumentClient>;
declare function sendMouseEvent(type: MouseEventType, button: MouseButton, x: number, y: number, modifiers?: string): void;
declare function sendKeyEvent(type: KeyEventType, key: string): void;
