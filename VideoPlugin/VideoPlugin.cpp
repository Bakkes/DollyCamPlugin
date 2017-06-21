#include "VideoPlugin.h"
#include <map>
#include "helpers.h"
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

	Path() {}

	template <class Archive>
	void serialize(Archive & ar)
	{
		ar(CEREAL_NVP(saves));
	}
};

Path currentPath;
bool playbackActive = false;
long long playback() {
	ReplayWrapper sw = gw->GetGameEventAsReplay();

	float replaySeconds = sw.GetReplayTimeElapsed();
	float currentTimeInMs = sw.GetSecondsElapsed();
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

		POV p;
		p.location = newLoc + Vector(0);
		p.rotation = newRot + Rotator(0);
		p.FOV = prevSave.FOV;
		gw->GetCamera().SetPOV(p);
	}
	else {
		sw.SetSecondsElapsed(sw.GetReplayTimeElapsed());
	}

	return 1.f;
}

void run_playback() {
	if (!gw->IsInReplay() || !playbackActive)
		return;

	gw->SetTimeout([](GameWrapper* gameWrapper) {
		run_playback();
	}, playback());
}

void videoPlugin_onCommand(std::vector<std::string> params)
{
	string command = params.at(0);

	if (command.compare("dolly_path_load") == 0)
	{
		if (params.size() < 2) {
			cons->log("Usage: dolly_path_load filename");
			return;
		}
		string filename = params.at(1);
		if (!file_exists("./bakkesmod/data/campaths/" + filename + ".json")) {
			cons->log("Campath " + filename + ".json does not exist");
			return;
		}
		try {
			{
				std::ifstream ifs("./bakkesmod/data/campaths/" + filename + ".json");  // Output file stream.
				cereal::JSONInputArchive  iarchive(ifs); // Choose binary format, writingdirection.
				iarchive(CEREAL_NVP(currentPath)); //oarchive(saves); // Save the modified instance.
			}
		}
		catch (exception e) {
			cons->log("Could not load campath, invalid json format?");
		}
	}
	else if (command.compare("dolly_path_save") == 0) {
		if (params.size() < 2) {
			cons->log("Usage: dolly_path_save filename");
			return;
		}

		{
			string filename = params.at(1);
			std::ofstream ofs("./bakkesmod/data/campaths/" + filename + ".json", std::ios::out | std::ios::trunc);  // Output file stream.
			cereal::JSONOutputArchive  oarchive(ofs); // Choose binary format, writingdirection.
			oarchive(CEREAL_NVP(currentPath)); //oarchive(saves); // Save the modified instance.
		}
	}


	if (!gw->IsInReplay()) {
		cons->log("You need to be watching a replay to execute this command");
		return;
	}

	if (command.compare("dolly_snapshot") == 0)
	{
		ReplayWrapper sw = gw->GetGameEventAsReplay();
		CamSave save;
		save.timeStamp = sw.GetReplayTimeElapsed();
		CameraWrapper flyCam = gw->GetCamera();
		
		POV currentPov = flyCam.GetPOV();
		save.id = current_id;
		save.location = VectorSerializable(currentPov.location);
		save.rotation = RotatorSerializable(currentPov.rotation);
		save.FOV = currentPov.FOV;
		currentPath.saves.insert(std::make_pair(save.timeStamp, save));
		current_id++;
	}
	else if (command.compare("dolly_deactivate") == 0)
	{
		cons->log("Dolly cam deactived");
		playbackActive = false;
	}
	else if (command.compare("dolly_activate") == 0)
	{
		cons->log("Dolly cam active");
		playbackActive = true;
		run_playback();
	}
	else if (command.compare("dolly_cam_show") == 0) 
	{
		CameraWrapper cam = gw->GetCamera();
		auto location = cam.GetLocation();
		auto rotation = cam.GetRotation();
		cons->log("Location " + to_string(location.X) + "," + to_string(location.Y) + "," + to_string(location.Z));
		cons->log("Rotation " + to_string(rotation.Pitch) + "," + to_string(rotation.Yaw) + "," + to_string(rotation.Roll));
	}
	else if (command.compare("dolly_cam_set") == 0) 
	{
		if (params.size() < 5) {
			cons->log("Usage: dolly_cam_set location|rotation x y z");
			return;
		}
	}
}

void VideoPlugin::onLoad()
{
	gw = gameWrapper;
	cons = console;

	cons->registerNotifier("dolly_snapshot", videoPlugin_onCommand);
	cons->registerNotifier("dolly_activate", videoPlugin_onCommand);
	cons->registerNotifier("dolly_deactivate", videoPlugin_onCommand);

	cons->registerNotifier("dolly_path_save", videoPlugin_onCommand);
	cons->registerNotifier("dolly_path_load", videoPlugin_onCommand);

	cons->registerNotifier("dolly_cam_show", videoPlugin_onCommand);
	cons->registerNotifier("dolly_cam_set", videoPlugin_onCommand);
}

void VideoPlugin::onUnload()
{
}
