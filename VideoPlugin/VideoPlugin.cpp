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
static int interp_mode = 0;

template <class Archive>
void serialize(Archive & ar, Vector & vector)
{
	ar(CEREAL_NVP(vector.X), CEREAL_NVP(vector.Y), CEREAL_NVP(vector.Z));
}

template <class Archive>
void serialize(Archive & ar, Rotator & rotator)
{
	ar(CEREAL_NVP(rotator.Pitch), CEREAL_NVP(rotator.Yaw), CEREAL_NVP(rotator.Roll));
}

struct CameraSnapshot {
	int id;
	float timeStamp;
	Vector location;
	Rotator rotation;
	float FOV;

	CameraSnapshot() {}
	template <class Archive>
	void serialize(Archive & ar)
	{
		ar(CEREAL_NVP(id), CEREAL_NVP(timeStamp));
		ar(CEREAL_NVP(location));
		ar(CEREAL_NVP(rotation));
		ar(CEREAL_NVP(FOV));
	}
};

struct Path {
	std::map<float, CameraSnapshot> saves;

	Path() {}

	template <class Archive>
	void serialize(Archive & ar)
	{
		ar(CEREAL_NVP(saves));
	}
};

string vector_to_string(Vector v) {
	return to_string_with_precision(v.X, 2) + ", " + to_string_with_precision(v.Y, 2) + ", " + to_string_with_precision(v.Z, 2);
}

string rotator_to_string(Rotator r) {
	return to_string_with_precision(r.Pitch, 2) + ", " + to_string_with_precision(r.Yaw, 2) + ", " + to_string_with_precision(r.Roll, 2);
}

Path currentPath;
bool playbackActive = false;
long long playback() {
	ReplayWrapper sw = gw->GetGameEventAsReplay();

	float replaySeconds = sw.GetReplayTimeElapsed();
	float currentTimeInMs = sw.GetSecondsElapsed();
	CameraSnapshot prevSave;
	CameraSnapshot nextSave;
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

		Vector newLoc = prevSave.location + ((nextSave.location - prevSave.location) * timeElapsed) / snap;
		if (prevSave.rotation.Yaw < -32768 / 2 && nextSave.rotation.Yaw > 32768 / 2) {
			nextSave.rotation.Yaw -= 32768 * 2;
		}
		if (prevSave.rotation.Yaw > 32768 / 2 && nextSave.rotation.Yaw < -32768 / 2) {
			nextSave.rotation.Yaw += 32768 * 2;
		}

		if (prevSave.rotation.Roll < -32768 / 2 && nextSave.rotation.Roll > 32768 / 2) {
			nextSave.rotation.Roll -= 32768 * 2;
		}
		if (prevSave.rotation.Roll > 32768 / 2 && nextSave.rotation.Roll < -32768 / 2) {
			nextSave.rotation.Roll += 32768 * 2;
		}

		if (prevSave.rotation.Pitch < -16364 / 2 && nextSave.rotation.Pitch > 16364 / 2) {
			nextSave.rotation.Pitch -= 16364 * 2;
		}
		if (prevSave.rotation.Pitch > 16364 / 2 && nextSave.rotation.Pitch < -16364 / 2) {
			nextSave.rotation.Pitch += 16364 * 2;
		}

		Rotator newRot;
		newRot.Pitch = prevSave.rotation.Pitch + ((nextSave.rotation.Pitch - prevSave.rotation.Pitch) * timeElapsed / frameDiff);
		newRot.Yaw = prevSave.rotation.Yaw + ((nextSave.rotation.Yaw - prevSave.rotation.Yaw) * timeElapsed / frameDiff);
		newRot.Roll = prevSave.rotation.Roll + ((nextSave.rotation.Roll - prevSave.rotation.Roll) * timeElapsed / frameDiff);

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
	{
		playbackActive = false;
		return;
	}

	gw->SetTimeout([](GameWrapper* gameWrapper) {
		run_playback();
	}, playback());
}

