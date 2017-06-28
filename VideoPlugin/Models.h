#pragma once
#include "wrappers/wrapperstructs.h"
#include <map>
#include <memory>
#define savetype std::map<float, CameraSnapshot>
//typedef std::map<float, CameraSnapshot> savetype;

enum InterpMode
{
	Linear = 0,
	QuadraticBezier = 1,
	RationalBezier = 2,
	Chaikin = 3,
};

struct CameraSnapshot {
	int id;
	float timeStamp;
	int frame;
	Vector location;
	Rotator rotation;
	float FOV;

	CameraSnapshot() {}

};


struct Path {
	std::unique_ptr<savetype> saves;

	Path() 
	{
		saves = std::unique_ptr<savetype>(new savetype());
	}
};