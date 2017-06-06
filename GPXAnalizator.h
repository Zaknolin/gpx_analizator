#pragma once

#include <QImage>
#include <QMainWindow>
#include <QFileDialog>
#include "GraphWidget.h"
#include "TrackInfo.h"
#include "MGpxTools.h"

namespace Ui {
class MainWindow;
}

class GPXAnalizator : public QMainWindow
{
	Q_OBJECT

public:
	explicit GPXAnalizator( QWidget * parent = 0 );
	~GPXAnalizator();

public slots:
	void openFile();
	void saveFile();
	void updateSize();
	void updateTrackInfo();

private:
	QImage makeTrackInfoImage() const;

private:
	Ui::MainWindow * ui;
	GraphWidget m_graphWidget;
	TrackInfo m_trackInfo;
	std::vector<Position> m_positions;
};

