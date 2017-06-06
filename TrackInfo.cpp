#include <limits>
#include "MGpxTools.h"
#include "TrackInfo.h"

bool TrackInfo::calculate( std::vector<Position> const & positions, float speedLimit ) {
	averageSpeed = 0;
	maxSpeed = std::numeric_limits< float >::min();
	minSpeed = std::numeric_limits< float >::max();
	distance = 0;
	driveDuration = 0;
	idleCount = 0;
	idleDuration = 0;
	overSpeedDuration = 0;
	overSpeedCount = 0;

	bool idleDetected = false;
	bool overSpeedDetected = false;
	for( size_t i = 0; i < positions.size() - 1; ++i ) {
		Position const & currPos = positions[ i ];
		Position const & nextPos = positions[ i + 1 ];
		if( currPos.speed < 0 )
			return false;

		int const currentIntervalTime = nextPos.time - currPos.time;
		if( currPos.speed > 0 ) {
			idleDetected = false;
			distance += currPos.DistanceInKM( nextPos );
			maxSpeed = std::max( maxSpeed, currPos.speed );
			minSpeed = std::min( minSpeed, currPos.speed );
			driveDuration += currentIntervalTime;
			if( currPos.speed > speedLimit ) {
				overSpeedDuration += currentIntervalTime;
				if( !overSpeedDetected ) {
					overSpeedDetected = true;
					overSpeedCount++;
				}
			} else
				overSpeedDetected = false;
		} else {
			idleDuration += currentIntervalTime;
			if( !idleDetected ) {
				idleDetected = true;
				idleCount++;
			}
		}
	}
	averageSpeed = distance / ( driveDuration / 3600.0 );
	return true;
}
