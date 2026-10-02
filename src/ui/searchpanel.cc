#include "common/a11y.hh"
#include "searchpanel.hh"
#include <QLabel>
#include <QVBoxLayout>

SearchPanel::SearchPanel( QWidget * parent ):
  QWidget( parent )
{
  lineEdit = new QLineEdit( this );

  close = new QPushButton( this );
  close->setIcon( QIcon( ":/icons/closetab.svg" ) );
  close->setToolTip( tr( "Close search bar" ) );
  A11y::setName( close, tr( "Close search bar" ), tr( "Closes the search bar and returns to the article" ) );
  A11y::setName( lineEdit, tr( "Find in article" ) );

  previous = new QPushButton( this );
  previous->setIcon( QIcon( ":/icons/previous.svg" ) );
  previous->setText( tr( "&Previous" ) );
  previous->setShortcut( QKeySequence( tr( "Ctrl+Shift+G" ) ) );

  next = new QPushButton( this );
  next->setIcon( QIcon( ":/icons/next.svg" ) );
  next->setText( tr( "&Next" ) );
  next->setShortcut( QKeySequence( tr( "Ctrl+G" ) ) );

  caseSensitive = new QCheckBox( this );
  caseSensitive->setText( tr( "&Case Sensitive" ) );

  auto * searchLabel = new QLabel( tr( "&Find:" ) );
  searchLabel->setBuddy( lineEdit );

  auto * editRow = new QHBoxLayout(); // parent will be set in layout->addLayout.
  editRow->addWidget( searchLabel );
  editRow->addWidget( lineEdit );
  editRow->addWidget( close );

  auto * buttonsRow = new QHBoxLayout();
  buttonsRow->addWidget( previous );
  buttonsRow->addWidget( next );
  buttonsRow->addWidget( caseSensitive );
  buttonsRow->addStretch();

  auto * layout = new QVBoxLayout( this );
  layout->addLayout( editRow );
  layout->addLayout( buttonsRow );
}
