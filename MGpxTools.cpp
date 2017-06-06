#include <map>
#include <math.h>
#include <exception>
#include <string>
#include <time.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include "MGpxTools.h"

char const * const GPX_HEADER_MASK = "<?xml version=\"1.0\"?>\n"
	"<gpx version=\"1.0\" creator=\"JamServer\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
	"xmlns=\"http://www.topografix.com/GPX/1/0\" xsi:schemaLocation=\"http://www.topografix.com/GPX/1/0 "
	"http://www.topografix.com/GPX/1/0/gpx.xsd\">\n";

char const * const GPX_TIME_MASK = "<time>%04d-%02d-%02dT%02d:%02d:%02dZ</time>";
char const * const GPX_HDOP_MASK = "<hdop>%d</hdop>";
char const * const GPX_POS_HEAD_AND_POINT_MASK = "      <trkpt lat=\"%.6f\" lon=\"%.6f\">";

char const * const GPX_POS_DESC_MASK = "<desc>mcc: %u, mnc: %u, lac: %u, cid: %u, ss: -%u, ta: %u</desc>";
char const * const GPX_POS_TAIL = "</trkpt>\n";

char const * const GPX_TRACK_HEAD = "  <trk><name>Track</name>\n    <trkseg>\n";
char const * const GPX_TRACK_TAIL = "    </trkseg>\n  </trk>\n";

char const * const GPX_TAIL = "</gpx>";

time_t const GAP_TIME = 60;
// --------------------------------------------------------------------------------------
double const PI = 3.141592653589793;
double const PI_FACTOR = PI / 180.0;
double const MERIDIAN_LEN = 40007860.; // Meridian length in meters.
double const EQUATOR_LEN = 40075696.; // Equator length in meters.
double const ONE_METER = 360.0 / MERIDIAN_LEN; // in Y-degrees (~9e-6)
std::string const WHITE_SPACE_CHARS = " \n\r\t\v\f";

time_t StringToTime( std::string const & iDateTime )
{
	struct tm tms = {};

	int const scanned = ::sscanf( iDateTime.c_str(), "%04d%*c%02d%*c%02d%*c%02d%*c%02d%*c%02d",
			&tms.tm_year, &tms.tm_mon, &tms.tm_mday, &tms.tm_hour, &tms.tm_min, &tms.tm_sec );
	if( scanned != 6 )
		return 0;

	tms.tm_year -= 1900;
	tms.tm_mon -= 1;

	return ::mktime( &tms );
}

//-------------------------------------------------------------------------
std::string & TrimLeft( std::string & ioStr )
{
	size_t const pos = ioStr.find_first_not_of( WHITE_SPACE_CHARS );
	if( pos == std::string::npos )
		ioStr.clear();
	else
		ioStr.erase( 0, pos );

	return ioStr;
}
//-------------------------------------------------------------------------
std::string & TrimRight( std::string & ioStr )
{
	size_t const pos = ioStr.find_last_not_of( WHITE_SPACE_CHARS );
	if( pos == std::string::npos )
		ioStr.clear();
	else
		ioStr.erase( pos + 1 );

	return ioStr;
}

inline std::string & Trim( std::string & ioStr ) {
	return TrimLeft( TrimRight( ioStr ) );
}

inline double CosLatitude( double iLatitude ) {
	return ::cos( iLatitude * PI_FACTOR );
}

inline double YDegreesToMeters( double iYDeg ) {
	double const res = iYDeg / ONE_METER;
	return res > 0 ? res : 0;
}

double Position::Distance( Position const & iPnt, double iCosY ) const {
	double const dx = ( x - iPnt.x ) * iCosY;
	double const dy = y - iPnt.y;
	return ::sqrt( dx * dx + dy * dy );
}

double Position::DistanceInKM( Position const & iPnt ) const {
	return YDegreesToMeters( this->Distance( iPnt ) ) / 1000.0;
}

double Position::Distance( Position const & iPnt ) const {
	return this->Distance( iPnt, CosLatitude( ( y + iPnt.y ) / 2 ) );
}
void Position::CalculateSpeedByNext( Position const & iNext ) {
	double const distance = YDegreesToMeters( this->Distance( iNext ) );
	time_t const timeInterval = ::abs( iNext.time - time );
	speed = timeInterval ? distance * 3.6 / timeInterval : 0; // where 3.6 = ( 3600s / 1000m ), for speed in km / h
}

