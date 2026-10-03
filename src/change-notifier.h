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

#pragma once

#include <QObject>
#include <QMetaObject>
#include <QThread>
#include <functional>

// ChangeNotifier: lightweight QObject you can instantiate per-sender.
// Usage model (example):
//
// struct MySenderData {
//     ChangeNotifier *notifier = nullptr; // per-sender notifier
//     QMetaObject::Connection notifier_conn; // not needed if you provide the lambda to the ctor
// };
//
// // create and store per-sender notifier with a lambda callback
// senderData->notifier = new ChangeNotifier([](){
//     /* handle change for this sender */
// });
//
// // emit from any thread:
// senderData->notifier->emitChanged();
//
// // cleanup when sender is destroyed:
// delete senderData->notifier;
// senderData->notifier = nullptr;
class ChangeNotifier : public QObject {
	Q_OBJECT

public:
	// Construct with a lambda (or any callable compatible with void()) that will be
	// invoked when this notifier's `changed()` signal is emitted.
	// Parent is optional to keep usual QObject ownership semantics.
	explicit ChangeNotifier(std::function<void()> onChange, QObject *parent = nullptr);
	~ChangeNotifier() override;

	// Emits the changed() signal. Safe to call from any thread; emission will be
	// queued to the notifier's thread when necessary.
	void emitChanged();

private:
	QMetaObject::Connection change_notify_connection;

signals:
	void changed();
};
