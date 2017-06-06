#include <QLabel>
#include <QImage>
#include <QImageWriter>
#include <QTime>
#include <QPainter>
#include <QDebug>
#include <QTimer>
#include <QFontMetrics>

#include "MGpxTools.h"
#include "GPXAnalizator.h"
#include "ui_mainwindow.h"

GPXAnalizator::GPXAnalizator( QWidget * parent )
	: QMainWindow( parent )
	, ui( new Ui::MainWindow )
	, m_graphWidget( this )
{
	ui->setupUi( this );
	ui->graphlLayout->insertWidget( 0, &m_graphWidget );
	ui->saveButton->setDisabled( true );

	connect( ui->saveButton, SIGNAL(pressed() ), this, SLOT( saveFile() ) );
	connect( ui->loadButton, SIGNAL(pressed() ), this, SLOT( openFile() ) );

	m_graphWidget.setScrollBar( ui->horizontalScrollBar );
	connect( ui->horizontalScrollBar, SIGNAL( valueChanged( int ) ), &m_graphWidget, SLOT( setStartPosition( int ) ) );

	connect( ui->speedLimitEdit, SIGNAL( editingFinished() ), this, SLOT( updateTrackInfo() ) );
}

void GPXAnalizator::openFile() {
	QString const fileName = QFileDialog::getOpenFileName( this, tr( "Загрузить GPX файл" ), "", tr( "GPX трек (*.gpx)" ) );
	if( !fileName.isEmpty() ) {
		m_positions = gpx::ReadTrack( fileName.toStdString() );
		updateTrackInfo();
	}
}

void GPXAnalizator::saveFile() {
	QString const fileName = QFileDialog::getSaveFileName( this, tr( "Сохранить файл" ), "", tr( "Картинки (*.png)" ) );
	if( !fileName.isEmpty() ) {
		QFile file( fileName );
		if( !file.open( QIODevice::WriteOnly | QIODevice::Truncate ) ) {
			statusBar()->showMessage( "Ошибка открытия файла для записи: " + fileName );
			return;
		}
		QImageWriter imgWriter( &file, "png" );
		QImage const graphImage = m_graphWidget.makeSpeedImageForSave();
		QImage const trackInfoImage = makeTrackInfoImage();
		QImage resultImage( std::max( graphImage.width(), trackInfoImage.width() ), graphImage.height() + trackInfoImage.height(), QImage::Format_RGB32 );
		resultImage.fill( Qt::white );
		QPainter painter( &resultImage );
		painter.drawImage( 0 , 0, trackInfoImage);
		painter.drawImage( 0, trackInfoImage.height(), graphImage );
		bool const result = imgWriter.write( resultImage );
		if( result )
			statusBar()->showMessage( "График скоростей записан в файл: " + fileName );
		else
			statusBar()->showMessage( "Ошибка записи картинки: " + imgWriter.errorString() );
	}
}

void GPXAnalizator::updateSize() {
	this->resize( this->width(), this->minimumHeight() );
}

void GPXAnalizator::updateTrackInfo() {
	float const speedLimit = ui->speedLimitEdit->text().toFloat();
	if( m_trackInfo.calculate( m_positions, speedLimit ) ) {
		ui->averageSpeedLabel->setText( "Средняя скорость: " + QString::asprintf( "%.1f", m_trackInfo.averageSpeed) + " км/ч") ;
		ui->distanceLabel->setText( "Длина пути: " + QString::asprintf("%.1f", m_trackInfo.distance) + " км" );
		ui->driveDurationLabel->setText( "Вермя в движении: " + GraphWidget::secondsToHumanReadable( m_trackInfo.driveDuration ) );
		ui->idleDurationLabel->setText( "Время стоянок: " + GraphWidget::secondsToHumanReadable( m_trackInfo.idleDuration ) );
		ui->idleCountLabel->setText( "Кол-во стоянок: " + QString::asprintf( "%d", m_trackInfo.idleCount ) );
		ui->maxSpeedLabel->setText( "Максимальная скорость: " + QString::asprintf( "%.1f", m_trackInfo.maxSpeed ) + " км/ч" );
		ui->minSpeedLabel->setText( "Минимальная скорость: " + QString::asprintf( "%.1f", m_trackInfo.minSpeed ) + " км/ч" );
		ui->overSpeedCountLabel->setText( "Кол-во превышений скорости: " + QString::asprintf( "%d", m_trackInfo.overSpeedCount ) );
		ui->overSpeedDurationLabel->setText( "Время с превышением скорости: " + GraphWidget::secondsToHumanReadable( m_trackInfo.overSpeedDuration ) );

		m_graphWidget.setPositions( m_positions, m_trackInfo.maxSpeed, speedLimit );
		statusBar()->showMessage( "Считано позиций из файла: " + QString::number( m_positions.size() ) );
		ui->saveButton->setDisabled( false );
	} else {
		statusBar()->showMessage( "Ошибочные данные: отрицательная скорость" );
		ui->saveButton->setDisabled( true );
	}
}