// --------------------------------------------------------------------------------------
/**
 * @class MParserGPX is tool class for parsing a .gpx file.
 */
class MParserGPX
{
public:
	MParserGPX( std::istream & iStream );
	bool GetNextTrackPos( Position & oPos );

private: // types
	typedef std::map< std::string, std::string > TTags; // < tag_name, tag_value > like: "<tag>value</tag>"

private: // helpers
	bool ReadDoubleAttribute( std::string const & iName, size_t iReadingIndex, double & oValue );
	bool SeekToken( std::string const & iToken, size_t & ioReadingIndex, std::string const & iEndToken = "" );
	void ReadSimpleTags( size_t const & iReadingIndex, std::string const & iEndToken, TTags & oResult );
	bool GetTagValue( TTags const & iTags, std::string const & iTagName, std::string & oValue );

	void ReadFromStream( std::istream & iStream );

private: // members
	time_t      m_lastPosTime;  /// time of the last position
	Position    m_next;         /// position to be parsed
	size_t      m_readingIndex; ///position of read index in fileExists
	std::string m_fileData;     /// string to store whole file
};

//-------------------------------------------------------------------------
MParserGPX::MParserGPX( std::istream & iStream )
	: m_lastPosTime( 0 )
	, m_readingIndex( 0 )
{
	ReadFromStream( iStream );
}

//-------------------------------------------------------------------------
size_t GetInputStreamFullSize( std::istream & iStream )
{
	std::streampos const curr = iStream.tellg();
	if( curr == -1 ) return -1;
	iStream.seekg( 0, std::ios::end );
	std::streampos const last = iStream.tellg();
	if( last == -1 ) return -1;
	iStream.seekg( curr );
	return last;
}

//-------------------------------------------------------------------------
void MParserGPX::ReadFromStream( std::istream & iStream )
{
	size_t const size = ::GetInputStreamFullSize( iStream );
	if( size == size_t( -1 ) )
		throw std::logic_error( "MParserGPX: Can't know the size of a stream" );
	m_fileData.resize( size );
	iStream.read( &m_fileData[0], size );
}

//-------------------------------------------------------------------------
bool MParserGPX::SeekToken( std::string const & iToken, size_t & ioReadingIndex, std::string const & iEndToken )
{
	// < tagname tagparam1= value1 param2 = value2>contents...contents...contents</tagname >
	//          ^ <-- cursor is positioned here.

	// TODO: make function faster?
	// false when not found till breakingTag or EOF

	size_t const tagStart = m_fileData.find( iToken, ioReadingIndex );

	if( !iEndToken.empty() )
	{
		size_t const end = m_fileData.find( iEndToken, ioReadingIndex );
		if( ( tagStart >= end ) && ( end != std::string::npos ) )
			return false;
	}

	if( ( tagStart != std::string::npos ) && ( tagStart > ioReadingIndex ) )
	{
		ioReadingIndex = tagStart + iToken.length();
		return true;
	}

	return false;
}

//-------------------------------------------------------------------------
bool MParserGPX::GetTagValue( TTags const & iTags, std::string const & iTagName, std::string & oValue )
{
	TTags::const_iterator it = iTags.find( iTagName );
	if( it == iTags.end() )
		return false;
	oValue = it->second;
	return true;
}

//-------------------------------------------------------------------------
void MParserGPX::ReadSimpleTags( size_t const & iReadingIndex, std::string const & iEndToken, TTags & oResult )
{
	// Reads all tags before </trkpt> (or any other end token) is met.
	oResult.clear();
	size_t index = iReadingIndex;
	std::string name, val;

	while( SeekToken( "<", index, iEndToken ) )
	{
		if( m_fileData.substr( index, 1 ).compare( "/" ) == 0 )
			continue;

		size_t end = index;
		SeekToken( ">", end, iEndToken );

		std::string temp = m_fileData.substr( index, end - index - 1 );
		name = Trim( temp ); //End points to the symbol next to '>'

		index = end;
		SeekToken( "<", end, iEndToken );

		temp = m_fileData.substr( index, end - index - 1 );
		val = Trim( temp ); //End points to the symbol next to '<'

		oResult[ name ] = val;
	}
}

