#include "calculations.h"
#include <algorithm>

float DollyCamCalculations::calculateBezierParam(float x0, float x1, float x2, float t)
{
	return pow(1 - t, 2) * x0 + 2 * t*(1 - t)*x1 + pow(t, 2)*x2;
}

Vector DollyCamCalculations::quadraticBezierCurve(Vector p0, Vector p1, Vector p2, float t)
{
	return {
		calculateBezierParam(p0.X, p1.X, p2.X, t) ,
		calculateBezierParam(p0.Y, p1.Y, p2.Y, t) ,
		calculateBezierParam(p0.Z, p1.Z, p2.Z, t)
	};
}

uint64_t factorial(int n)
{
	return (n == 1 || n == 0) ? 1 : factorial(n - 1) * n;
}

void DollyCamCalculations::nBezierCurve(std::shared_ptr<savetype> l2, float t, Vector & v, Rotator &r, float & fov)
{
	v = Vector(0);
	r = Rotator(0);
	fov = 0;
	int n = l2->size();
	int k = 0;
	for (savetype::const_iterator it = l2->cbegin(); it != l2->cend(); it++) 
	{
		uint64_t fact = (factorial(n - 1) / ((factorial(k)*factorial((n - 1) - k)))); //Maximum of 19 points =\ 
		float po = pow(1 - t, n - 1 - k) * pow(t, k);
		float pofact = po * fact;

		float weight = 1.f;
		if (it != l2->cbegin() && it != --(l2->cend())) {
			//weight = (++it)->first - (--it)->first;
		}
		//pofact *= weight;

		Vector v2 = Vector(pofact) * it->second.location;
		Rotator r2;
		r2.Pitch = it->second.rotation.Pitch * pofact;
		r2.Yaw = it->second.rotation.Yaw * pofact;
		r2.Roll = it->second.rotation.Roll * pofact;
		float fov2 = pofact * it->second.FOV;

		v = v + v2;
		r.Pitch += r2.Pitch;
		r.Yaw += r2.Yaw;
		r.Roll += r2.Roll;

		fov = fov + fov2;
		k++;
	}
	//newVec = newVec + (Vector(pow(t, n-1)) * l.at(n - 1));
	
}





//Vector DollyCamCalculations::slerp2(vec3 start, vec3 end, float percent)
//{
//	// Dot product - the cosine of the angle between 2 vectors.
//	//float dot = vec3_dot(start, end);
//	//// Clamp it to be in the range of Acos()
//	//// This may be unnecessary, but floating point
//	//// precision can be a fickle mistress.
//	//dot = clamp(dot, -1.0, 1.0);
//	//// Acos(dot) returns the angle between start and end,
//	//// And multiplying that by percent returns the angle between
//	//// start and the final result.
//	//float theta = acos(dot)*percent;
//
//	//vec3 temp;
//	//vec3 temp2 = { dot, dot, dot };
//	//
//	//vec3_mul(temp, start, temp2);
//
//	//vec3 temp3;
//	//vec3_sub(temp3, end, temp);
//
//	//vec3 RelativeVec;
//	//vec3_norm(RelativeVec, temp3); // Orthonormal basis
//	////RelativeVec = temp3;
//	//							   // The final result.
//
//	//vec3 start2 = { start[0], start[1], start[2] };
//
//	//vec3 start_res;
//	//vec3 end_res;
//	//vec3 cosTheta = { cos(theta) , cos(theta) , cos(theta) };
//	//vec3 sinTheta = { sin(theta) , sin(theta) , sin(theta) };
//	//vec3_mul(start_res, start2, cosTheta);
//	//vec3_mul(end_res, RelativeVec, sinTheta);
//	//return { start_res[0] + end_res[0], start_res[1] + end_res[1], start_res[2] + end_res[2]};
//}

//Vector DollyCamCalculations::slerp2(Vector start, Vector end, float percent)
//{
//	/*vec3 s = { start.X, start.Y, start.Z };
//	vec3 e = { end.X, end.Y, end.Z };
//	return slerp2(s, e, percent);*/
//}


//Vector lerp(vec2 start, vec2 end, float percent)
//{
//	return mix(start, end, percent);
//}
//
//Vector nlerp(vec2 start, vec2 end, float percent)
//{
//	return normalize(mix(start, end, percent));
//}

