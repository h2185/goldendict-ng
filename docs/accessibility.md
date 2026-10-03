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
article view.  `Ctrl+Shift+T` (View > Read Article as Text) opens the article in a plain text box that can be read
with the arrow keys, and by default every article you look up opens there automatically (View > Open Results as
Text Automatically turns that on or off).  `Esc` closes the view and returns to the search field.

The text box re-implements the quick navigation keys of NVDA and JAWS.  Add `Shift` to move backwards.

| Key | Moves to the next |
| --- | --- |
| `D` | dictionary (every dictionary name is also a level 1 heading) |
| `H`, `1` ... `6` | heading, heading of that level |
| `K` | link (the link text is selected) |
| `M` | numbered meaning: a paragraph starting with a number such as `1:` or `2.` |
| `L`, `I` | list, list item |
| `T` | table |
| `G` | graphic (image with alternative text) |
| `Q` | block quote |
| `S` | separator |
| `B` | button |
| `Ctrl+Down` / `Ctrl+Up` | paragraph (the original line of the dictionary), selected and read completely |
| `Enter` | open the link at the caret; otherwise look up the word at the caret or the selected text |
| `F7` | list of the element types the article contains (dictionaries, headings, links, numbered items ...) |
| `F1` | help with this list |

Long lines are broken into lines of at most 80 characters (the text view does not wrap visually), because screen
readers would otherwise read a whole paragraph for every Up/Down key press.  View > Text View Line Length changes
the limit; "No line breaks" keeps every paragraph (for example every numbered meaning) on one single line.

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
* In `.ui` files give every `QLabel` that describes an input a `buddy`; an input that is described by a check box or a
  radio button needs an `accessibleName` of its own (for example "Collapse articles longer than, number of symbols").
* Checkable group boxes are reported to screen readers as check boxes (`A11y::installAccessibleFactory()`).
* On Windows, `--force-renderer-accessibility` is passed to Chromium automatically when a screen reader is running.
