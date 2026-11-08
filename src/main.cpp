#include <QApplication>
#include "commonincludes.h"
#include "reversigui.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	QApplication::setOrganizationName(QStringLiteral("Reversi"));
	QApplication::setApplicationName(QStringLiteral("Reversi")); // bestimmt den Log-Ordner
    ReversiGui w;
	w.show();
	return a.exec();
}
