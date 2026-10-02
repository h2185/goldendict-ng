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
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

#define ATR( s ) QCoreApplication::translate( "ArticleTextDialog", s )

namespace {

/// Letter or digit of the physical key, independent of the keyboard layout (so H and K keep working with a
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

void readItems( const QJsonArray & array, std::vector< ArticleTextItem > & out, int textLength )
{
  for ( const auto & value : array ) {
    const QJsonObject o = value.toObject();
    ArticleTextItem item;
    item.start = o.value( "start" ).toInt();
    item.end   = o.value( "end" ).toInt();
    item.level = o.value( "level" ).toInt();
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

void ArticleTextEdit::jumpHeading( int direction, int level )
{
  const int pos                 = textCursor().selectionStart();
  const ArticleTextItem * target = nullptr;

  if ( direction > 0 ) {
    for ( const auto & h : headings ) {
      if ( h.start > pos && ( level == 0 || h.level == level ) ) {
        target = &h;
        break;
      }
    }
  }
  else {
    for ( auto it = headings.rbegin(); it != headings.rend(); ++it ) {
      if ( it->start < pos && ( level == 0 || it->level == level ) ) {
        target = &*it;
        break;
      }
    }
  }

  if ( !target ) {
    const QString what = level == 0 ? ATR( "heading" ) : ATR( "heading of level %1" ).arg( level );
    A11y::announce( this, direction > 0 ? ATR( "No next %1" ).arg( what ) : ATR( "No previous %1" ).arg( what ) );
    return;
  }

  goTo( *target, false );
  A11y::announce( this, ATR( "Heading level %1" ).arg( target->level ) );
}

void ArticleTextEdit::jumpLink( int direction )
{
  const int pos = textCursor().selectionStart();
  int found     = -1;

  if ( direction > 0 ) {
    for ( int i = 0; i < static_cast< int >( links.size() ); ++i ) {
      if ( links[ i ].start > pos ) {
        found = i;
        break;
      }
    }
  }
  else {
    for ( int i = static_cast< int >( links.size() ) - 1; i >= 0; --i ) {
      if ( links[ i ].start < pos ) {
        found = i;
        break;
      }
    }
  }

  if ( found < 0 ) {
    A11y::announce( this, direction > 0 ? ATR( "No next link" ) : ATR( "No previous link" ) );
    return;
  }

  // The link text is selected so that the screen reader speaks exactly the link.
  goTo( links[ found ], true );
  A11y::announce( this, ATR( "Link %1 of %2" ).arg( found + 1 ).arg( static_cast< int >( links.size() ) ) );
}

const ArticleTextItem * ArticleTextEdit::linkAtCaret() const
{
  const int pos = textCursor().selectionStart();
  for ( const auto & l : links ) {
    if ( pos >= l.start && pos < l.end ) {
      return &l;
    }
  }
  return nullptr;
}

void ArticleTextEdit::keyPressEvent( QKeyEvent * e )
{
  const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
  const bool plain                 = ( mods == Qt::NoModifier );
  const bool shifted               = ( mods == Qt::ShiftModifier );

  if ( plain || shifted ) {
    const int direction = shifted ? -1 : 1;
    const int k         = letterOrDigit( e );

    if ( k == 'H' ) {
      jumpHeading( direction, 0 );
      return;
    }
    if ( k == 'K' ) {
      jumpLink( direction );
      return;
    }
    if ( k >= '1' && k <= '6' ) {
      jumpHeading( direction, k - '0' );
      return;
    }
  }

  if ( plain && ( e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter ) ) {
    if ( const ArticleTextItem * link = linkAtCaret() ) {
      if ( followLink ) {
        followLink( QUrl( link->href ) );
      }
      return;
    }
  }

  if ( plain && e->key() == Qt::Key_F7 ) {
    if ( showElements ) {
      showElements();
    }
    return;
  }

  QPlainTextEdit::keyPressEvent( e );
}

// ---------------------------------------------------------------------------------------------------------------

ArticleTextDialog::ArticleTextDialog( const QString & title,
                                      const QString & structuredJson,
                                      const QString & plainText,
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
      readItems( root.value( "headings" ).toArray(), edit->headings, static_cast< int >( text.size() ) );
      readItems( root.value( "links" ).toArray(), edit->links, static_cast< int >( text.size() ) );
    }
  }

  // A read-only text box with a real caret: screen readers speak the line, word or character as the
  // user moves with the arrow keys, exactly like in a text document.
  edit->setReadOnly( true );
  edit->setTextInteractionFlags( Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard );
  edit->setLineWrapMode( QPlainTextEdit::WidgetWidth );
  edit->setPlainText( text );
  edit->moveCursor( QTextCursor::Start );

  A11y::setName( edit,
                 ATR( "Article text" ),
                 ATR( "Read with the arrow keys. H: next heading. K: next link. Enter: open link. "
                      "F7: list of elements. Escape: close. %1 headings, %2 links." )
                   .arg( static_cast< int >( edit->headings.size() ) )
                   .arg( static_cast< int >( edit->links.size() ) ) );

  auto * layout = new QVBoxLayout( this );
  layout->addWidget( edit );

  auto * buttons       = new QDialogButtonBox( this );
  auto * elementsButton = buttons->addButton( ATR( "&Elements list (F7)" ), QDialogButtonBox::ActionRole );
  buttons->addButton( QDialogButtonBox::Close );
  layout->addWidget( buttons );

  QObject::connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::close );
  QObject::connect( elementsButton, &QPushButton::clicked, this, [ this ]() {
    openElementsList();
  } );

