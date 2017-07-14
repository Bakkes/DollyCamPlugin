#pragma once
#include "Models.h"

namespace DollyCamCalculations {

	//Quadratic bezier curve for one data point
	float calculateBezierParam(float x0, float x1, float x2, float t);

	//Quadratic bezier curve for vectors
	Vector quadraticBezierCurve(Vector p0, Vector p1, Vector p2, float t);

	void nBezierCurve(std::shared_ptr<savetype> l, float t, Vector &v, Rotator &r, float &fov);
	void slerp(std::shared_ptr<savetype> l, float t, Vector &v, Rotator &r, float &fov);
	//Vector slerp2(Vector start, Vector end, float percent);
	//Vector slerp2(vec3 start, vec3 end, float percent);
	//Apply chaikin to the given path
	void apply_chaikin(std::shared_ptr<Path> path);

	float cosineInterp(float y1, float y2, float t);
	Vector cosineInterp(Vector p0, Vector p1, float t);

	float cubicInterp(float y0, float y1, float y2, float y3, float t);
	Vector cubicInterp(Vector p0, Vector p1, Vector p2, Vector p3, float t);
	

	//Overflows rotators so camera doesn't rotate the wrong way. Could probably be done in a better way lol
	void fix_rotators(Rotator *prevSave, Rotator *nextSave, Rotator *nextNextSave);


	static inline void vec3_mul(vec3 r, vec3 const a, vec3 const b)
	{
		r[0] = a[0] * b[0];
		r[1] = a[1] * b[1];
		r[2] = a[2] * b[2];
	}
}

