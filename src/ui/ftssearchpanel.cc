#include "common/a11y.hh"
#include "ftssearchpanel.hh"
#include <QHBoxLayout>

FtsSearchPanel::FtsSearchPanel( QWidget * parent ):
  QWidget( parent )
{

  auto * layout = new QHBoxLayout( this );
  previous      = new QPushButton( this );
  next          = new QPushButton( this );
  statusLabel   = new QLabel( this );

  layout->addWidget( previous );
  layout->addWidget( next );
  layout->addWidget( statusLabel );

  previous->setIcon( QIcon( ":/icons/previous.svg" ) );
  next->setIcon( QIcon( ":/icons/next.svg" ) );

  previous->setText( tr( "&Previous" ) );
  next->setText( tr( "&Next" ) );

  // The status label changes while the user searches; let screen readers find it by name.
  A11y::setName( statusLabel, tr( "Full-text search status" ) );

  layout->addStretch();
}
