/*
 * This file is part of Licq, an instant messaging client for UNIX.
 * Copyright (C) 1999-2012 Licq developers <licq-dev@googlegroups.com>
 *
 * Licq is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Licq is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Licq; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "userviewbase.h"

#include <boost/foreach.hpp>

#include <licq/contactlist/usermanager.h>

#include "contactlist/contactlist.h"

#include "config/contactlist.h"
#include "config/skin.h"

#include "core/groupmenu.h"
#include "core/licqgui.h"
#include "core/mainwin.h"
#include "core/usermenu.h"

#include "contactdelegate.h"

using namespace LicqQtGui;

UserViewBase::UserViewBase(ContactListModel* contactList, bool useSkin, QWidget* parent)
  : QTreeView(parent),
    myContactList(contactList),
    myIsMainView(false),
    myAllowScrollTo(false)
{
  setItemDelegate(new ContactDelegate(this, useSkin, this));
  setEditTriggers(EditKeyPressed);

  // Look'n'Feel
  setIndentation(0);
  setVerticalScrollMode(ScrollPerPixel);
  setAcceptDrops(true);
  setRootIsDecorated(false);
  setAllColumnsShowFocus(true);

  connect(this, SIGNAL(doubleClicked(const QModelIndex&)),
      SLOT(slotDoubleClicked(const QModelIndex&)));

  if (useSkin)
  {
    applySkin();
    connect(Config::Skin::active(), SIGNAL(frameChanged()), SLOT(applySkin()));
  }
}

UserViewBase::~UserViewBase()
{
  // Empty
}

void UserViewBase::setColors(QColor back)
{
  if (!Config::ContactList::instance()->useSystemBackground())
  {
    QPalette pal = palette();

    if (back.isValid())
      pal.setColor(QPalette::Base, back);
    else
      pal.setColor(QPalette::Base, QColor("silver"));

    setPalette(pal);
  }
}

void UserViewBase::applySkin()
{
  setPalette(Config::Skin::active()->palette(gMainWindow));
  setColors(Config::Skin::active()->backgroundColor);
}

void UserViewBase::mousePressEvent(QMouseEvent* event)
{
  QTreeView::mousePressEvent(event);

  if (event->button() == Qt::LeftButton)
  {
    // Save position for later, needed by dragging and moving floaties
    myMousePressPos = event->pos();
  }
  else if (event->button() == Qt::MidButton)
  {
    QModelIndex clickedItem = indexAt(event->pos());
    if (clickedItem.isValid())
    {
      // Needed to distinguish double click origination
      if (static_cast<ContactListModel::ItemType>
          (clickedItem.data(ContactListModel::ItemTypeRole).toInt()) == ContactListModel::GroupItem)
        midEvent = true;
      emit doubleClicked(clickedItem);
    }
  }
}

void UserViewBase::mouseReleaseEvent(QMouseEvent* event)
{
  QTreeView::mouseReleaseEvent(event);

  myMousePressPos.setX(0);
  myMousePressPos.setY(0);
}

void UserViewBase::contextMenuEvent(QContextMenuEvent* event)
{
  QModelIndex clickedItem = indexAt(event->pos());
  if (clickedItem.isValid())
  {
    setCurrentIndex(clickedItem);

    popupMenu(viewport()->mapToGlobal(event->pos()), clickedItem);
  }
}

void UserViewBase::popupMenu(QPoint point, QModelIndex item)
{
  ContactListModel::ItemType itemType = static_cast<ContactListModel::ItemType>
    (item.data(ContactListModel::ItemTypeRole).toInt());

  if (itemType == ContactListModel::UserItem)
  {
    Licq::UserId userId = item.data(ContactListModel::UserIdRole).value<Licq::UserId>();

    gUserMenu->popup(point, userId, myIsMainView);
  }
  else if (itemType == ContactListModel::GroupItem)
  {
    int id = item.data(ContactListModel::GroupIdRole).toInt();
    bool online = (item.data(ContactListModel::SortPrefixRole).toInt() < 2);

    gLicqGui->groupMenu()->popup(point, id, online);
  }
}

void UserViewBase::editGroupName(int groupId, bool online)
{
  // We cannot just ask the model which index to use since the proxy/proxies
  //   use different indexes, so we'll just have to search for it.
  int rowCount = model()->rowCount();
  for (int i = 0; i < rowCount; ++i)
  {
    QModelIndex item = model()->index(i, 0);
    if (static_cast<ContactListModel::ItemType>(item.data(ContactListModel::ItemTypeRole).toInt()) != ContactListModel::GroupItem)
      continue;

    // Check if we got the correct group
    if (item.data(ContactListModel::GroupIdRole).toInt() != groupId)
      continue;
    if ((item.data(ContactListModel::SortPrefixRole).toInt() < 2) != online)
      continue;

    // We found it, make sure it's selected and start editing it
    setCurrentIndex(item);
    edit(item);
    return;
  }
}

void UserViewBase::dragEnterEvent(QDragEnterEvent* event)
{
  if (event->mimeData()->hasText() ||
      event->mimeData()->hasUrls())
    event->acceptProposedAction();
}

void UserViewBase::dropEvent(QDropEvent* event)
{
  // We ignore the event per default and then accept it if we
  // get to the end of this function.
  event->ignore();

  QModelIndex dropIndex = indexAt(event->pos());
  if (!dropIndex.isValid())
    return;

  ContactListModel::ItemType itemType = static_cast<ContactListModel::ItemType>
    (dropIndex.data(ContactListModel::ItemTypeRole).toInt());

  switch (itemType)
  {
    case ContactListModel::UserItem:
    {
      Licq::UserId userId = dropIndex.data(ContactListModel::UserIdRole).value<Licq::UserId>();

      // Drops for user is handled by common function
      if (!gLicqGui->userDropEvent(userId, *event->mimeData()))
        return;

      break;
    }
    case ContactListModel::GroupItem:
    {
      Licq::UserId dropUserId(gLicqGui->userIdFromMimeData(*event->mimeData()));
      if (!dropUserId.isValid())
        return;

      int gid = dropIndex.data(ContactListModel::GroupIdRole).toInt();

      // Should user be moved or just added to the new group?
      bool moveUser;
      if ((event->keyboardModifiers() & Qt::ShiftModifier) != 0)
        moveUser = true;
      else if ((event->keyboardModifiers() & Qt::ControlModifier) != 0)
        moveUser = false;
      else
        moveUser = Config::ContactList::instance()->dragMovesUser();

      Licq::gUserManager.setUserInGroup(dropUserId, gid, true, moveUser);

      // If we are moving user we now need to remove it from the old group.
      // However, since the drop event doesn't contain the originating
      // group, we don't know which group that is so we'll just have to
      // remove the user from all other groups.
      if (moveUser)
      {
        Licq::UserGroupList userGroups;
        {
          Licq::UserReadGuard u(dropUserId);
          if (u.isLocked())
            userGroups = u->GetGroups();
        }

        Licq::UserGroupList::const_iterator i;
        for (i = userGroups.begin(); i != userGroups.end(); ++i)
          if (*i != gid)
            Licq::gUserManager.setUserInGroup(dropUserId, *i, false, false);
      }
      break;
    }
    default:
      break;
  }

  event->acceptProposedAction();
}

void UserViewBase::dragMoveEvent(QDragMoveEvent* /* event */)
{
  // Do nothing, just overload the function so base class won't interfere
}