  edit->followLink = [ this ]( const QUrl & url ) {
    const FollowLink callback = follow;
    if ( callback && callback( url ) ) {
      close();
    }
  };
  edit->showElements = [ this ]() {
    openElementsList();
  };

  edit->setFocus();
}

void ArticleTextDialog::openElementsList()
{
  auto * dlg = new QDialog( this );
  dlg->setAttribute( Qt::WA_DeleteOnClose );
  dlg->setWindowTitle( ATR( "Elements list" ) );
  dlg->resize( 560, 440 );

  auto * layout    = new QVBoxLayout( dlg );
  auto * typeLabel = new QLabel( ATR( "Type of &element:" ), dlg );
  auto * type      = new QComboBox( dlg );
  type->addItem( ATR( "Headings" ) );
  type->addItem( ATR( "Links" ) );
  typeLabel->setBuddy( type );

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

  auto currentItems = [ type, textEdit ]() -> const std::vector< ArticleTextItem > & {
    return type->currentIndex() == 1 ? textEdit->links : textEdit->headings;
  };

  auto refill = [ type, list, followButton, textEdit, currentItems ]() {
    list->clear();
    const bool isLinks = type->currentIndex() == 1;
    const auto & items = currentItems();
    const int caret    = textEdit->textCursor().selectionStart();
    int row            = -1;

    for ( int i = 0; i < static_cast< int >( items.size() ); ++i ) {
      const auto & item = items[ i ];
      list->addItem( isLinks ? item.text : ATR( "%1, level %2" ).arg( item.text ).arg( item.level ) );
      if ( row < 0 && item.start >= caret ) {
        row = i;
      }
    }
    followButton->setEnabled( isLinks );
    if ( list->count() > 0 ) {
      list->setCurrentRow( row >= 0 ? row : list->count() - 1 );
    }
  };

  auto moveTo = [ dlg, list, type, textEdit, currentItems ]() {
    const int row      = list->currentRow();
    const auto & items = currentItems();
    if ( row < 0 || row >= static_cast< int >( items.size() ) ) {
      return;
    }
    const ArticleTextItem item = items[ row ];
    dlg->accept();
    textEdit->goTo( item, type->currentIndex() == 1 );
    textEdit->setFocus();
  };

  auto openLink = [ dlg, list, type, textEdit ]() {
    const int row = list->currentRow();
    if ( type->currentIndex() != 1 || row < 0 || row >= static_cast< int >( textEdit->links.size() ) ) {
      return;
    }
    const QUrl url( textEdit->links[ row ].href );
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
  // Walks the DOM of the article and produces: the readable text (one block per line), the position of every
  // heading and of every link inside that text.  Offsets are UTF-16 code units, the same unit QString uses.
  return QStringLiteral( R"JS(
(function () {
  var text = '', headings = [], links = [], pre = 0;
  var SKIP = { SCRIPT: 1, STYLE: 1, NOSCRIPT: 1, TEMPLATE: 1, HEAD: 1, SELECT: 1, TEXTAREA: 1, INPUT: 1,
               CANVAS: 1, SVG: 1, AUDIO: 1, VIDEO: 1, OPTION: 1, IFRAME: 1 };

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
      if (alt && alt.trim()) add('[' + alt.trim() + ']');
      return;
    }
    if (tag === 'BR') { trimEnd(); if (text.length) text += '\n'; return; }

    var isCell = (d === 'table-cell');
    var block = tag === 'GD-DICT-HEADER' ||
                (d !== 'inline' && d.indexOf('inline') !== 0 && d !== 'contents' && !isCell && d.indexOf('ruby') !== 0);
    var level = /^H[1-6]$/.test(tag) ? +tag.charAt(1) : (tag === 'GD-DICT-HEADER' ? 1 : 0);
    var isLink = tag === 'A' && typeof el.href === 'string' && el.getAttribute('href') &&
                 el.href.indexOf('javascript:') !== 0;
    var isPre = (tag === 'PRE' || cs.whiteSpace.indexOf('pre') === 0);

    if (block) lineBreak(1);
    if (isCell && el.previousElementSibling) add(' | ');
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

    if (isLink || level) {
      var s = start;
      while (s < text.length && text.charAt(s) === ' ') s++;
      if (s >= text.length && isLink) {
        var lab = '[' + label(el) + ']';
        add(lab);
        s = text.length - lab.length;
      }
      trimEnd();
      var e = text.length;
      if (e > s) {
        if (level) headings.push({ start: s, end: e, level: level, text: text.slice(s, e) });
        if (isLink) links.push({ start: s, end: e, text: text.slice(s, e), href: el.href });
      }
    }

    if (block) lineBreak(el.classList && el.classList.contains('gdarticle') ? 2 : 1);
  }

  if (!document.body) return '';
  walk(document.body);
  return JSON.stringify({ text: text.replace(/\s+$/, ''), headings: headings, links: links });
})()
)JS" );
}
