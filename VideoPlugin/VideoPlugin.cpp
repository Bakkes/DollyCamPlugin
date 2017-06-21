#include "VideoPlugin.h"
#include <map>
#include <fstream>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>

BAKKESMOD_PLUGIN(VideoPlugin, "Video plugin", "0.2", 0)

GameWrapper* gw = NULL;
ConsoleWrapper* cons = NULL;

static int current_id = 0;

struct VectorSerializable {
	Vector vector;

	VectorSerializable(Vector v) : vector(v) {}
	VectorSerializable() {}
	template <class Archive>
	void serialize(Archive & ar)
	{
		ar(CEREAL_NVP(vector.X), CEREAL_NVP(vector.Y), CEREAL_NVP(vector.Z));
	}
};

struct RotatorSerializable {
	Rotator rotator;

	RotatorSerializable(Rotator r) : rotator(r) {}
	RotatorSerializable() {}
	template <class Archive>
	void serialize(Archive & ar)
	{
		ar(CEREAL_NVP(rotator.Pitch), CEREAL_NVP(rotator.Yaw), CEREAL_NVP(rotator.Roll));
	}
};

struct CamSave {
	int id;
	float timeStamp;
	VectorSerializable location;
	RotatorSerializable rotation;
	float FOV;

	CamSave() {}
	template <class Archive>
	void serialize(Archive & ar)
	{
		ar(CEREAL_NVP(id), CEREAL_NVP(timeStamp));
		location.serialize(ar);
		rotation.serialize(ar);
		ar(CEREAL_NVP(FOV));
	}
};

struct Path {
	std::map<float, CamSave> saves;

	template <class Archive>
	void serialize(Archive & ar)
	{
		ar(CEREAL_NVP(saves));
	}
};

Path currentPath;

