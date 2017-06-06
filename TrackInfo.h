#pragma once
#include <vector>

class Position;

struct TrackInfo
{
	bool calculate( std::vector< Position > const & positions, float speedLimit );

	double averageSpeed = 0;
	double maxSpeed = 0;
	double minSpeed = 0;
	double distance = 0;
	long driveDuration = 0;
	int idleCount = 0;
	long idleDuration = 0;
	long overSpeedDuration = 0;
	int overSpeedCount = 0;
};
