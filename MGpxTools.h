#pragma once

#include <limits>
#include <vector>
#include <istream>
#include <string>

struct Position
{
	explicit Position( double iX = std::numeric_limits< double >::quiet_NaN(), double iY = std::numeric_limits< double >::quiet_NaN(), time_t iTime = 0, float iSpeed = -1 )
		: x( iX )
		, y( iY )
		, time( iTime )
		, speed( iSpeed )
	{}

	double DistanceInKM( Position const & iPnt ) const;

	void CalculateSpeedByNext( Position const & iNext );
	std::string trace() const {
		return "x=" + std::to_string( x ) + ", y=" + std::to_string( y ) + ", time=" + std::to_string( time ) + ", speed=" + std::to_string( speed );
	}

	double x = 0;
	double y = 0;
	time_t time = 0;
	double speed = -1;

private:
	double Distance( Position const & iPnt, double iCosY ) const;
	double Distance( Position const & iPnt ) const;
};

namespace gpx
{
	/// Restore positions from file to position vector.
	std::vector< Position > ReadTrack( std::string const & iFilePath );
	std::vector< Position > ReadTrack( std::istream & ioStream );
}

