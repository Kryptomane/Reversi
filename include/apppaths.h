#pragma once
// Plattformunabhaengiger Ordner fuer Log-Dateien (Windows: %APPDATA%/Reversi/Reversi, Android: App-Datenordner).
#include <QString>
#include <QStandardPaths>
#include <QDir>

inline QString logDirectory()
{
	static const QString dir = []() {
		QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
		if (d.isEmpty())
			d = QDir::tempPath();
		QDir().mkpath(d);
		return d + "/";
	}();
	return dir;
}
