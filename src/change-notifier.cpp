/******************************************************************************
	Copyright (C) 2016-2026 DistroAV <contact@distroav.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

#include "change-notifier.h"
#include <utility>

ChangeNotifier::ChangeNotifier(std::function<void()> onChange, QObject *parent) : QObject(parent)
{
	// If a callable was provided, connect it to the `changed` signal and keep
	// the QMetaObject::Connection so it can be disconnected automatically in
	// the destructor.
	if (onChange) {
		// Use the functor overload of QObject::connect.
		change_notify_connection = QObject::connect(this, &ChangeNotifier::changed, std::move(onChange));
	}
}

ChangeNotifier::~ChangeNotifier()
{
	// Disconnect the stored connection if valid.
	if (change_notify_connection) {
		QObject::disconnect(change_notify_connection);
	}
}

void ChangeNotifier::emitChanged()
{
	// Use Qt meta-object invoke to ensure the signal is emitted in the object's
	// thread. If we're already in the object's thread, this will execute
	// synchronously, otherwise it will queue the emission to the object's
	// thread event loop.
	if (QThread::currentThread() == thread()) {
		emit changed();
	} else {
		QMetaObject::invokeMethod(this, "changed", Qt::QueuedConnection);
	}
}
