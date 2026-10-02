#pragma once

/// A screen reader friendly view of an article.
///
/// Qt WebEngine does not expose web pages to NVDA as a "document", so NVDA's browse mode (arrow keys, H for
/// headings, K for links ...) does not work in the article view.  This dialog shows the text of the article in a
/// standard Qt text box (which every screen reader can read with the arrow keys) and re-implements the most
/// important browse mode quick keys on top of it:
///
///   H / Shift+H      next / previous heading          1 .. 6   next heading of that level
///   K / Shift+K      next / previous link              Enter    open the link under the caret
///   F7               list of all headings and links
///
/// The class does not use signals or slots on purpose (no Q_OBJECT), it only needs callbacks.

#include <QDialog>
#include <QPlainTextEdit>
#include <QString>
#include <QUrl>
#include <functional>
#include <vector>

struct ArticleTextItem
{
  int start = 0;
  int end   = 0;
  int level = 0;  // headings only
  QString text;
  QString href;   // links only
};

class ArticleTextEdit: public QPlainTextEdit
{
public:
  explicit ArticleTextEdit( QWidget * parent );

  std::vector< ArticleTextItem > headings;
  std::vector< ArticleTextItem > links;

  std::function< void( const QUrl & ) > followLink;
  std::function< void() > showElements;

  /// Move the caret to the item; links are selected so the screen reader speaks them.
  void goTo( const ArticleTextItem & item, bool selectItem );

protected:
  void keyPressEvent( QKeyEvent * event ) override;

private:
  void jumpHeading( int direction, int level );
  void jumpLink( int direction );
  const ArticleTextItem * linkAtCaret() const;
};

class ArticleTextDialog: public QDialog
{
public:
  /// Called when the user opens a link.  Return true if the dialog should close afterwards (a new article is
  /// being loaded), false to stay in the text (audio links, external links ...).
  using FollowLink = std::function< bool( const QUrl & ) >;

  /// \param structuredJson  result of extractionScript(); an empty or invalid string falls back to plainText
  ArticleTextDialog( const QString & title,
                     const QString & structuredJson,
                     const QString & plainText,
                     FollowLink followLink,
                     QWidget * parent );

  /// JavaScript that returns the readable text of the current page plus its headings and links as JSON.
  static QString extractionScript();

private:
  void openElementsList();

  ArticleTextEdit * edit;
  FollowLink follow;
};