QImage GPXAnalizator::makeTrackInfoImage() const {
	int const columnSpace = 30;

	// подготовка информации о треке
	QStringList trackInfos;
	trackInfos.push_back( "Кол-во позиций в треке: " + QString::number( m_positions.size() ) );
	trackInfos.push_back( "Средняя скорость: " + QString::asprintf( "%.1f", m_trackInfo.averageSpeed ) + " км/ч");
	trackInfos.push_back( "Длина пути: " + QString::asprintf( "%.1f", m_trackInfo.distance ) + " км" );
	trackInfos.push_back( "Вермя в движении: " + GraphWidget::secondsToHumanReadable( m_trackInfo.driveDuration ) );
	trackInfos.push_back( "Время стоянок: " + GraphWidget::secondsToHumanReadable( m_trackInfo.idleDuration ) );
	trackInfos.push_back( "Кол-во стоянок: " + QString::asprintf( "%d", m_trackInfo.idleCount ) );
	trackInfos.push_back( "Максимальная скорость: " + QString::asprintf( "%.1f", m_trackInfo.maxSpeed ) + " км/ч" );
	trackInfos.push_back( "Минимальная скорость: " + QString::asprintf( "%.1f", m_trackInfo.minSpeed ) + " км/ч" );
	trackInfos.push_back( "Кол-во превышений скорости: " + QString::asprintf( "%d", m_trackInfo.overSpeedCount ) );
	trackInfos.push_back( "Время с превышением скорости: " + GraphWidget::secondsToHumanReadable( m_trackInfo.overSpeedDuration ) );

	// расчет требуемых размеров катинки для размещения текста
	QImage fakeImage( 100, 100, QImage::Format_RGB32 );
	QPainter fakePainter( &fakeImage );
	fakePainter.setRenderHint( QPainter::Antialiasing );
	QFontMetrics fontInfo = fakePainter.fontMetrics();
	int maxLeftTextWidth = 0;
	int textHeight = 0;
	for( int i = 0; i < trackInfos.size() / 2; ++i ) {
		maxLeftTextWidth = std::max( maxLeftTextWidth, fontInfo.boundingRect( trackInfos[ i ] ).width() );
		textHeight += fontInfo.boundingRect( trackInfos[ i ] ).height();
	}
	int maxRightTextWidth = 0;
	for( int i = trackInfos.size() / 2; i < trackInfos.size(); ++i ) {
		maxRightTextWidth = std::max( maxRightTextWidth, fontInfo.boundingRect( trackInfos[ i ] ).width() );
		textHeight += fontInfo.boundingRect( trackInfos[ i ] ).height();
	}


	int const fontHeight = textHeight / trackInfos.size();
	QImage result( maxLeftTextWidth + columnSpace * 3 + maxRightTextWidth, textHeight / 2, QImage::Format_RGB32 );
	result.fill( Qt::white );
	QPainter painter( &result );
	painter.setPen( Qt::black );
	int y = 0;
	int const leftColumnX = columnSpace / 2;
	for( int i = 0; i < trackInfos.size() / 2; ++i ) {
		painter.drawText( leftColumnX, y, trackInfos[ i ] );
		y += fontHeight;
	}
	y = 0;
	int const rightColumnX = leftColumnX + maxLeftTextWidth + columnSpace;
	for( int i = trackInfos.size() / 2; i < trackInfos.size(); ++i ) {
		painter.drawText( rightColumnX, y, trackInfos[ i ] );
		y += fontHeight;
	}

	QPen pen( Qt::SolidLine );
	pen.setWidth( 3 );
	pen.setColor( Qt::gray );
	painter.setPen( pen );
	int const lineX = rightColumnX - columnSpace / 2;
	painter.drawLine( lineX, fontHeight / 2, lineX, result.height() - fontHeight / 2);

	return result;
}

GPXAnalizator::~GPXAnalizator()
{
	m_graphWidget.setScrollBar( nullptr );
	delete ui;
}