long long playback() {
	ReplayWrapper sw = gw->GetGameEventAsReplay();


	float replaySeconds = sw.GetReplayTimeElapsed();
	float currentTimeInMs = sw.GetSecondsElapsed(); //GetReplayTimeElapsed returns weird values?
	CamSave prevSave;
	CamSave nextSave;
	prevSave.timeStamp = -1;
	nextSave.timeStamp = -1;
	for (auto it = currentPath.saves.begin(); it != currentPath.saves.end(); it++)
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

		//Vector newLoc = gw->GetCamera().linterp(prevSave.location.vector, nextSave.location.vector, timeElapsed, 1.0f); //; prevSave.location.vector + ((nextSave.location.vector - prevSave.location.vector) * timeElapsed) / snap;
		Vector newLoc = prevSave.location.vector + ((nextSave.location.vector - prevSave.location.vector) * timeElapsed) / snap;
		if (prevSave.rotation.rotator.Yaw < -32768 / 2 && nextSave.rotation.rotator.Yaw > 32768 / 2) {
			nextSave.rotation.rotator.Yaw -= 32768 * 2;
		}
		if (prevSave.rotation.rotator.Yaw > 32768 / 2 && nextSave.rotation.rotator.Yaw < -32768 / 2) {
			nextSave.rotation.rotator.Yaw += 32768 * 2;
		}

		if (prevSave.rotation.rotator.Roll < -32768 / 2 && nextSave.rotation.rotator.Roll > 32768 / 2) {
			nextSave.rotation.rotator.Roll -= 32768 * 2;
		}
		if (prevSave.rotation.rotator.Roll > 32768 / 2 && nextSave.rotation.rotator.Roll < -32768 / 2) {
			nextSave.rotation.rotator.Roll += 32768 * 2;
		}

		if (prevSave.rotation.rotator.Pitch < -16364 / 2 && nextSave.rotation.rotator.Pitch > 16364 / 2) {
			nextSave.rotation.rotator.Pitch -= 16364 * 2;
		}
		if (prevSave.rotation.rotator.Pitch > 16364 / 2 && nextSave.rotation.rotator.Pitch < -16364 / 2) {
			nextSave.rotation.rotator.Pitch += 16364 * 2;
		}

		Rotator newRot;
		newRot.Pitch = prevSave.rotation.rotator.Pitch + ((nextSave.rotation.rotator.Pitch - prevSave.rotation.rotator.Pitch) * timeElapsed / frameDiff);
		newRot.Yaw = prevSave.rotation.rotator.Yaw + ((nextSave.rotation.rotator.Yaw - prevSave.rotation.rotator.Yaw) * timeElapsed / frameDiff);
		newRot.Roll = prevSave.rotation.rotator.Roll + ((nextSave.rotation.rotator.Roll - prevSave.rotation.rotator.Roll) * timeElapsed / frameDiff);


		//Rotator newRot = prevSave.rotation.rotator + ((nextSave.rotation.rotator - prevSave.rotation.rotator) * timeElapsed) / snapR;
		//newLoc = newLoc - (Vector(newRot.Pitch, newRot.Yaw, newRot.Roll) * 100.0);
		//Vector newLoc = gw->VecInterp(prevSave.location.vector, nextSave.location.vector, timeElapsed, 1.0f);

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
//			Vector newLoc = prevSave.location.vector + ((nextSave.location.vector - prevSave.location.vector) * timeElapsed) / snap;
//			//-32768, -32768
//			if (prevSave.rotation.rotator.Yaw < -32768 / 2 && nextSave.rotation.rotator.Yaw > 32768/2) {
//				nextSave.rotation.rotator.Yaw -= 32768 * 2;
//			}
//			if (prevSave.rotation.rotator.Yaw > 32768 / 2 && nextSave.rotation.rotator.Yaw < -32768 / 2) {
//				nextSave.rotation.rotator.Yaw += 32768 * 2;
//			}
//
//			if (prevSave.rotation.rotator.Roll < -32768 / 2 && nextSave.rotation.rotator.Roll > 32768 / 2) {
//				nextSave.rotation.rotator.Roll -= 32768 * 2;
//			}
//			if (prevSave.rotation.rotator.Roll > 32768 / 2 && nextSave.rotation.rotator.Roll < -32768 / 2) {
//				nextSave.rotation.rotator.Roll += 32768 * 2;
//			}
//
//			if (prevSave.rotation.rotator.Pitch < -16364 / 2 && nextSave.rotation.rotator.Pitch > 16364 / 2) {
//				nextSave.rotation.rotator.Pitch -= 16364 * 2;
//			}
//			if (prevSave.rotation.rotator.Pitch > 16364 / 2 && nextSave.rotation.rotator.Pitch < -16364 / 2) {
//				nextSave.rotation.rotator.Pitch += 16364 * 2;
//			}
//			prevSave.rotation.rotator.Pitch - nextSave.rotation.rotator.Pitch;
//			//if(prevSave.rotation.rotator.Pitch
//
//			Rotator newRot;
//			newRot.Pitch = prevSave.rotation.rotator.Pitch + ((nextSave.rotation.rotator.Pitch - prevSave.rotation.rotator.Pitch) * timeElapsed) / snapR.Pitch;
//			newRot.Yaw = prevSave.rotation.rotator.Yaw + ((nextSave.rotation.rotator.Yaw - prevSave.rotation.rotator.Yaw) * timeElapsed) / snapR.Yaw;
//			newRot.Roll = prevSave.rotation.rotator.Roll + ((nextSave.rotation.rotator.Roll - prevSave.rotation.rotator.Roll) * timeElapsed) / snapR.Roll;
//			newRot = newRot + Rotator(0);
//			//Rotator newRot = prevSave.rotation.rotator + ((nextSave.rotation.rotator - prevSave.rotation.rotator) * timeElapsed) / snapR;
//			//newLoc = newLoc - (Vector(newRot.Pitch, newRot.Yaw, newRot.Roll) * 100.0);
//			//Vector newLoc = gw->VecInterp(prevSave.location.vector, nextSave.location.vector, timeElapsed, 1.0f);
//			POV p;
//			p.location.vector = newLoc + Vector(0);
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
	int size = currentPath.saves.size();
	write_pod(out, size);
	for (auto it = currentPath.saves.begin(); it != currentPath.saves.end(); it++)
	{
		write_pod(out, it->second);
	}
}

void loadCam() {
	std::ifstream in("out.fly", ios::binary);
	currentPath.saves.clear();
	current_id = 0;
	int size = 0;
	read_pod(in, size);
	for (unsigned int i = 0; i < size; i++)
	{
		CamSave save;
		read_pod(in, save);
		currentPath.saves.insert(std::make_pair(save.timeStamp, save));
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
		save.id = current_id;
		save.location = VectorSerializable(currentPov.location);
		save.rotation = RotatorSerializable(currentPov.rotation);
		save.FOV = currentPov.FOV;
		currentPath.saves.insert(std::make_pair(save.timeStamp, save));
		saveCam();
		current_id++;

		std::ofstream ofs("cam.json", std::ios::out | std::ios::trunc);  // Output file stream.
		cereal::JSONOutputArchive  oarchive(ofs); // Choose binary format, writingdirection.
		currentPath.serialize(oarchive); //oarchive(saves); // Save the modified instance.
		ofs.close();
		
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
