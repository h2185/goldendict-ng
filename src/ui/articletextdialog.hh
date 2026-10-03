#pragma once

/// A screen reader friendly view of an article.
///
/// Qt WebEngine does not expose web pages to NVDA as a "document", so NVDA's browse mode (arrow keys, H for
/// headings, K for links ...) does not work in the article view.  This dialog shows the text of the article in a
/// standard Qt text box (which every screen reader can read with the arrow keys) and re-implements the browse mode
/// quick navigation keys of NVDA and JAWS on top of it.  Shift+key moves backwards.
///
///   D dictionary      H heading (1-6: heading of that level)   K link          M numbered meaning ("1:", "2.")
///   L list            I list item                              T table         G graphic
///   Q block quote     S separator                              B button
///   Ctrl+Down / Up    next / previous paragraph, read completely
///   Enter             open the link at the caret, or look up the word at the caret
///   F7                list of all elements         F1   help
///
/// The class does not use signals or slots on purpose (no Q_OBJECT), it only needs callbacks.

#include <QDialog>
#include <QPlainTextEdit>
#include <QString>
#include <QUrl>
#include <functional>
#include <vector>

/// The kinds of elements the user can jump between.
enum ArticleElementKind {
  ElementDictionary = 0,
  ElementHeading,
  ElementLink,
  ElementNumbered,
  ElementList,
  ElementListItem,
  ElementTable,
  ElementGraphic,
  ElementQuote,
  ElementSeparator,
  ElementButton,
  ElementKindCount
};

struct ArticleTextItem
{
  int start = 0;
  int end   = 0;
  int level = 0; // headings
  int count = 0; // lists: number of items, tables: number of rows
  int cols  = 0; // tables: number of columns
  QString text;
  QString href;  // links only
};

class ArticleTextEdit: public QPlainTextEdit
{
public:
  explicit ArticleTextEdit( QWidget * parent );

  std::vector< ArticleTextItem > elements[ ElementKindCount ];

  /// Logical lines of the article (what the dictionary shows as one line).  Independent of how lines are broken
  /// for display.
  std::vector< ArticleTextItem > paragraphs;

  std::function< void( const QUrl & ) > followLink;
  std::function< void( const QString & ) > lookupWord;
  std::function< void() > showElements;
  std::function< void() > showHelp;

  /// Move the caret to the item, optionally selecting it so that the screen reader speaks all of it.
  void goTo( const ArticleTextItem & item, bool selectItem );

protected:
  void keyPressEvent( QKeyEvent * event ) override;

private:
  void jumpElement( int kind, int direction, int level );
  void jumpParagraph( int direction );
  const ArticleTextItem * linkAtCaret() const;
  QString lookupCandidate() const;
};

class ArticleTextDialog: public QDialog
{
public:
  /// Called when the user opens a link.  Return true if the dialog should close afterwards (a new article is
  /// being loaded), false to stay in the text (audio links, external links ...).
  using FollowLink = std::function< bool( const QUrl & ) >;

  /// \param structuredJson  result of extractionScript(); an empty or invalid string falls back to plainText
  /// \param lineLength      longest line in characters; 0 keeps every paragraph on one line
  ArticleTextDialog( const QString & title,
                     const QString & structuredJson,
                     const QString & plainText,
                     int lineLength,
                     FollowLink followLink,
                     QWidget * parent );

  /// JavaScript that returns the readable text of the current page plus its structure as JSON.
  static QString extractionScript();

private:
  void openElementsList();
  void openHelp();

  ArticleTextEdit * edit;
  FollowLink follow;
};
