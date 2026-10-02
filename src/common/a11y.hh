#pragma once

/// Small helpers that make the Qt widgets usable with screen readers (NVDA, JAWS, Narrator, Orca, VoiceOver)
/// and with the keyboard alone.  Header-only on purpose.

#include <QAbstractButton>
#include <QAccessible>
#include <QAction>
#include <QKeySequence>
#include <QPointer>
#include <QString>
#include <QToolBar>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

namespace A11y {

/// "&Save Article" -> "Save Article"; "Foo && Bar" -> "Foo & Bar"
inline QString stripMnemonic( QString text )
{
  text.replace( QStringLiteral( "&&" ), QStringLiteral( "\x01" ) );
  text.remove( QLatin1Char( '&' ) );
  text.replace( QLatin1Char( '\x01' ), QLatin1Char( '&' ) );
  return text.trimmed();
}

/// Give a widget a name (and optionally a longer description) that a screen reader will speak.
inline void setName( QWidget * widget, const QString & name, const QString & description = QString() )
{
  if ( !widget ) {
    return;
  }
  widget->setAccessibleName( stripMnemonic( name ) );
  if ( !description.isEmpty() ) {
    widget->setAccessibleDescription( description );
  }
}

/// Speak a message through the screen reader without moving focus.
/// Qt >= 6.8 has a real announcement event (UIA notification on Windows, which NVDA supports).
/// On older Qt an Alert event is raised on the widget, which most screen readers also speak.
inline void announce( QWidget * widget, const QString & message )
{
  if ( !widget || message.isEmpty() || !QAccessible::isActive() ) {
    return;
  }
#if QT_VERSION >= QT_VERSION_CHECK( 6, 8, 0 )
  QAccessibleAnnouncementEvent event( widget, message );
  event.setPoliteness( QAccessible::AnnouncementPoliteness::Polite );
  QAccessible::updateAccessibility( &event );
#else
  // Screen readers read the name asynchronously, so keep the message as the name for a moment.
  const QString previousName = widget->accessibleName();
  widget->setAccessibleName( message );
  QAccessibleEvent event( widget, QAccessible::Alert );
  QAccessible::updateAccessibility( &event );
  QPointer< QWidget > guard( widget );
  QTimer::singleShot( 1500, widget, [ guard, previousName, message ]() {
    if ( guard && guard->accessibleName() == message ) {
      guard->setAccessibleName( previousName );
    }
  } );
#endif
}

/// Describe the current state of a checkable tool button's action for the screen reader.
inline QString shortcutSuffix( const QAction * action )
{
  if ( !action || action->shortcut().isEmpty() ) {
    return QString();
  }
  return action->shortcut().toString( QKeySequence::NativeText );
}

/// Make every button of a QToolBar reachable with Tab and give it a spoken name.
/// Qt creates toolbar buttons with Qt::NoFocus, so a keyboard-only user can never reach them otherwise.
/// Call again after the toolbar's actions have changed.
inline void makeToolBarAccessible( QToolBar * toolBar )
{
  if ( !toolBar ) {
    return;
  }

  const auto buttons = toolBar->findChildren< QToolButton * >( QString(), Qt::FindDirectChildrenOnly );
  for ( QToolButton * button : buttons ) {
    // The extension ("»") button is created by Qt itself; leave it focusable too.
    button->setFocusPolicy( Qt::TabFocus );

    if ( QAction * action = button->defaultAction() ) {
      QString name = stripMnemonic( action->text() );
      if ( name.isEmpty() ) {
        name = stripMnemonic( action->toolTip() );
      }
      if ( !name.isEmpty() && button->accessibleName().isEmpty() ) {
        button->setAccessibleName( name );
      }

      const QString shortcut = shortcutSuffix( action );
      if ( !shortcut.isEmpty() && button->accessibleDescription().isEmpty() ) {
        button->setAccessibleDescription( shortcut );
      }
    }
  }
}

/// Make a standalone icon-only button speak its tooltip.
inline void nameFromToolTip( QAbstractButton * button )
{
  if ( button && button->accessibleName().isEmpty() ) {
    button->setAccessibleName( stripMnemonic( button->toolTip().isEmpty() ? button->text() : button->toolTip() ) );
  }
}

} // namespace A11y
