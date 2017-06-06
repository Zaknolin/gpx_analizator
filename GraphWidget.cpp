#include <algorithm>
#include <QPainter>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QDebug>
#include "GraphWidget.h"

int const g_axisWidth = 35;
int const g_axisLineWidth = 2;

GraphWidget::GraphWidget( QWidget * parent )
	: QWidget( parent )
{
	setMinimumHeight( 100 );
}

void GraphWidget::setPositions( std::vector< Position > positions, float maxSpeed, float speedLimit ) {
	m_maxSpeed = maxSpeed;
	m_speedLimit = speedLimit;
	m_positions = std::move( positions );
	if ( m_positions.size() < 2 ) {
		m_positions.clear();
		return;
	}
	if ( m_scrollBar != nullptr )
		m_scrollBar->setValue( 0 );
	update();
}

QImage GraphWidget::makeSpeedImageForSave() {
	int const maxImageWidth = 32000;
	time_t const duration = m_positions.back().time - m_positions.front().time;
	float const scaleFactor = ( duration <= maxImageWidth ) ? 1 : float( maxImageWidth ) / duration;
	float const imageWidth = duration * scaleFactor;
	float const imageHeight = m_maxSpeed * scaleFactor;
	QImage const image = makeSpeedImage( imageWidth, imageHeight, 0, scaleFactor );

	QImage result( imageWidth + g_axisWidth, imageHeight + g_axisWidth, QImage::Format_RGB32 );
	result.fill( Qt::white );

	QPainter painter( &result );
	painter.setRenderHint( QPainter::Antialiasing );
	painter.drawImage( g_axisWidth, 0, image, 0, 0 );
	drawAxis( painter, result.width(), result.height(), 0, scaleFactor );
	return result;
}

void GraphWidget::setStartPosition( int pos ) {
	m_startPosition = pos;
	this->update();
}

void GraphWidget::paintEvent( QPaintEvent * ) {
	if( m_positions.empty() )
		return;

	int const imageWidth = width() - g_axisWidth;
	int const imageHeight = height() - g_axisWidth;
	float const scaleFactor =  imageHeight / m_maxSpeed;
	// адаптируем полосу прокрутки под текущий размер
	if( m_scrollBar != nullptr ) {
		time_t const duration = m_positions.back().time - m_positions.front().time;
		if( duration * scaleFactor <= imageWidth ) {
			m_scrollBar->setMaximum( 1 );
			m_scrollBar->setMinimum( 0 );
			m_scrollBar->setPageStep( 1 );
		} else {
			int const  pageStep = imageWidth / scaleFactor;
			int const maximum = duration - pageStep - 1;
			m_scrollBar->setMaximum( maximum );
			m_scrollBar->setMinimum( 0 );
			m_scrollBar->setPageStep( pageStep );
			m_scrollBar->setSingleStep( std::max( pageStep / 20, 1 ) );
		}
	}

	QPainter painter( this );
	QImage image = makeSpeedImage( imageWidth, imageHeight, m_startPosition, scaleFactor );
	painter.setRenderHint( QPainter::Antialiasing );
	painter.drawImage( g_axisWidth, 0, image, 0, 0 );
	drawAxis( painter, width(), height(), m_startPosition, scaleFactor );
}

QString GraphWidget::secondsToHumanReadable( time_t seconds ) {
	std::vector< int > times;
	times.reserve( 5 );
	const auto & rawSeconds = std::div( static_cast< int64_t >( seconds ), static_cast< int64_t >( 60 ) );
	times.push_back( rawSeconds.rem );
	const auto & rawMinutes = std::div( rawSeconds.quot, static_cast< int64_t >( 60 ) );
	times.push_back( rawMinutes.rem );
	const auto & rawHours = std::div( rawMinutes.quot, static_cast< int64_t >( 24 ) );
	times.push_back( rawHours.rem );
	const auto & rawDays = std::div( rawHours.quot, static_cast< int64_t >( 30 ) );

	QString result;
	if( rawDays.quot > 0 )
		result = QString::number( rawDays.quot ) + " м." + QString::number( rawDays.rem ) + " д. ";
	else if( rawDays.rem > 0 )
		result = QString::number( rawDays.rem ) + " д. ";

	for( int i = times.size() - 1; i >= 0; --i )
		if( times[ i ] > 0 || !result.isEmpty() )
			result += QString::asprintf( "%02d:", times[ i ] );
	result.resize( result.size() - 1 ); // удаляем последний лишний символ ':'
	return result;
}

