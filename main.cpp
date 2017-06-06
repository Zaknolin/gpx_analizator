#include "GPXAnalizator.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	GPXAnalizator gpxAnalizator;
	gpxAnalizator.show();

	return a.exec();
}
