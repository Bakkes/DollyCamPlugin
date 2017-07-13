#include "calculations.h"

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

int factorial(int n)
{
	return (n == 1 || n == 0) ? 1 : factorial(n - 1) * n;
}

void DollyCamCalculations::nBezierCurve(std::shared_ptr<savetype> l2, float t, Vector & v, Rotator &r, float & fov)
{
	v = Vector(0);
	r = Rotator(0);
	fov = 0;
	int n = l2->size();
	int idx = 0;
	for (auto it = l2->begin(); it != l2->end(); it++) 
	{
		float fact = (factorial(n - 1) / ((factorial(idx)*factorial((n - 1) - idx))));
		float po = pow(1 - t, n - 1 - idx) * pow(t, idx);
		float pofact = po * fact;


		Vector v2 = Vector(fact) * Vector(po) * it->second.location;
		Rotator r2;
		r2.Pitch = it->second.rotation.Pitch * pofact;
		r2.Yaw = it->second.rotation.Yaw * pofact;
		r2.Roll = it->second.rotation.Roll * pofact;
		float fov2 = fact * po * it->second.FOV;

		v = v + v2;
		r = r + r2;
		fov = fov + fov2;
		idx++;
	}
	//newVec = newVec + (Vector(pow(t, n-1)) * l.at(n - 1));
	
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
