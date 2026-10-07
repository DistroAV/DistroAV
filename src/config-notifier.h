#pragma once

#include <QObject>

// Lightweight QObject used to emit configuration change notifications.
// This avoids making Config itself a QObject and keeps signalling centralized.
class ConfigNotifier : public QObject {
	Q_OBJECT
public:
	static ConfigNotifier *instance();

signals:
	void configChanged();
};
