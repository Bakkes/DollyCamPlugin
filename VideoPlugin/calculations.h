#pragma once
#include "Models.h"

namespace DollyCamCalculations {

	//Quadratic bezier curve for one data point
	float calculateBezierParam(float x0, float x1, float x2, float t);

	//Quadratic bezier curve for vectors
	Vector quadraticBezierCurve(Vector p0, Vector p1, Vector p2, float t);

	void nBezierCurve(std::shared_ptr<savetype> l, float t, Vector &v, Rotator &r, float &fov);

	//Apply chaikin to the given path
	void apply_chaikin(std::shared_ptr<Path> path);


	//Overflows rotators so camera doesn't rotate the wrong way. Could probably be done in a better way lol
	void fix_rotators(Rotator *prevSave, Rotator *nextSave, Rotator *nextNextSave);
}