void DollyCamCalculations::slerp(std::shared_ptr<savetype> l2, float t, Vector & v, Rotator & r, float & fov)
{
	v = Vector(0);
	r = Rotator(0);
	fov = 0;
	int n = l2->size();
	int k = 0;
	for (savetype::const_iterator it = l2->cbegin(); it != l2->cend(); it++)
	{
		uint64_t fact = (factorial(n - 1) / ((factorial(k)*factorial((n - 1) - k)))); //Maximum of 19 points =\ 
		float po = pow(1 - t, n - 1 - k) * pow(t, k);
		float pofact = po * fact;

		float weight = 1.f;
		/*if (it != l2->cbegin() && it != --(l2->cend())) {
			weight = (++it)->first - (--it)->first;
		}*/
		pofact *= weight;

		Vector v2 = Vector(pofact) * it->second.location;
		Rotator r2;
		r2.Pitch = it->second.rotation.Pitch * pofact;
		r2.Yaw = it->second.rotation.Yaw * pofact;
		r2.Roll = it->second.rotation.Roll * pofact;
		float fov2 = pofact * it->second.FOV;

		v = v + v2;
		r.Pitch += r2.Pitch;
		r.Yaw += r2.Yaw;
		r.Roll += r2.Roll;

		fov = fov + fov2;
		k++;
	}

}



void DollyCamCalculations::apply_chaikin(std::shared_ptr<Path> path)
{
	std::unique_ptr<savetype> newPath(new savetype());
	savetype::iterator it = path->saves->begin();

	newPath->insert(std::make_pair(it->second.timeStamp, it->second));

	while (it != (--path->saves->end()))
	{
		CameraSnapshot currentSnapshot = (it->second);
		CameraSnapshot nextSnapshot = ((++it)->second);
		currentSnapshot.id *= 4;
		CameraSnapshot q1 = currentSnapshot;
		q1.id += 1;
		q1.location = currentSnapshot.location * .75 + nextSnapshot.location * .25;
		q1.rotation = currentSnapshot.rotation * .75 + nextSnapshot.rotation * .25;
		q1.FOV = currentSnapshot.FOV * .75 + nextSnapshot.FOV * .25;
		q1.timeStamp = currentSnapshot.timeStamp + ((nextSnapshot.timeStamp - currentSnapshot.timeStamp) * .25);

		CameraSnapshot q2 = currentSnapshot;
		q2.id += 2;
		q2.location = currentSnapshot.location * .25 + nextSnapshot.location * .75;
		q2.rotation = currentSnapshot.rotation * .25 + nextSnapshot.rotation * .75;
		q2.FOV = currentSnapshot.FOV * .25 + nextSnapshot.FOV * .75;
		q2.timeStamp = currentSnapshot.timeStamp + ((nextSnapshot.timeStamp - currentSnapshot.timeStamp) * .75);
		newPath->insert(std::make_pair(q1.timeStamp, q1));
		newPath->insert(std::make_pair(q2.timeStamp, q2));
	}

	path->saves->clear();
	path->saves->insert(newPath->begin(), newPath->end());
}
# define M_PI           3.14159265358979323846


float DollyCamCalculations::cosineInterp(float y1, float y2, float t)
{

	float t2 = (1 - cos(t*M_PI)) / 2;
	return (y1*(1 - t2) + y2*t2);
}

Vector DollyCamCalculations::cosineInterp(Vector p0, Vector p1, float t)
{
	return Vector(cosineInterp(p0.X, p1.X, t), cosineInterp(p0.Y, p1.Y, t), cosineInterp(p0.Z, p1.Z, t));
}

float DollyCamCalculations::cubicInterp(float y0, float y1, float y2, float y3, float t)
{
	float a0, a1, a2, a3, t2;
	t2 = t*t;
	a0 = y3 - y2 - y0 + y1;
	a1 = y0 - y1 - a0;
	a2 = y2 - y0;
	a3 = y1;
	return a0*t*t2 + a1*t2 + a2*t + a3;
}