void GraphWidget::drawAxis( QPainter & painter, float painterWidth, float painterHeight, int startOffset, float scaleFactor ) {
	QPen axisPen( Qt::SolidLine );
	axisPen.setWidth( g_axisLineWidth );
	axisPen.setColor( Qt::black );
	painter.setPen( axisPen );

	// рисуем оси
	int const axisDrawWidth = g_axisWidth - g_axisLineWidth;
	painter.drawLine( axisDrawWidth, 0, axisDrawWidth, painterHeight - axisDrawWidth );
	painter.drawLine( axisDrawWidth, painterHeight - axisDrawWidth, painterWidth, painterHeight - axisDrawWidth );

	// рисуем стрелки на осях
	painter.drawLine( axisDrawWidth, g_axisLineWidth, axisDrawWidth - g_axisLineWidth, g_axisLineWidth * 4 );
	painter.drawLine( axisDrawWidth, g_axisLineWidth, axisDrawWidth + g_axisLineWidth, g_axisLineWidth * 4 );
	painter.drawLine( painterWidth - g_axisLineWidth, painterHeight - axisDrawWidth, painterWidth - g_axisLineWidth * 4, painterHeight - axisDrawWidth - g_axisLineWidth );
	painter.drawLine( painterWidth - g_axisLineWidth, painterHeight - axisDrawWidth, painterWidth - g_axisLineWidth * 4, painterHeight - axisDrawWidth + g_axisLineWidth );

	// размечаем Y ось координатами
	QFontMetrics fontInfo = painter.fontMetrics();
	const float YlabelHeight = fontInfo.boundingRect( "0" ).height();
	const float rawYLabelsCount = painterHeight / 3 / YlabelHeight;
	const float rawYDelimeter = m_maxSpeed / rawYLabelsCount;
	const std::vector< int > yDelimeters = { 1, 5, 10, 20, 50, 100, 200, 300, 400, 500, 1000 };
	const auto YDelimIter = std::upper_bound( yDelimeters.begin(), yDelimeters.end(), int( rawYDelimeter ) );
	const int YDelimeter = YDelimIter != yDelimeters.end() ? *YDelimIter : yDelimeters.back();
	for( int i = 0; i * YDelimeter < m_maxSpeed; ++i ) {
		int const YLabelValue = i * YDelimeter;
		QString const YLabel = QString::number( YLabelValue ) + "  ";
		const float YLabelX = axisDrawWidth - g_axisLineWidth * 2 - fontInfo.boundingRect( YLabel ).width();
		const float lineY = painterHeight - axisDrawWidth - YLabelValue * scaleFactor;
		const float YLabelY = lineY + fontInfo.boundingRect( YLabel ).height() / 2.0;
		painter.drawText( YLabelX, YLabelY, YLabel );
		painter.drawLine( axisDrawWidth, lineY, axisDrawWidth - g_axisLineWidth * 2, lineY );
	}

	// размечаем X ось координатами
	const float XlabelWidth = fontInfo.boundingRect( secondsToHumanReadable( startOffset + painterWidth / scaleFactor ) ).width();
	const float rawXLabelsCount = painterWidth / 2 / XlabelWidth;
	const float rawXDelimeter = (painterWidth / scaleFactor) / rawXLabelsCount;
	const std::vector< int > xDelimeters = { 1, 5, 10, 20, 30, 60, 120, 180, 300, 600, 1800, 3600, 18000 };
	const auto XDelimIter = std::upper_bound( xDelimeters.begin(), xDelimeters.end(), int( rawXDelimeter ) );
	const int XDelimeter = XDelimIter != xDelimeters.end() ? *XDelimIter : xDelimeters.back();
	for( int i = startOffset / XDelimeter; i * XDelimeter * scaleFactor < ( startOffset * scaleFactor + painterWidth ); ++i ) {
		int const XLabelValue = i * XDelimeter;
		QString const XLabel = secondsToHumanReadable( XLabelValue );
		const float axisLineX = ( XLabelValue - startOffset ) * scaleFactor;
		if( axisLineX < 0 )
			continue;
		const float lineX = axisLineX + axisDrawWidth;

		painter.drawLine( lineX, painterHeight - axisDrawWidth, lineX, painterHeight - axisDrawWidth - g_axisLineWidth * 2 );

		const float XLabelX = lineX - fontInfo.boundingRect( XLabel ).width() / 2.0;
		painter.drawText( XLabelX, painterHeight, XLabel );
	}
}

QImage GraphWidget::makeSpeedImage( float imageWidth, float imageHeight, int startOffset, float scaleFactor ) {
	QImage image( imageWidth, imageHeight, QImage::Format_RGB32 );
	image.fill( Qt::white );
	QPainter imagePainter( &image );
	imagePainter.setRenderHint( QPainter::Antialiasing );

	// рисуем линию ограничения скорости
	imagePainter.setPen( Qt::red );
	float const speedLimitY = imageHeight - m_speedLimit * scaleFactor;
	imagePainter.drawLine( 0, speedLimitY, imageWidth, speedLimitY );
	QFontMetrics fontInfo = imagePainter.fontMetrics();
	QString const label = QString::number( m_speedLimit ) + " км/ч";
	int const labelWidth = fontInfo.boundingRect( label ).width();
	int const labelHeight = fontInfo.boundingRect( label ).height();
	imagePainter.drawText( imageWidth - labelWidth, speedLimitY - labelHeight / 3.0, label );

	// рисуем график скоростей
	imagePainter.setPen( Qt::blue );
	time_t const startTime = m_positions.front().time;
	QPointF fromPoint( 0, imageHeight );
	for( Position const pos: m_positions ) {
		QPointF toPoint( ( pos.time - startTime - startOffset ) * scaleFactor, imageHeight - pos.speed * scaleFactor );
		imagePainter.drawLine( fromPoint, toPoint );
		fromPoint = std::move( toPoint );
	}
	return image;
}
