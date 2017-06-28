#pragma once
#include "Models.h"


//Quadratic bezier curve for one data point
float calculateBezierParam(float x0, float x1, float x2, float t) {
	return pow(1 - t, 2) * x0 + 2 * t*(1 - t)*x1 + pow(t, 2)*x2;
}

//Quadratic bezier curve for vectors
Vector quadraticBezierCurve(Vector p0, Vector p1, Vector p2, float t) {
	return{
		calculateBezierParam(p0.X, p1.X, p2.X, t) ,
		calculateBezierParam(p0.Y, p1.Y, p2.Y, t) ,
		calculateBezierParam(p0.Z, p1.Z, p2.Z, t)
	};
}

//Apply chaikin to the given path
void apply_chaikin(std::unique_ptr<Path> path) 
{
	std::unique_ptr<savetype> newPath(new savetype());
	auto it = path->saves->begin();
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


//Overflows rotators so camera doesn't rotate the wrong way. Could probably be done in a better way lol
void fix_rotators(Rotator *prevSave, Rotator *nextSave, Rotator *nextNextSave) 
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