void videoPlugin_onCommand(std::vector<std::string> params)
{
	string command = params.at(0);

	if (command.compare("dolly_path_load") == 0)
	{
		if (params.size() < 2) 
		{
			cons->log("Usage: " + params.at(0) + " filename");
			return;
		}
		string filename = params.at(1);
		if (!file_exists("./bakkesmod/data/campaths/" + filename + ".json")) 
		{
			cons->log("Campath " + filename + ".json does not exist");
			return;
		}
		try {
			{
				currentPath.saves.clear();
				std::ifstream ifs("./bakkesmod/data/campaths/" + filename + ".json");  // Output file stream.
				cereal::JSONInputArchive  iarchive(ifs); // Choose binary format, writingdirection.
				iarchive(CEREAL_NVP(currentPath)); //oarchive(saves); // Save the modified instance.
			}

			current_id = 0;
			//need to get the highest ID in the loaded path so we don't get ID collisions, doesn't work to just take the count
			for (auto it = currentPath.saves.cbegin(); it != currentPath.saves.cend() /* not hoisted */; /* no increment */)
			{
				if (it->second.id >= current_id) 
				{
					current_id = it->second.id + 1;
				}
				it++;
			}
			cons->log("Loaded " + filename + ".json with " + to_string(currentPath.saves.size()) + " snapshots. current_id is " + to_string(current_id));
		}
		catch (exception e) {
			cons->log("Could not load campath, invalid json format?");
		}
	}
	else if (command.compare("dolly_path_save") == 0) 
	{
		if (params.size() < 2) {
			cons->log("Usage: " + params.at(0) + " filename");
			return;
		}

		{
			string filename = params.at(1);
			std::ofstream ofs("./bakkesmod/data/campaths/" + filename + ".json", std::ios::out | std::ios::trunc);  // Output file stream.
			cereal::JSONOutputArchive  oarchive(ofs); // Choose binary format, writingdirection.
			oarchive(CEREAL_NVP(currentPath)); //oarchive(saves); // Save the modified instance.
		}
	}
	else if (command.compare("dolly_interpmode") == 0) 
	{
		if (params.size() < 2) {
			cons->log("Current dolly interp mode is " + interp_mode);
			cons->log("0=linear, 1=bezier");
			return;
		}
		interp_mode = get_safe_int(params.at(1));
	} 
	else if (command.compare("dolly_snapshot_delete") == 0)
	{
		if (params.size() < 2) {
			cons->log("Usage: " + params.at(0) + " id");
			return;
		}
		int id = get_safe_int(params.at(1));
		for (auto it = currentPath.saves.cbegin(); it != currentPath.saves.cend() /* not hoisted */; /* no increment */)
		{
			if (it->second.id == id)
			{
				currentPath.saves.erase(it++);
				cons->log("Removed snapshot with id " + params.at(1));
				return;
			}
			else
			{
				++it;
			}
		}
		cons->log("Snapshot with id " + params.at(1) + " not found, use dolly_snapshot_list to list snapshots");
	}
	else if (command.compare("dolly_snapshot_list") == 0)
	{
		for (auto it = currentPath.saves.cbegin(); it != currentPath.saves.cend() /* not hoisted */; /* no increment */)
		{
			cons->log("ID: " + to_string(it->second.id) + ", [" + to_string_with_precision(it->second.timeStamp, 2) + "][" + to_string_with_precision(it->second.FOV, 2) + "] (" + vector_to_string(it->second.location) + ") (" + rotator_to_string(it->second.rotation) + " )");
			++it;
		}
		cons->log("Current path has " + to_string(currentPath.saves.size()) + " snapshots.");
	}
	else if (command.compare("dolly_snapshot_info") == 0)
	{
		if (params.size() < 2) {
			cons->log("Usage: " + params.at(0) + " id");
			return;
		}
		int id = get_safe_int(params.at(1));
		for (auto it = currentPath.saves.cbegin(); it != currentPath.saves.cend() /* not hoisted */; /* no increment */)
		{
			if (it->second.id == id)
			{
				cons->log("ID " + to_string(it->second.id) + ". FOV: " + to_string(it->second.FOV) + ". Time: " + to_string_with_precision(it->second.timeStamp, 3));
				cons->log("Location " + vector_to_string(it->second.location));
				cons->log("Rotation " + rotator_to_string(it->second.rotation));
				return;
			}
			else
			{
				++it;
			}
		}
	}
	else if (!gw->IsInReplay()) 
	{
		cons->log("You need to be watching a replay to execute this command");
		return;
	}
	else if (command.compare("dolly_cam_show") == 0)
	{
		CameraWrapper cam = gw->GetCamera();
		auto location = cam.GetLocation();
		auto rotation = cam.GetRotation();
		cons->log("Time: " + to_string_with_precision(gw->GetGameEventAsReplay().GetSecondsElapsed(), 5));
		cons->log("FOV: " + to_string_with_precision(cam.GetPOV().FOV, 5));
		cons->log("Location " + vector_to_string(location));
		cons->log("Rotation " + rotator_to_string(rotation));
	} 
	else if (gw->GetCamera().GetCameraState().compare("CameraState_ReplayFly_TA") != 0) 
	{
		cons->log("You need to be in fly camera mode to use this command");
	} 
	else if (command.compare("dolly_snapshot_take") == 0)
	{
		ReplayWrapper sw = gw->GetGameEventAsReplay();
		CameraSnapshot save;
		save.timeStamp = sw.GetReplayTimeElapsed();
		CameraWrapper flyCam = gw->GetCamera();
		
		POV currentPov = flyCam.GetPOV();
		save.id = current_id;
		save.location = currentPov.location;
		save.rotation = currentPov.rotation;
		save.FOV = currentPov.FOV;
		currentPath.saves.insert(std::make_pair(save.timeStamp, save));
		cons->log("Snapshot saved under id " + to_string(current_id));
		current_id++;
	}
	else if (command.compare("dolly_deactivate") == 0)
	{
		cons->log("Dolly cam deactived");
		playbackActive = false;
	}
	else if (command.compare("dolly_activate") == 0)
	{
		
		if (!playbackActive) 
		{
			cons->log("Dolly cam activated");
			playbackActive = true;
			run_playback();
		}
		else 
		{
			cons->log("Dolly cam already active");
		}
	}
	else if (command.compare("dolly_cam_set_location") == 0) 
	{
		if (params.size() < 4) {
			cons->log("Usage: " + params.at(0) + " x y z");
			return;
		}
		float x = get_safe_float(params.at(1));
		float y = get_safe_float(params.at(2));
		float z = get_safe_float(params.at(3));
		gw->GetCamera().SetLocation({ x,y,z });
	}
	else if (command.compare("dolly_cam_set_rotation") == 0)
	{
		if (params.size() < 4) {
			cons->log("Usage: " + params.at(0) + " pitch yaw roll");
			return;
		}
		float pitch = get_safe_float(params.at(1));
		float yaw = get_safe_float(params.at(2));
		float roll = get_safe_float(params.at(3));
		gw->GetCamera().SetRotation({ pitch, yaw, roll });
	}
	else if (command.compare("dolly_cam_set_time") == 0)
	{
		cons->log("Command not supported, please just edit the json manually");
		return;
		if (params.size() < 2) {
			cons->log("Usage: " + params.at(0) + " timestamp");
			return;
		}
		float timestamp = get_safe_float(params.at(1));
		//gw->GetGameEventAsReplay().SetSecondsElapsed(timestamp);
		gw->GetGameEventAsReplay().SkipToTime(timestamp);
	} 
	else if (command.compare("dolly_cam_set_fov") == 0) 
	{
		if (params.size() < 2) {
			cons->log("Usage: " + params.at(0) + " fov");
			return;
		}
		float fov = get_safe_float(params.at(1));

		CameraWrapper flyCam = gw->GetCamera();
		flyCam.SetFOV(fov);
	}
}