Vector DollyCamCalculations::cubicInterp(Vector p0, Vector p1, Vector p2, Vector p3, float t)
{
	return Vector(cubicInterp(p0.X, p1.X, p2.X, p3.X, t), cubicInterp(p0.Y, p1.Y, p2.Y, p3.Y, t), cubicInterp(p0.Z, p1.Z, p2.Z, p3.Z, t));
}

float DollyCamCalculations::hermiteInterp(float y0, float y1, float y2, float y3, float t)
{
	float m0, m1, mu2, mu3;
	float a0, a1, a2, a3;
	float bias = 0;
	float tension = 0;

	mu2 = t * t;
	mu3 = mu2 * t;
	m0 = (y1 - y0)*(1 + bias)*(1 - tension) / 2;
	m0 += (y2 - y1)*(1 - bias)*(1 - tension) / 2;
	m1 = (y2 - y1)*(1 + bias)*(1 - tension) / 2;
	m1 += (y3 - y2)*(1 - bias)*(1 - tension) / 2;
	a0 = 2 * mu3 - 3 * mu2 + 1;
	a1 = mu3 - 2 * mu2 + t;
	a2 = mu3 - mu2;
	a3 = -2 * mu3 + 3 * mu2;

	return(a0*y1 + a1*m0 + a2*m1 + a3*y2);
}

Vector DollyCamCalculations::hermiteInterp(Vector p0, Vector p1, Vector p2, Vector p3, float t)
{
	return Vector(hermiteInterp(p0.X, p1.X, p2.X, p3.X, t), hermiteInterp(p0.Y, p1.Y, p2.Y, p3.Y, t), hermiteInterp(p0.Z, p1.Z, p2.Z, p3.Z, t));
}

void DollyCamCalculations::fix_rotators(Rotator * prevSave, Rotator * nextSave, Rotator * nextNextSave)
{
	if (prevSave->Yaw < 0)
	{
		if (nextSave->Yaw > 0 && nextSave->Yaw - prevSave->Yaw > 32768)
		{
			nextSave->Yaw -= 32768 * 2;
		}
		if (nextNextSave->Yaw > 0 && nextNextSave->Yaw - prevSave->Yaw > 32768)
		{
			nextNextSave->Yaw -= 32768 * 2;
		}
	}
	else if (prevSave->Yaw > 0)
	{
		if (nextSave->Yaw < 0 && prevSave->Yaw - nextSave->Yaw > 32768)
		{
			nextSave->Yaw += 32768 * 2;
		}
		if (nextNextSave->Yaw < 0 && prevSave->Yaw - nextNextSave->Yaw > 32768)
		{
			nextNextSave->Yaw += 32768 * 2;
		}
	}
	if (prevSave->Roll < 0)
	{
		if (nextSave->Roll > 0 && prevSave->Roll - nextSave->Roll > 32768)
		{
			nextSave->Roll -= 32768 * 2;
		}
		if (nextNextSave->Roll > 0 && nextNextSave->Roll - prevSave->Roll > 32768)
		{
			nextNextSave->Roll -= 32768 * 2;
		}
	}
	else if (prevSave->Roll > 0)
	{
		if (nextSave->Roll < 0 && prevSave->Roll - nextSave->Roll > 32768)
		{
			nextSave->Roll += 32768 * 2;
		}
		if (nextNextSave->Roll < 0 && prevSave->Roll - nextNextSave->Roll > 32768)
		{
			nextNextSave->Roll += 32768 * 2;
		}
	}
	if (prevSave->Pitch < 0)
	{
		if (nextSave->Pitch > 0 && prevSave->Pitch - nextSave->Pitch > 16364)
		{
			nextSave->Pitch -= 16364 * 2;
		}
		if (nextNextSave->Pitch > 0 && nextNextSave->Pitch - prevSave->Pitch > 16364)
		{
			nextNextSave->Pitch -= 16364 * 2;
		}
	}
	else if (prevSave->Pitch > 0)
	{
		if (nextSave->Pitch < 0 && prevSave->Pitch - nextSave->Pitch > 16364)
		{
			nextSave->Pitch += 16364 * 2;
		}
		if (nextNextSave->Pitch < 0 && prevSave->Pitch - nextNextSave->Pitch > 16364)
		{
			nextNextSave->Pitch += 16364 * 2;
		}
	}
}
