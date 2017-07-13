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
	RationalBezier = 2, //Not implemented
	Chaikin = 3, //Not an interp mode, but something that can be applied to existing path, why is this there
	nBezier = 4,
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
	std::shared_ptr<savetype> saves;

	Path() 
	{
		saves = std::unique_ptr<savetype>(new savetype());
	}
};