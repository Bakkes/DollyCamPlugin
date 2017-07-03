#include "IGameApplier.h"
#include <iostream>
#include <fstream>

RealGameApplier::RealGameApplier(GameWrapper * gw) : gameWrapper(gw)
{
}

void RealGameApplier::SetPOV(Vector location, Rotator rotation, float FOV)
{
	gameWrapper->GetCamera().SetPOV({ location, rotation, FOV });

}

POV RealGameApplier::GetPOV()
{
	CameraWrapper camera = gameWrapper->GetCamera();
	return{ camera.GetLocation(), camera.GetRotation(), camera.GetFOV() };
}

ofstream output;
MockGameApplier::MockGameApplier(string filename)
{
	output.open(filename, ios::out | ios::trunc);
}

MockGameApplier::~MockGameApplier()
{
	output.flush();
	output.close();
}

void MockGameApplier::SetTime(float t)
{
	time = t;
}

void MockGameApplier::SetPOV(Vector location, Rotator rotation, float FOV)
{
	loc = location;
	rot = rotation;
	fov = FOV;
	output << time << "," << location.X << "," << location.Y << "," << location.Z << endl;
}

POV MockGameApplier::GetPOV()
{
	return{ loc, rot, fov };
}