//-------------------------------------------------------------------------
bool MParserGPX::ReadDoubleAttribute( std::string const & iName, size_t iReadingIndex, double & oValue )
{
	// Format is the following: <trkpt lat="55.743412" lon="37.533829"><ele>32....
	size_t start = iReadingIndex;

	if ( !SeekToken( iName, start, ">" ) ) // name = lat, lon
		return false;
	if ( !SeekToken( "\"", start, ">" ) )
		return false;

	size_t end = start;
	if ( !SeekToken( "\"", end, ">" ) )
		return false;

	std::string const valueStr = m_fileData.substr( start, end - start - 1 );
	oValue = std::stod( valueStr );
	return true;
}

//-------------------------------------------------------------------------
bool MParserGPX::GetNextTrackPos(Position & oPos )
{
	static char const * const OPENING_TRACKPT = "<trkpt";
	static char const * const CLOSING_TRACKPT = "</trkpt";
	oPos = Position();

	TTags       tags;
	std::string content;

	while( true )
	{
		if( !this->SeekToken( OPENING_TRACKPT, m_readingIndex ) ) // this automatically increases index
			return false; // the end of a track reached

		if( ( !this->ReadDoubleAttribute( "lon", m_readingIndex, m_next.x ) ) ||
				( !this->ReadDoubleAttribute( "lat", m_readingIndex, m_next.y ) ) )
			continue; // skip position without coordinates

		this->ReadSimpleTags( m_readingIndex, CLOSING_TRACKPT, tags );

		if( this->GetTagValue( tags, "time", content ) ) {
			if( content.size() != 20 )
				continue; // skip position with wront time field format
			m_next.time = StringToTime( content );
		}
		else {
			continue; // skip position without time
		}

		if( m_next.time == 0 ) // skip position with incorrect time
			continue;

		if( m_next.time <= m_lastPosTime ) // non-chronological positions -> skip it
			continue;

		break;
	}

	m_lastPosTime = m_next.time;
	oPos = m_next;
	return true;
}

//-------------------------------------------------------------------------
void ReadTrackFromStream( std::istream & iStream, std::vector< Position > & oPositions )
{
	MParserGPX parserGpx( iStream );
	Position pi;

	while( parserGpx.GetNextTrackPos( pi ) )
		oPositions.push_back( pi );
}

//#########################################################################
//---------------------------- Namespace gpx ------------------------------
//#########################################################################
std::vector<Position> gpx::ReadTrack(std::string const & iFilePath)
{
	std::locale::global(std::locale("C"));
	std::ifstream file( iFilePath.c_str(), std::ios::binary | std::ios::in );

	if( !file )
		throw std::logic_error( "gpx: Can't open GPX track file: " + iFilePath );

	return gpx::ReadTrack( file );
}

//-------------------------------------------------------------------------
std::vector< Position >  gpx::ReadTrack( std::istream & ioStream)
{
	std::vector< Position > result;
	try
	{
		std::vector< Position > rawPositions;
		MParserGPX parserGpx( ioStream );
		Position pi;

		while( parserGpx.GetNextTrackPos( pi ) )
			rawPositions.push_back( pi );

		std::sort(rawPositions.begin(), rawPositions.end(), [](Position const & lv, Position const & rv) {return lv.time < rv.time;});
		if(rawPositions.size() > 1) {
			// fill gap
			for(size_t i = 0; i < rawPositions.size() - 1; ++i) {
				result.push_back(rawPositions[i]);
				const time_t duration = rawPositions[i + 1].time - rawPositions[i].time;
				if (duration > GAP_TIME) {
					result.back().speed = 0;
					result.push_back(rawPositions[i + 1]);
					result.back().speed = 0;
					result.back().time -= 1;
				} else
					result.back().CalculateSpeedByNext(rawPositions[i + 1]);
			}
			result.push_back(rawPositions.back());
			result.back().speed = (++result.rbegin())->speed;
		}
	}
	catch( std::exception & e )
	{
		std::cerr << "gpx: std::exception: " << e.what() << ", unable to read track from stream" << std::endl;
		return {};
	}
	catch( ... )
	{
		std::cerr << "gpx: Unknown exception, unable to read track from stream" << std::endl;
		return {};
	}
	return result;
}
