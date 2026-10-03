#include "articletextdialog.hh"
#include "common/a11y.hh"
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

#define ATR( s ) QCoreApplication::translate( "ArticleTextDialog", s )

namespace {

/// Describes one kind of element: the key that jumps to it (as in NVDA and JAWS where possible), its name,
/// whether the element is selected when the user jumps to it, and the JSON array it comes from.
struct KindInfo
{
  const char * json; // nullptr: computed here
  char letter;
  const char * singular;
  const char * plural;
  bool select;       // select the element so that the screen reader speaks all of it
};

const KindInfo kKinds[ ElementKindCount ] = {
  { "dicts", 'D', QT_TRANSLATE_NOOP( "ArticleTextDialog", "dictionary" ), QT_TRANSLATE_NOOP( "ArticleTextDialog", "Dictionaries" ), false },
  { "headings", 'H', QT_TRANSLATE_NOOP( "ArticleTextDialog", "heading" ), QT_TRANSLATE_NOOP( "ArticleTextDialog", "Headings" ), false },
  { "links", 'K', QT_TRANSLATE_NOOP( "ArticleTextDialog", "link" ), QT_TRANSLATE_NOOP( "ArticleTextDialog", "Links" ), true },
  { nullptr, 'M', QT_TRANSLATE_NOOP( "ArticleTextDialog", "numbered item" ), QT_TRANSLATE_NOOP( "ArticleTextDialog", "Numbered items" ), true },
  { "lists", 'L', QT_TRANSLATE_NOOP( "ArticleTextDialog", "list" ), QT_TRANSLATE_NOOP( "ArticleTextDialog", "Lists" ), false },
  { "listItems", 'I', QT_TRANSLATE_NOOP( "ArticleTextDialog", "list item" ), QT_TRANSLATE_NOOP( "ArticleTextDialog", "List items" ), true },
  { "tables", 'T', QT_TRANSLATE_NOOP( "ArticleTextDialog", "table" ), QT_TRANSLATE_NOOP( "ArticleTextDialog", "Tables" ), false },
  { "graphics", 'G', QT_TRANSLATE_NOOP( "ArticleTextDialog", "graphic" ), QT_TRANSLATE_NOOP( "ArticleTextDialog", "Graphics" ), true },
  { "quotes", 'Q', QT_TRANSLATE_NOOP( "ArticleTextDialog", "block quote" ), QT_TRANSLATE_NOOP( "ArticleTextDialog", "Block quotes" ), false },
  { "separators", 'S', QT_TRANSLATE_NOOP( "ArticleTextDialog", "separator" ), QT_TRANSLATE_NOOP( "ArticleTextDialog", "Separators" ), false },
  { "buttons", 'B', QT_TRANSLATE_NOOP( "ArticleTextDialog", "button" ), QT_TRANSLATE_NOOP( "ArticleTextDialog", "Buttons" ), true },
};

QString kindSingular( int kind )
{
  return QCoreApplication::translate( "ArticleTextDialog", kKinds[ kind ].singular );
}

QString kindPlural( int kind )
{
  return QCoreApplication::translate( "ArticleTextDialog", kKinds[ kind ].plural );
}

/// Letter or digit of the physical key, independent of the keyboard layout (so the quick keys keep working with a
/// Persian or Arabic layout).  Returns 0 for any other key.
int letterOrDigit( const QKeyEvent * e )
{
#ifdef Q_OS_WIN
  const quint32 vk = e->nativeVirtualKey();
  if ( ( vk >= 'A' && vk <= 'Z' ) || ( vk >= '0' && vk <= '9' ) ) {
    return static_cast< int >( vk );
  }
#endif
  const int key = e->key();
  if ( ( key >= Qt::Key_A && key <= Qt::Key_Z ) || ( key >= Qt::Key_0 && key <= Qt::Key_9 ) ) {
    return key;
  }
  return 0;
}

/// Breaks long lines into short ones by turning the space at the break point into a newline.
///
/// Screen readers (through Qt's accessibility layer) treat a whole paragraph as one "line", even when the text box
/// wraps it visually.  Moving with the Up and Down keys then reads the entire paragraph again and again.  With
/// real short lines every key press reads exactly one line.  A space is replaced by a newline one for one, so the
/// text keeps its length and the positions of all elements stay valid.
QString hardWrap( QString text, int width )
{
  int lineStart = 0;
  int lastSpace = -1;

  for ( int i = 0; i < text.size(); ++i ) {
    const QChar c = text.at( i );

    if ( c == QLatin1Char( '\n' ) ) {
      lineStart = i + 1;
      lastSpace = -1;
      continue;
    }

    if ( c == QLatin1Char( ' ' ) ) {
      if ( i - lineStart >= width ) {
        // A word longer than the width: break after it.
        text[ i ] = QLatin1Char( '\n' );
        lineStart = i + 1;
        lastSpace = -1;
      }
      else {
        lastSpace = i;
      }
      continue;
    }

    if ( i - lineStart >= width && lastSpace >= lineStart ) {
      text[ lastSpace ] = QLatin1Char( '\n' );
      lineStart         = lastSpace + 1;
      lastSpace         = -1;
    }
  }

  return text;
}

/// Splits the text into its logical lines and picks out those that start with a number.
void readParagraphs( const QString & text, std::vector< ArticleTextItem > & all, std::vector< ArticleTextItem > & numbered )
{
  static const QRegularExpression numberStart( R"(^\s*[\(\[]?\p{Nd}{1,3}\s*[.:)\]\-\x{2013}])",
                                                QRegularExpression::UseUnicodePropertiesOption );

  int start = 0;
  while ( start <= text.size() ) {
    int end = text.indexOf( QLatin1Char( '\n' ), start );
    if ( end < 0 ) {
      end = static_cast< int >( text.size() );
    }

    const QString line = text.mid( start, end - start );
    if ( !line.trimmed().isEmpty() ) {
      ArticleTextItem item;
      item.start = start;
      item.end   = end;
      item.text  = line.trimmed();
      all.push_back( item );
      if ( numberStart.match( line ).hasMatch() ) {
        numbered.push_back( item );
      }
    }
    start = end + 1;
  }
}

void readItems( const QJsonArray & array, std::vector< ArticleTextItem > & out, int textLength )
{
  for ( const auto & value : array ) {
    const QJsonObject o = value.toObject();
    ArticleTextItem item;
    item.start = o.value( "start" ).toInt();
    item.end   = o.value( "end" ).toInt();
    item.level = o.value( "level" ).toInt();
    item.count = o.value( "count" ).toInt();
    item.cols  = o.value( "cols" ).toInt();
    item.text  = o.value( "text" ).toString();
    item.href  = o.value( "href" ).toString();
    if ( item.start >= 0 && item.end > item.start && item.end <= textLength ) {
      out.push_back( item );
    }
  }
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------

ArticleTextEdit::ArticleTextEdit( QWidget * parent ):
  QPlainTextEdit( parent )
{
}

void ArticleTextEdit::goTo( const ArticleTextItem & item, bool selectItem )
{
  const int last = std::max( 0, document()->characterCount() - 1 );
  QTextCursor cursor = textCursor();
  cursor.setPosition( std::clamp( item.start, 0, last ) );
  if ( selectItem ) {
    cursor.setPosition( std::clamp( item.end, 0, last ), QTextCursor::KeepAnchor );
  }
  setTextCursor( cursor );
  ensureCursorVisible();
}

void ArticleTextEdit::jumpElement( int kind, int direction, int level )
{
  const auto & items = elements[ kind ];
  const int pos      = textCursor().selectionStart();
  int found          = -1;

  if ( direction > 0 ) {
    for ( int i = 0; i < static_cast< int >( items.size() ); ++i ) {
      if ( items[ i ].start > pos && ( level == 0 || items[ i ].level == level ) ) {
        found = i;
        break;
      }
    }
  }
  else {
    for ( int i = static_cast< int >( items.size() ) - 1; i >= 0; --i ) {
      if ( items[ i ].start < pos && ( level == 0 || items[ i ].level == level ) ) {
        found = i;
        break;
      }
    }
  }

  if ( found < 0 ) {
    QString what = kindSingular( kind );
    if ( level > 0 ) {
      what = ATR( "heading of level %1" ).arg( level );
    }
    A11y::announce( this, direction > 0 ? ATR( "No next %1" ).arg( what ) : ATR( "No previous %1" ).arg( what ) );
    return;
  }

  const ArticleTextItem & item = items[ found ];
  goTo( item, kKinds[ kind ].select );

  // Say what kind of element this is (the screen reader itself speaks the text at the caret or the selection).
  const int total = static_cast< int >( items.size() );
  QString info;
  switch ( kind ) {
    case ElementHeading:
      info = ATR( "Heading level %1" ).arg( item.level );
      break;
    case ElementLink:
      info = ATR( "Link %1 of %2" ).arg( found + 1 ).arg( total );
      break;
    case ElementDictionary:
      info = ATR( "Dictionary %1 of %2" ).arg( found + 1 ).arg( total );
      break;
    case ElementList:
      info = ATR( "List with %1 items" ).arg( item.count );
      break;
    case ElementTable:
      info = ATR( "Table with %1 rows and %2 columns" ).arg( item.count ).arg( item.cols );
      break;
    case ElementListItem:
      info = ATR( "List item" );
      break;
    case ElementQuote:
      info = ATR( "Block quote" );
      break;
    case ElementSeparator:
      info = ATR( "Separator" );
      break;
    case ElementGraphic:
      info = ATR( "Graphic" );
      break;
    case ElementButton:
      info = ATR( "Button" );
      break;
    default:
      break;
  }
  if ( !info.isEmpty() ) {
    A11y::announce( this, info );
  }
}

void ArticleTextEdit::jumpParagraph( int direction )
{
  const int pos                  = textCursor().selectionStart();
  const ArticleTextItem * target = nullptr;

  if ( direction > 0 ) {
    for ( const auto & item : paragraphs ) {
      if ( item.start > pos ) {
        target = &item;
        break;
      }
    }
  }
  else {
    for ( auto it = paragraphs.rbegin(); it != paragraphs.rend(); ++it ) {
      if ( it->start < pos ) {
        target = &*it;
        break;
      }
    }
  }

  if ( !target ) {
    A11y::announce( this, direction > 0 ? ATR( "No next paragraph" ) : ATR( "No previous paragraph" ) );
    return;
  }

  // The whole paragraph is selected so that the screen reader speaks all of it, however it is broken into lines.
  goTo( *target, true );
}

const ArticleTextItem * ArticleTextEdit::linkAtCaret() const
{
  const int pos = textCursor().selectionStart();
  for ( const auto & l : elements[ ElementLink ] ) {
    if ( pos >= l.start && pos < l.end ) {
      return &l;
    }
  }
  return nullptr;
}

QString ArticleTextEdit::lookupCandidate() const
{
  QTextCursor cursor = textCursor();

  if ( cursor.hasSelection() ) {
    const QString selected = cursor.selectedText().trimmed();
    if ( !selected.isEmpty() && selected.size() <= 60 && !selected.contains( QChar::ParagraphSeparator ) ) {
      return selected;
    }
    // A long selection (a whole paragraph after a jump): use the word where it starts.
    cursor.setPosition( cursor.selectionStart() );
  }

  cursor.select( QTextCursor::WordUnderCursor );
  return cursor.selectedText().trimmed();
}

void ArticleTextEdit::keyPressEvent( QKeyEvent * e )
{
  const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
  const bool plain                 = ( mods == Qt::NoModifier );
  const bool shifted               = ( mods == Qt::ShiftModifier );

  if ( plain || shifted ) {
    const int direction = shifted ? -1 : 1;
    const int k         = letterOrDigit( e );

    if ( k >= '1' && k <= '6' ) {
      jumpElement( ElementHeading, direction, k - '0' );
      return;
    }
    if ( k != 0 ) {
      for ( int kind = 0; kind < ElementKindCount; ++kind ) {
        if ( kKinds[ kind ].letter == k ) {
          jumpElement( kind, direction, 0 );
          return;
        }
      }
    }
  }

  if ( mods == Qt::ControlModifier && ( e->key() == Qt::Key_Down || e->key() == Qt::Key_Up ) ) {
    jumpParagraph( e->key() == Qt::Key_Down ? 1 : -1 );
    return;
  }

  if ( ( plain || mods == Qt::ControlModifier ) && ( e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter ) ) {
    if ( const ArticleTextItem * link = linkAtCaret() ) {
      if ( followLink ) {
        followLink( QUrl( link->href ) );
      }
      return;
    }

    const QString word = lookupCandidate();
    if ( word.isEmpty() ) {
      A11y::announce( this, ATR( "No word at the caret" ) );
    }
    else if ( lookupWord ) {
      lookupWord( word );
    }
    return;
  }

  if ( plain && e->key() == Qt::Key_F7 ) {
    if ( showElements ) {
      showElements();
    }
    return;
  }

  if ( plain && e->key() == Qt::Key_F1 ) {
    if ( showHelp ) {
      showHelp();
    }
    return;
  }

  QPlainTextEdit::keyPressEvent( e );
}

// ---------------------------------------------------------------------------------------------------------------

ArticleTextDialog::ArticleTextDialog( const QString & title,
                                      const QString & structuredJson,
                                      const QString & plainText,
                                      int lineLength,
                                      FollowLink followLink,
                                      QWidget * parent ):
  QDialog( parent ),
  edit( new ArticleTextEdit( this ) ),
  follow( std::move( followLink ) )
{
  setAttribute( Qt::WA_DeleteOnClose );
  setWindowTitle( title );
  resize( 780, 580 );

  QString text = plainText;
  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson( structuredJson.toUtf8(), &error );
  if ( error.error == QJsonParseError::NoError && doc.isObject() ) {
    const QJsonObject root = doc.object();
    const QString t        = root.value( "text" ).toString();
    if ( !t.trimmed().isEmpty() ) {
      text = t;
      for ( int kind = 0; kind < ElementKindCount; ++kind ) {
        if ( kKinds[ kind ].json ) {
          readItems( root.value( QLatin1String( kKinds[ kind ].json ) ).toArray(),
                     edit->elements[ kind ],
                     static_cast< int >( text.size() ) );
        }
      }
    }
  }

  readParagraphs( text, edit->paragraphs, edit->elements[ ElementNumbered ] );

  // A read-only text box with a real caret: screen readers speak the line, word or character as the
  // user moves with the arrow keys, exactly like in a text document.
  edit->setReadOnly( true );
  edit->setTextInteractionFlags( Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard );
  // No visual wrapping: every line is one block, so a screen reader reads exactly one line per key press.
  // Long paragraphs are broken into short lines beforehand (see hardWrap), unless the user asked for none.
  edit->setLineWrapMode( QPlainTextEdit::NoWrap );
  edit->setPlainText( lineLength > 0 ? hardWrap( text, lineLength ) : text );
  edit->moveCursor( QTextCursor::Start );

  A11y::setName( edit,
                 ATR( "Article text" ),
                 ATR( "Read with the arrow keys. Press F1 for the list of quick navigation keys. "
                      "%1 dictionaries, %2 headings, %3 links." )
                   .arg( static_cast< int >( edit->elements[ ElementDictionary ].size() ) )
                   .arg( static_cast< int >( edit->elements[ ElementHeading ].size() ) )
                   .arg( static_cast< int >( edit->elements[ ElementLink ].size() ) ) );

  auto * layout = new QVBoxLayout( this );
  layout->addWidget( edit );

  auto * buttons        = new QDialogButtonBox( this );
  auto * elementsButton = buttons->addButton( ATR( "&Elements list (F7)" ), QDialogButtonBox::ActionRole );
  auto * helpButton     = buttons->addButton( ATR( "Quick &keys help (F1)" ), QDialogButtonBox::ActionRole );
  buttons->addButton( QDialogButtonBox::Close );
  layout->addWidget( buttons );

  QObject::connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::close );
  QObject::connect( elementsButton, &QPushButton::clicked, this, [ this ]() {
    openElementsList();
  } );
  QObject::connect( helpButton, &QPushButton::clicked, this, [ this ]() {
    openHelp();
  } );

  edit->followLink = [ this ]( const QUrl & url ) {
    const FollowLink callback = follow;
    if ( callback && callback( url ) ) {
      close();
    }
  };
  edit->lookupWord = [ this ]( const QString & word ) {
    QUrl url;
    url.setScheme( QStringLiteral( "bword" ) );
    url.setPath( word );
    const FollowLink callback = follow;
    if ( callback && callback( url ) ) {
      close();
    }
  };
  edit->showElements = [ this ]() {
    openElementsList();
  };
  edit->showHelp = [ this ]() {
    openHelp();
  };

  edit->setFocus();
}

void ArticleTextDialog::openHelp()
{
  auto * dlg = new QDialog( this );
  dlg->setAttribute( Qt::WA_DeleteOnClose );
  dlg->setWindowTitle( ATR( "Quick navigation keys" ) );
  dlg->resize( 640, 460 );

  auto * layout = new QVBoxLayout( dlg );
  auto * help   = new QPlainTextEdit( dlg );
  help->setReadOnly( true );
  help->setTextInteractionFlags( Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard );
  help->setLineWrapMode( QPlainTextEdit::NoWrap );
  A11y::setName( help, ATR( "Quick navigation keys" ) );
  help->setPlainText( ATR( "Add Shift to any of these letters to move backwards.\n"
                           "D: next dictionary.\n"
                           "H: next heading. 1 to 6: next heading of that level.\n"
                           "K: next link.\n"
                           "M: next numbered meaning, a paragraph starting with 1: or 2. and so on.\n"
                           "L: next list. I: next list item.\n"
                           "T: next table.\n"
                           "G: next graphic.\n"
                           "Q: next block quote.\n"
                           "S: next separator.\n"
                           "B: next button.\n"
                           "Control+Down and Control+Up: next and previous paragraph, read completely.\n"
                           "Enter: open the link at the caret, otherwise look up the word at the caret or the "
                           "selected text.\n"
                           "F7: list of elements, with the types of elements this article contains.\n"
                           "F1: this help.\n"
                           "Escape: close." ) );
  layout->addWidget( help );

  auto * buttons = new QDialogButtonBox( QDialogButtonBox::Close, dlg );
  QObject::connect( buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close );
  layout->addWidget( buttons );

  dlg->show();
  help->setFocus();
}

void ArticleTextDialog::openElementsList()
{
  auto * dlg = new QDialog( this );
  dlg->setAttribute( Qt::WA_DeleteOnClose );
  dlg->setWindowTitle( ATR( "Elements list" ) );
  dlg->resize( 580, 460 );

  auto * layout    = new QVBoxLayout( dlg );
  auto * typeLabel = new QLabel( ATR( "Type of &element:" ), dlg );
  auto * type      = new QComboBox( dlg );
  typeLabel->setBuddy( type );

  // Only the types this article really contains are offered, each with its number of elements.
  for ( int kind = 0; kind < ElementKindCount; ++kind ) {
    const int count = static_cast< int >( edit->elements[ kind ].size() );
    if ( count > 0 ) {
      type->addItem( ATR( "%1 (%2)" ).arg( kindPlural( kind ) ).arg( count ), kind );
    }
  }

  auto * list = new QListWidget( dlg );
  A11y::setName( list, ATR( "Elements" ) );

  layout->addWidget( typeLabel );
  layout->addWidget( type );
  layout->addWidget( list );

  auto * buttons      = new QDialogButtonBox( dlg );
  auto * goButton     = buttons->addButton( ATR( "&Move to" ), QDialogButtonBox::ActionRole );
  auto * followButton = buttons->addButton( ATR( "&Open link" ), QDialogButtonBox::ActionRole );
  buttons->addButton( QDialogButtonBox::Cancel );
  layout->addWidget( buttons );

  ArticleTextEdit * textEdit = edit;

  auto currentKind = [ type ]() -> int {
    return type->currentIndex() >= 0 ? type->currentData().toInt() : -1;
  };

  auto refill = [ list, followButton, textEdit, currentKind ]() {
    list->clear();
    const int kind = currentKind();
    if ( kind < 0 ) {
      return;
    }

    const auto & items = textEdit->elements[ kind ];
    const int caret    = textEdit->textCursor().selectionStart();
    int row            = -1;

    for ( int i = 0; i < static_cast< int >( items.size() ); ++i ) {
      const auto & item = items[ i ];
      QString label     = item.text;
      if ( kind == ElementHeading ) {
        label = ATR( "%1, level %2" ).arg( item.text ).arg( item.level );
      }
      else if ( kind == ElementList ) {
        label = ATR( "List with %1 items: %2" ).arg( item.count ).arg( item.text );
      }
      else if ( kind == ElementTable ) {
        label = ATR( "Table with %1 rows and %2 columns: %3" ).arg( item.count ).arg( item.cols ).arg( item.text );
      }
      list->addItem( label );
      if ( row < 0 && item.start >= caret ) {
        row = i;
      }
    }
    followButton->setEnabled( kind == ElementLink );
    if ( list->count() > 0 ) {
      list->setCurrentRow( row >= 0 ? row : list->count() - 1 );
    }
  };

  auto moveTo = [ dlg, list, textEdit, currentKind ]() {
    const int kind = currentKind();
    const int row  = list->currentRow();
    if ( kind < 0 || row < 0 || row >= static_cast< int >( textEdit->elements[ kind ].size() ) ) {
      return;
    }
    const ArticleTextItem item = textEdit->elements[ kind ][ row ];
    dlg->accept();
    textEdit->goTo( item, kKinds[ kind ].select );
    textEdit->setFocus();
  };

  auto openLink = [ dlg, list, textEdit, currentKind ]() {
    const int row = list->currentRow();
    if ( currentKind() != ElementLink || row < 0 || row >= static_cast< int >( textEdit->elements[ ElementLink ].size() ) ) {
      return;
    }
    const QUrl url( textEdit->elements[ ElementLink ][ row ].href );
    dlg->accept();
    if ( textEdit->followLink ) {
      textEdit->followLink( url );
    }
  };

  QObject::connect( type, &QComboBox::currentIndexChanged, dlg, [ refill ]( int ) {
    refill();
  } );
  QObject::connect( goButton, &QPushButton::clicked, dlg, moveTo );
  QObject::connect( followButton, &QPushButton::clicked, dlg, openLink );
  QObject::connect( list, &QListWidget::itemActivated, dlg, [ moveTo ]( QListWidgetItem * ) {
    moveTo();
  } );
  QObject::connect( buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject );

  refill();
  dlg->show();
  list->setFocus();
}

// ---------------------------------------------------------------------------------------------------------------

QString ArticleTextDialog::extractionScript()
{
  // Walks the DOM of the article and produces: the readable text (one block per line) and the position of every
  // element of interest (dictionaries, headings, links, lists, list items, tables, graphics, quotes, separators and
  // buttons) inside that text.  Offsets are UTF-16 code units, the same unit QString uses.
  return QStringLiteral( R"JS(
(function () {
  var text = '', pre = 0;
  var R = { dicts: [], headings: [], links: [], lists: [], listItems: [], tables: [], graphics: [], quotes: [],
            separators: [], buttons: [] };
  var SKIP = { SCRIPT: 1, STYLE: 1, NOSCRIPT: 1, TEMPLATE: 1, HEAD: 1, SELECT: 1, TEXTAREA: 1, CANVAS: 1,
               SVG: 1, AUDIO: 1, VIDEO: 1, OPTION: 1, IFRAME: 1 };

  function trimEnd() {
    var i = text.length;
    while (i > 0 && (text.charAt(i - 1) === ' ' || text.charAt(i - 1) === '\t')) i--;
    if (i < text.length) text = text.slice(0, i);
  }
  function lineBreak(n) {
    trimEnd();
    if (!text.length) return;
    var m = 0, i = text.length;
    while (i > 0 && text.charAt(i - 1) === '\n') { i--; m++; }
    while (m < n) { text += '\n'; m++; }
  }
  function add(s) {
    if (!pre) {
      s = s.replace(/[ \t\r\n\f\u00a0]+/g, ' ');
      if (!s) return;
      if (s.charAt(0) === ' ') {
        var c = text.charAt(text.length - 1);
        if (!c || c === ' ' || c === '\n') s = s.slice(1);
      }
    } else {
      s = s.replace(/\r/g, '');
    }
    text += s;
  }
  function record(kind, s, e, full, extra) {
    if (e <= s) return null;
    var o = { start: s, end: e, text: text.slice(s, full ? e : Math.min(e, s + 120)) };
    for (var k in extra) o[k] = extra[k];
    R[kind].push(o);
    return o;
  }
  function label(a) {
    var t = a.getAttribute('aria-label') || a.getAttribute('title') || '';
    if (!t) {
      var im = a.querySelector('img');
      if (im) t = im.getAttribute('alt') || im.getAttribute('title') || '';
    }
    return (t && t.trim()) || 'link';
  }

  function walk(el) {
    var tag = (el.tagName || '').toUpperCase();
    if (SKIP[tag]) return;
    var cs = getComputedStyle(el), d = cs.display;
    var isBody = el.classList && el.classList.contains('gdarticlebody');
    if (d === 'none' && !isBody) return;
    if (cs.visibility === 'hidden') return;

    if (tag === 'IMG') {
      var alt = el.getAttribute('alt');
      if (alt && alt.trim()) {
        var g = '[' + alt.trim() + ']';
        add(g);
        record('graphics', text.length - g.length, text.length, true, {});
      }
      return;
    }
    if (tag === 'BR') { trimEnd(); if (text.length) text += '\n'; return; }
    if (tag === 'HR') {
      lineBreak(1);
      add('-----');
      record('separators', text.length - 5, text.length, true, {});
      lineBreak(1);
      return;
    }
    if (tag === 'INPUT') {
      var ty = (el.getAttribute('type') || '').toLowerCase();
      if (ty === 'button' || ty === 'submit' || ty === 'reset') {
        var b = '[' + (el.value || el.getAttribute('aria-label') || 'button') + ']';
        lineBreak(1);
        add(b);
        record('buttons', text.length - b.length, text.length, true, {});
      }
      return;
    }

    var isCell = (d === 'table-cell');
    var block = tag === 'GD-DICT-HEADER' ||
                (d !== 'inline' && d.indexOf('inline') !== 0 && d !== 'contents' && !isCell && d.indexOf('ruby') !== 0);
    var level = /^H[1-6]$/.test(tag) ? +tag.charAt(1) : (tag === 'GD-DICT-HEADER' ? 1 : 0);
    var isLink = tag === 'A' && typeof el.href === 'string' && el.getAttribute('href') &&
                 el.href.indexOf('javascript:') !== 0;
    var isButton = tag === 'BUTTON' || el.getAttribute('role') === 'button';
    var isPre = (tag === 'PRE' || cs.whiteSpace.indexOf('pre') === 0);

    if (block) lineBreak(1);
    if (isCell && el.previousElementSibling) add(' | ');
    var itemStart = text.length;
    if (tag === 'LI') {
      var p = el.parentNode;
      if (p && p.tagName === 'OL') add((Array.prototype.indexOf.call(p.children, el) + 1) + '. ');
      else add('\u2022 ');
    }

    var start = text.length;
    if (isPre) pre++;
    for (var n = el.firstChild; n; n = n.nextSibling) {
      if (n.nodeType === 3) add(n.nodeValue);
      else if (n.nodeType === 1) walk(n);
    }
    if (isPre) pre--;

    // End of the element's own text, without trailing white space (the text itself is left untouched, a space
    // after an inline element still separates it from the next word).
    var end = text.length;
    while (end > start && /[ \t\n]/.test(text.charAt(end - 1))) end--;

    if (isLink || isButton || level) {
      var s = start;
      while (s < end && text.charAt(s) === ' ') s++;
      if (s >= end && (isLink || isButton)) {
        var lab = '[' + label(el) + ']';
        add(lab);
        end = text.length;
        s = end - lab.length;
      }
      if (level) record('headings', s, end, true, { level: level });
      if (isLink) record('links', s, end, true, { href: el.href });
      if (isButton) record('buttons', s, end, true, {});
      if (tag === 'GD-DICT-HEADER') {
        var t = el.querySelector('.gddicttitle');
        var o = record('dicts', s, end, true, {});
        if (o && t && t.textContent.trim()) o.text = t.textContent.trim();
      }
    }

    if (tag === 'LI') record('listItems', itemStart, end, false, {});
    if (tag === 'UL' || tag === 'OL') {
      var items = 0;
      for (var c = el.firstElementChild; c; c = c.nextElementSibling) if (c.tagName === 'LI') items++;
      record('lists', start, end, false, { count: items });
    }
    if (tag === 'BLOCKQUOTE') record('quotes', start, end, false, {});
    if (tag === 'TABLE' && el.rows && el.rows.length > 1 && el.rows[0].cells.length > 1) {
      record('tables', start, end, false, { count: el.rows.length, cols: el.rows[0].cells.length });
    }

    if (block) lineBreak(el.classList && el.classList.contains('gdarticle') ? 2 : 1);
  }

  if (!document.body) return '';
  walk(document.body);
  R.text = text.replace(/\s+$/, '');
  return JSON.stringify(R);
})()
)JS" );
}
