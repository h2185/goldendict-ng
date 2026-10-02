# Accessibility (screen readers and keyboard)

GoldenDict-ng is usable with NVDA, JAWS, Narrator (Windows), Orca (Linux) and VoiceOver (macOS) and without a mouse.

## Keyboard

| Key | Action |
| --- | --- |
| `Alt+D` or `Ctrl+L` | Jump to the word input field |
| `Down` (in the input field) | Move into the suggestion list |
| `Enter` | Look the word up / open the selected suggestion |
| `Ctrl+N` | Move focus to the article |
| `Alt+G` | Open the dictionary group list |
| `Alt+PgUp` / `Alt+PgDn` | Previous / next dictionary group |
| `Ctrl+F` | Search inside the article (`Ctrl+G` / `Ctrl+Shift+G` for next / previous) |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Next / previous article tab |
| `Tab` / `Shift+Tab` | Move through input field, group list, toolbar buttons, panes and article |
| `Left` / `Right` (on the dictionary bar) | Move between dictionary buttons; `Space` toggles one |
| `Menu` / `Shift+F10` (on the dictionary bar) | Context menu of the dictionary |

## Reading articles with the arrow keys (screen readers)

Qt WebEngine does not expose web pages to NVDA as a document, so NVDA's browse mode cannot be used inside the
article view.  Press `Ctrl+Shift+T` (View > Read Article as Text) to open the article in a plain text box that
can be read with the arrow keys.  It re-implements the most useful browse mode quick keys:

| Key | Action |
| --- | --- |
| `Up` / `Down` / `Left` / `Right`, `Ctrl+Left/Right` | Read by line, character, word |
| `H` / `Shift+H` | Next / previous heading (each dictionary name is a level 1 heading) |
| `1` ... `6` / `Shift+1` ... `Shift+6` | Next / previous heading of that level |
| `K` / `Shift+K` | Next / previous link (the link text is selected) |
| `Enter` | Open the link under the caret (`Shift+Enter` opens it in a new tab); the new article is shown again automatically |
| `F7` | List of all headings or links, with "Move to" and "Open link" |
| `Esc` | Close and return to the program |

By default every article you look up opens in this text view automatically (View > Open Results as Text
Automatically turns that on or off; the choice is saved).  `Esc` closes the view and returns to the search field.

The quick keys use the physical key, so they also work with a Persian or Arabic keyboard layout.

## What is announced

* Number of suggestions and "No results" while typing a word.
* "Article loaded: <title>" when a lookup finishes.
* Status bar messages (warnings, import results...).
* Match found / no matches in the in-article search.

## Notes for developers

* New icon-only buttons need an accessible name: use `A11y::setName()` or `A11y::nameFromToolTip()` from `src/common/a11y.hh`.
* Toolbar buttons are created by Qt with `Qt::NoFocus`; call `A11y::makeToolBarAccessible()` after filling a `QToolBar`.
* Never convey state with colour alone; also call `A11y::announce()`.
* In `.ui` files give every `QLabel` that describes an input a `buddy`.
* On Windows, `--force-renderer-accessibility` is passed to Chromium automatically when a screen reader is running.
