#include <QApplication>
#include "commonincludes.h"
#include "reversigui.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
    ReversiGui w;
	w.show();
	return a.exec();
}
