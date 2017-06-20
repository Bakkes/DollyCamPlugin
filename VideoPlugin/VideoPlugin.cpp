#include "VideoPlugin.h"
#include <map>
#include <fstream>
BAKKESMOD_PLUGIN(VideoPlugin, "Video plugin", "0.2", 0)

GameWrapper* gw = NULL;
ConsoleWrapper* cons = NULL;


struct CamSave {
	float timeStamp;
	Vector location;
	Rotator rotator;
	float FOV;
};

std::map<float, CamSave> saves;

long long playback() {
	ReplayWrapper sw = gw->GetGameEventAsReplay();


	float replaySeconds = sw.GetReplayTimeElapsed();
	float currentTimeInMs = sw.GetSecondsElapsed(); //GetReplayTimeElapsed returns weird values?
	CamSave prevSave;
	CamSave nextSave;
	prevSave.timeStamp = -1;
	nextSave.timeStamp = -1;
	for (auto it = saves.begin(); it != saves.end(); it++)
	{
		if (it->first > replaySeconds && nextSave.timeStamp < 0)
		{
			nextSave = it->second;
			break;
		}
		prevSave = it->second;
	}

	if (nextSave.timeStamp >= 0 && prevSave.timeStamp >= 0)
	{
		float frameDiff = nextSave.timeStamp - prevSave.timeStamp;
		float timeElapsed = currentTimeInMs - prevSave.timeStamp;

		Rotator snapR = Rotator(frameDiff);
		Vector snap = Vector(frameDiff);

		//Vector newLoc = gw->GetCamera().linterp(prevSave.location, nextSave.location, timeElapsed, 1.0f); //; prevSave.location + ((nextSave.location - prevSave.location) * timeElapsed) / snap;
		Vector newLoc = prevSave.location + ((nextSave.location - prevSave.location) * timeElapsed) / snap;
		if (prevSave.rotator.Yaw < -32768 / 2 && nextSave.rotator.Yaw > 32768 / 2) {
			nextSave.rotator.Yaw -= 32768 * 2;
		}
		if (prevSave.rotator.Yaw > 32768 / 2 && nextSave.rotator.Yaw < -32768 / 2) {
			nextSave.rotator.Yaw += 32768 * 2;
		}

		if (prevSave.rotator.Roll < -32768 / 2 && nextSave.rotator.Roll > 32768 / 2) {
			nextSave.rotator.Roll -= 32768 * 2;
		}
		if (prevSave.rotator.Roll > 32768 / 2 && nextSave.rotator.Roll < -32768 / 2) {
			nextSave.rotator.Roll += 32768 * 2;
		}

		if (prevSave.rotator.Pitch < -16364 / 2 && nextSave.rotator.Pitch > 16364 / 2) {
			nextSave.rotator.Pitch -= 16364 * 2;
		}
		if (prevSave.rotator.Pitch > 16364 / 2 && nextSave.rotator.Pitch < -16364 / 2) {
			nextSave.rotator.Pitch += 16364 * 2;
		}

		Rotator newRot;
		newRot.Pitch = prevSave.rotator.Pitch + ((nextSave.rotator.Pitch - prevSave.rotator.Pitch) * timeElapsed / frameDiff);
		newRot.Yaw = prevSave.rotator.Yaw + ((nextSave.rotator.Yaw - prevSave.rotator.Yaw) * timeElapsed / frameDiff);
		newRot.Roll = prevSave.rotator.Roll + ((nextSave.rotator.Roll - prevSave.rotator.Roll) * timeElapsed / frameDiff);


		//Rotator newRot = prevSave.rotator + ((nextSave.rotator - prevSave.rotator) * timeElapsed) / snapR;
		//newLoc = newLoc - (Vector(newRot.Pitch, newRot.Yaw, newRot.Roll) * 100.0);
		//Vector newLoc = gw->VecInterp(prevSave.location, nextSave.location, timeElapsed, 1.0f);

		//newRot.Yaw += 50;
		POV p;
		p.location = newLoc + Vector(0);
		p.rotation = newRot + Rotator(0);
		p.FOV = prevSave.FOV;
		gw->GetCamera().SetPOV(p);
		//gw->SetFlyCameraLoc(newLoc, newRot);
	}
	else {
		sw.SetSecondsElapsed(sw.GetReplayTimeElapsed());
	}

	return 1.f;
}

