# TextInput demo

A single-line text field driven by the platform IME through the `TextInput`
singleton.

## Run

1. Copy `Game.exe` from the build tree into this directory:
   `build/win64-vendored-ruby/app/RelWithDebInfo/Game.exe`
2. Run `run.bat` (it packs `main.rb` into `Data/Scripts.rxdata` with the system
   Ruby, which must provide `zlib`).

The window shows the committed text and the current IME preedit at the caret.
`Enter` commits the line, `Backspace`/`Delete` edit it, the arrow keys and
`Home`/`End` move the caret, and `Escape` leaves.

`TextInput.text` accumulates committed text until `TextInput.clear`; the demo
pumps it into its own buffer every frame so the caret can be edited. The IME
candidate area is set to the input box in window coordinates.
