#include <QApplication>
#include "reversigui.h"

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	QApplication::setOrganizationName(QStringLiteral("Reversi"));
	QApplication::setApplicationName(QStringLiteral("Reversi")); // Ablageort der Einstellungen (QSettings)
	ReversiGui window;
	window.show();
	return app.exec();
}