void run_playback() {
	if (!gw->IsInReplay())
		return;

	gw->SetTimeout([](GameWrapper* gameWrapper) {
		run_playback();
	}, playback());
}

//void testCallback(ActorWrapper aw, string s, void* params) {
//	//Best so far? : "Function Engine.PlayerController.PreRender" PRE hook
//	if (s.find("Engine.Interaction.Tick") != -1 ||
//		s.compare("Function Engine.PlayerController.PlayerTick") == 0 ||
//		s.compare("Function Engine.PlayerController.PreRender") == 0) // Function ProjectX.Camera_X.GetCameraState, ProjectX.Camera_X.ModifyPostProcessSettings, Function TAGame.Camera_Replay_TA.UpdateCamera
//	{
//		//cons->log("Test");
//		ReplayWrapper sw = gw->GetGameEventAsReplay();
//
//		float currentTimeInMs = sw.GetReplayTimeElapsed();
//
//		CamSave prevSave;
//		CamSave nextSave;
//		prevSave.timeStamp = -1;
//		nextSave.timeStamp = -1;
//		for (auto it = saves.begin(); it != saves.end(); it++)
//		{
//			if (it->first > currentTimeInMs && nextSave.timeStamp < 0)
//			{
//				nextSave = it->second;
//				break;
//			}
//			prevSave = it->second;
//		}
//		if (nextSave.timeStamp >= 0 && prevSave.timeStamp >= 0)
//		{
//			float frameDiff = nextSave.timeStamp - prevSave.timeStamp;
//			float timeElapsed = currentTimeInMs - prevSave.timeStamp;
//
//			Rotator snapR = Rotator(frameDiff);
//			Vector snap = Vector(frameDiff);
//
//			Vector newLoc = prevSave.location + ((nextSave.location - prevSave.location) * timeElapsed) / snap;
//			//-32768, -32768
//			if (prevSave.rotator.Yaw < -32768 / 2 && nextSave.rotator.Yaw > 32768/2) {
//				nextSave.rotator.Yaw -= 32768 * 2;
//			}
//			if (prevSave.rotator.Yaw > 32768 / 2 && nextSave.rotator.Yaw < -32768 / 2) {
//				nextSave.rotator.Yaw += 32768 * 2;
//			}
//
//			if (prevSave.rotator.Roll < -32768 / 2 && nextSave.rotator.Roll > 32768 / 2) {
//				nextSave.rotator.Roll -= 32768 * 2;
//			}
//			if (prevSave.rotator.Roll > 32768 / 2 && nextSave.rotator.Roll < -32768 / 2) {
//				nextSave.rotator.Roll += 32768 * 2;
//			}
//
//			if (prevSave.rotator.Pitch < -16364 / 2 && nextSave.rotator.Pitch > 16364 / 2) {
//				nextSave.rotator.Pitch -= 16364 * 2;
//			}
//			if (prevSave.rotator.Pitch > 16364 / 2 && nextSave.rotator.Pitch < -16364 / 2) {
//				nextSave.rotator.Pitch += 16364 * 2;
//			}
//			prevSave.rotator.Pitch - nextSave.rotator.Pitch;
//			//if(prevSave.rotator.Pitch
//
//			Rotator newRot;
//			newRot.Pitch = prevSave.rotator.Pitch + ((nextSave.rotator.Pitch - prevSave.rotator.Pitch) * timeElapsed) / snapR.Pitch;
//			newRot.Yaw = prevSave.rotator.Yaw + ((nextSave.rotator.Yaw - prevSave.rotator.Yaw) * timeElapsed) / snapR.Yaw;
//			newRot.Roll = prevSave.rotator.Roll + ((nextSave.rotator.Roll - prevSave.rotator.Roll) * timeElapsed) / snapR.Roll;
//			newRot = newRot + Rotator(0);
//			//Rotator newRot = prevSave.rotator + ((nextSave.rotator - prevSave.rotator) * timeElapsed) / snapR;
//			//newLoc = newLoc - (Vector(newRot.Pitch, newRot.Yaw, newRot.Roll) * 100.0);
//			//Vector newLoc = gw->VecInterp(prevSave.location, nextSave.location, timeElapsed, 1.0f);
//			POV p;
//			p.location = newLoc + Vector(0);
//			//p.rotation = newRot + Rotator(0);
//			p.FOV = prevSave.FOV;
//			gw->GetCamera().SetPOV(p); //, newRot
//			gw->GetCamera().SetLocation(newLoc);
//		}
//	}
//	else {
//		cons->log(s);
//	}
//}