void UserViewBase::slotDoubleClicked(const QModelIndex& index)
{
  if (static_cast<ContactListModel::ItemType>
      (index.data(ContactListModel::ItemTypeRole).toInt()) == ContactListModel::UserItem)
  {
    Licq::UserId userId = index.data(ContactListModel::UserIdRole).value<Licq::UserId>();
    emit userDoubleClicked(userId);
  }
  else
  if (static_cast<ContactListModel::ItemType>
      (index.data(ContactListModel::ItemTypeRole).toInt()) == ContactListModel::GroupItem &&
      (index.column() != 0 || midEvent))
  {
    midEvent = false;
    setExpanded(index, !isExpanded(index));
  }
}

void UserViewBase::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
  // Workaround for annoying auto scrolling, see comment in scrollTo()
  myAllowScrollTo = true;
  QTreeView::currentChanged(current, previous);
  myAllowScrollTo = false;
}

void UserViewBase::timerEvent(QTimerEvent* event)
{
  // Workaround for annoying auto scrolling, see comment in scrollTo()
  myAllowScrollTo = true;
  QTreeView::timerEvent(event);
  myAllowScrollTo = false;
}

void UserViewBase::scrollTo(const QModelIndex& index, ScrollHint hint)
{
  // scrollTo is called from the following functions:
  //   QAbstractItemView::setVerticalScrollMode
  //   QAbstractItemView::timerEvent
  //   QAbstractItemView::currentChanged
  //   QAbstractItemViewPrivate::_q_layoutChanged
  //
  // Since layoutChanged is emitted by the sort proxy whenever anything item
  // in the list is changed this causes the list to scroll back to current item
  // which can be annoying when trying to scroll the list manually.
  // Since we cannot override a private function this is a ugly workaround to
  // block scrollTo as default but allow it for timerEvent and currentChanged
  // instead. (setVerticalScrollMode isn't used so we don't care for that one.)
  if (myAllowScrollTo)
    QTreeView::scrollTo(index, hint);
}

