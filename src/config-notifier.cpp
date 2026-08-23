#include "config-notifier.h"

// Meyers singleton pattern for QObject instance.
ConfigNotifier *ConfigNotifier::instance()
{
	static ConfigNotifier notifier;
	return &notifier;
}

// No manual include of moc; rely on build system automoc to generate and compile moc files.