void saveCam() {
	std::ofstream out("out.fly", ios::out | ios::trunc | ios::binary);
	int size = saves.size();
	write_pod(out, size);
	for (auto it = saves.begin(); it != saves.end(); it++)
	{
		write_pod(out, it->second);
	}
}

void loadCam() {
	std::ifstream in("out.fly", ios::binary);
	saves.clear();
	int size = 0;
	read_pod(in, size);
	for (unsigned int i = 0; i < size; i++)
	{
		CamSave save;
		read_pod(in, save);
		saves.insert(std::make_pair(save.timeStamp, save));
	}
}

void videoPlugin_onCommand(std::vector<std::string> params)
{
	string command = params.at(0);
	if (!gw->IsInReplay()) {
		return;
	}
	if (command.compare("cam_save") == 0)
	{
		ReplayWrapper sw = gw->GetGameEventAsReplay();
		CamSave save;
		save.timeStamp = sw.GetReplayTimeElapsed();
		CameraWrapper flyCam = gw->GetCamera();
		
		//if (flyCam.IsNull()) {
		//	cons->log("Need to be in fly cam for this");
		//	return;
		//} 
		POV currentPov = flyCam.GetPOV();

		save.location = currentPov.location;
		save.rotator = currentPov.rotation;
		save.FOV = currentPov.FOV;
		saves.insert(std::make_pair(save.timeStamp, save));
		saveCam();
	}
	else if (command.compare("cam_play") == 0)
	{
		cons->log("playback");
		loadCam();

		run_playback();
		//ActorWrapper flyCam = gw->GetFlyCameraLoc();
		//flyCam.ListenForEvents(testCallback);
		//gw->GetSolidHook().ListenForEvents(testCallback, HookMode_Post);
		//gw->GetCamera().ListenForEvents(testCallback);
		//gw->GetCameraOwner().ListenForEvents(testCallback);
		

	}
	else if (command.compare("debug_cam") == 0) 
	{
		/*ActorWrapper cam = gw->GetCamra();
		auto location = cam.GetLocation();
		auto rotation = cam.GetRotation();
		cons->log("Location" + to_string(location.X) + "," + to_string(location.Y) + "," + to_string(location.Z));
		cons->log("Rotation" + to_string(rotation.Pitch) + "," + to_string(rotation.Yaw) + "," + to_string(rotation.Roll));*/
	}
}

void VideoPlugin::onLoad()
{
	gw = gameWrapper;
	cons = console;

	cons->registerNotifier("cam_save", videoPlugin_onCommand);
	cons->registerNotifier("cam_play", videoPlugin_onCommand);
	cons->registerNotifier("debug_cam", videoPlugin_onCommand);
}

void VideoPlugin::onUnload()
{
}
