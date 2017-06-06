#pragma once

#include <QWidget>
#include "MGpxTools.h"

class QPainter;
class QScrollBar;

class GraphWidget : public QWidget
{
	Q_OBJECT
public:
	explicit GraphWidget( QWidget * parent = 0 );

	void setPositions( std::vector<Position> positions, float maxSpeed, float speedLimit );

	void setScrollBar( QScrollBar * scrollBar ) {
		m_scrollBar = scrollBar;
	}

	QImage makeSpeedImageForSave();

	static QString secondsToHumanReadable( time_t seconds );

signals:

public slots:
	void setStartPosition( int pos );

protected:
	void paintEvent( QPaintEvent * );

private:
	void drawAxis( QPainter & painter, float painterWidth, float painterHeight, int startOffset, float scaleFactor );
	QImage makeSpeedImage( float imageWidth, float imageHeight, int startOffset, float scaleFactor );

private:
	std::vector< Position > m_positions;
	float m_maxSpeed = 0;
	float m_speedLimit = 105;
	int m_startPosition = 0;
	QScrollBar * m_scrollBar = nullptr;
};
