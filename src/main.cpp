#include <QApplication>
#include "reversigui.h"
#include "checkboxstyle.h"

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	app.setStyle(new CheckBoxStyle()); // eigene, gut sichtbare Ankreuzfelder (uebernimmt sonst den Systemstil)
	QApplication::setOrganizationName(QStringLiteral("Reversi"));
	QApplication::setApplicationName(QStringLiteral("Reversi")); // Ablageort der Einstellungen (QSettings)
	ReversiGui window;
	window.show();
	return app.exec();
}
