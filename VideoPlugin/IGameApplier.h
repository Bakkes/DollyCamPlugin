#pragma once
#include "bakkesmod/wrappers/gamewrapper.h"

//Interface for applying stuff to the game which will help with simulations
class IGameApplier
{
public:
	virtual void SetPOV(Vector location, Rotator rotation, float FOV) = 0;
	virtual POV GetPOV() = 0;
};

//Applies the given angles to the Rocket League game
class RealGameApplier : public IGameApplier {
private:
	GameWrapper* gameWrapper;
public:
	RealGameApplier(GameWrapper* gw);
	void SetPOV(Vector location, Rotator rotation, float FOV);
	POV GetPOV();
};

//Mock game applier for simulating data
class MockGameApplier : public IGameApplier {
public:
	Vector loc;
	Rotator rot;
	float fov;
	float time;
	MockGameApplier(string filename);
	~MockGameApplier();
	void SetTime(float time);
	void SetPOV(Vector location, Rotator rotation, float FOV);
	POV GetPOV();
};