void VideoPlugin::onLoad()
{
	gw = gameWrapper;
	cons = console;

	cons->registerNotifier("dolly_snapshot_take", videoPlugin_onCommand);
	cons->registerNotifier("dolly_snapshot_list", videoPlugin_onCommand);
	cons->registerNotifier("dolly_snapshot_info", videoPlugin_onCommand);
	cons->registerNotifier("dolly_snapshot_delete", videoPlugin_onCommand);

	cons->registerNotifier("dolly_activate", videoPlugin_onCommand);
	cons->registerNotifier("dolly_deactivate", videoPlugin_onCommand);

	cons->registerNotifier("dolly_path_save", videoPlugin_onCommand);
	cons->registerNotifier("dolly_path_load", videoPlugin_onCommand);

	cons->registerNotifier("dolly_cam_show", videoPlugin_onCommand);
	cons->registerNotifier("dolly_cam_set_location", videoPlugin_onCommand);
	cons->registerNotifier("dolly_cam_set_rotation", videoPlugin_onCommand);
	cons->registerNotifier("dolly_cam_set_fov", videoPlugin_onCommand);
	cons->registerNotifier("dolly_cam_set_time", videoPlugin_onCommand);

	cons->registerNotifier("dolly_interpmode", videoPlugin_onCommand);
}

void VideoPlugin::onUnload()
{
}
