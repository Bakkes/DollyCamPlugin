#include "DollyCamPlugin.h"
#include <map>
#include "helpers.h"
#include <fstream>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>

BAKKESMOD_PLUGIN(DollyCamPlugin, "Dollycam plugin", "0.2", 0)


//Known bug, cannot have path after goal if path started before goal
GameWrapper* gw = NULL;
ConsoleWrapper* cons = NULL;

static int current_id = 0;
static byte interp_mode = 3;
static bool renderPath = false;

enum InterpMode
{
	Linear = 0,
	QuadraticBezier = 1,
	RationalBezier = 2,
	Chaikin = 3,
};

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
	int frame;
	Vector location;
	Rotator rotation;
	float FOV;

	CameraSnapshot() {}
	template <class Archive>
	void serialize(Archive & ar)
	{
		ar(CEREAL_NVP(id), CEREAL_NVP(timeStamp), CEREAL_NVP(frame));
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

float calculateParam(float x0, float x1, float x2, float t) {
	return pow(1 - t, 2) * x0 + 2 * t*(1 - t)*x1 + pow(t, 2)*x2;
}

Vector quadraticBezierCurve(Vector p0, Vector p1, Vector p2, float t) {
	return{
		calculateParam(p0.X, p1.X, p2.X, t) ,
		calculateParam(p0.Y, p1.Y, p2.Y, t) ,
		calculateParam(p0.Z, p1.Z, p2.Z, t)
	};
}

ofstream camlog;

template <typename T>
T closest(T num, T one, T two) 
{
	return abs(num - one) < abs(num - two) ? two : one;
}

Path currentPath;
bool playbackActive = false;

bool firstFrame = false;
int lastGenBezierId = -1;

CameraSnapshot usedSave;
long long playback() {
	ReplayWrapper sw = gw->GetGameEventAsReplay();

	float replaySeconds = sw.GetReplayTimeElapsed(); //replay seconds are at 30 tickrate? so only updated once every 1/30 s
	float currentTimeInMs = sw.GetSecondsElapsed();
	

	CameraSnapshot prevSave;
	CameraSnapshot nextSave;
	CameraSnapshot nextNextSave;
	
	if ((interp_mode == 0 && currentPath.saves.size() < 2) || (interp_mode == 1 && currentPath.saves.size() < 3)) //No frames, don't need to execute, need atleast 2 for linterp, 3 for bezier
	{ 
		sw.SetSecondsElapsed(sw.GetReplayTimeElapsed());
		firstFrame = true;
		return 1.f;
	}

	if (currentPath.saves.begin()->first > replaySeconds || (--currentPath.saves.end())->first < replaySeconds) //Replay time doesn't have any snapshots
	{
		sw.SetSecondsElapsed(sw.GetReplayTimeElapsed());
		firstFrame = true;
		return 1.f;
	}


	
	for (auto it = currentPath.saves.begin(); it != currentPath.saves.end(); it++)
	{
		//if (it->first > replaySeconds && prevSave.timeStamp >= 0 && nextSave.timeStamp < 0) //why not ++ the it lol?
		//{
		//	nextSave = it->second;
		//}
		//else if (it->first > replaySeconds && nextSave.timeStamp >= 0) {
		//	nextNextSave = it->second;
		//	break;
		//}


		int it_incs = 0;
		prevSave = it->second;
		if (++it != currentPath.saves.end())
		{
			nextSave = it->second;
			it_incs++;
			if (++it != currentPath.saves.end())
			{
				nextNextSave = it->second;
				it_incs++;
			}
		}

		
		if (prevSave.timeStamp <= replaySeconds && nextSave.timeStamp >= replaySeconds)
		{
			if (nextNextSave.timeStamp < 0 && interp_mode == 1) { //if there is no nextnext, just take last 3 items
				it = (currentPath.saves.end());
				nextNextSave = (--it)->second;
				nextSave = (--it)->second;
				prevSave = (--it)->second;
				it_incs = 0;
			}
			if (lastGenBezierId != prevSave.id)
			{
				for (int i = 0; i < it_incs; i++)
				{
					--it;
				}
				usedSave = it->second;
				if (!firstFrame) 
				{
					usedSave.location = gw->GetCamera().GetLocation();
					usedSave.rotation = gw->GetCamera().GetRotation();
					usedSave.FOV = gw->GetCamera().GetFOV();
					prevSave = usedSave;
				}
				else {
				}
				lastGenBezierId = prevSave.id;
			}
			break;
		}
		else {
			prevSave.timeStamp = -1;
			nextSave.timeStamp = -1;
			nextNextSave.timeStamp = -1;
			for (int i = 0; i < it_incs; i++) 
			{
				--it;
			}
		}

	}

	

	if (nextSave.timeStamp >= 0 && prevSave.timeStamp >= 0)
	{
		prevSave = usedSave;
		if (firstFrame) 
		{

			gw->GetCamera().SetLocation(prevSave.location);
			gw->GetCamera().SetRotation(prevSave.rotation);
			gw->GetCamera().SetFOV(prevSave.FOV);
			firstFrame = false;
		}

		float frameDiff = nextSave.timeStamp - prevSave.timeStamp;
		float timeElapsed = currentTimeInMs - prevSave.timeStamp;

		Rotator snapR = Rotator(frameDiff);
		Vector snap = Vector(frameDiff);
		
		if (prevSave.rotation.Yaw < 0) 
		{
			if (nextSave.rotation.Yaw > 0 && nextSave.rotation.Yaw - prevSave.rotation.Yaw > 32768)
			{
				nextSave.rotation.Yaw -= 32768 * 2;
			}
			if (nextNextSave.rotation.Yaw > 0 && nextNextSave.rotation.Yaw - prevSave.rotation.Yaw > 32768)
			{
				nextNextSave.rotation.Yaw -= 32768 * 2;
			}
		}
		else if (prevSave.rotation.Yaw > 0) {
			if (nextSave.rotation.Yaw < 0 && prevSave.rotation.Yaw - nextSave.rotation.Yaw > 32768)
			{
				nextSave.rotation.Yaw += 32768 * 2;
			}
			if (nextNextSave.rotation.Yaw < 0 && prevSave.rotation.Yaw - nextNextSave.rotation.Yaw > 32768)
			{
				nextNextSave.rotation.Yaw += 32768 * 2;
			}
		}
		if (prevSave.rotation.Roll < 0) 
		{
			if (nextSave.rotation.Roll > 0 && prevSave.rotation.Roll - nextSave.rotation.Roll > 32768)
			{
				nextSave.rotation.Roll -= 32768 * 2;
			}
			if (nextNextSave.rotation.Roll > 0 && nextNextSave.rotation.Roll - prevSave.rotation.Roll > 32768) 
			{
				nextNextSave.rotation.Roll -= 32768 * 2;
			}
		}
		else if (prevSave.rotation.Roll > 0) 
		{
			if (nextSave.rotation.Roll < 0 && prevSave.rotation.Roll - nextSave.rotation.Roll > 32768)
			{
				nextSave.rotation.Roll += 32768 * 2;
			}
			if (nextNextSave.rotation.Roll < 0 && prevSave.rotation.Roll - nextNextSave.rotation.Roll > 32768) 
			{
				nextNextSave.rotation.Roll += 32768 * 2;
			}
		}
		if (prevSave.rotation.Pitch < 0) 
		{
			if (nextSave.rotation.Pitch > 0 && prevSave.rotation.Pitch - nextSave.rotation.Pitch > 16364)
			{
				nextSave.rotation.Pitch -= 16364 * 2;
			}
			if (nextNextSave.rotation.Pitch > 0 && nextNextSave.rotation.Pitch - prevSave.rotation.Pitch > 16364)
			{
				nextNextSave.rotation.Pitch -= 16364 * 2;
			}
		}
		else if (prevSave.rotation.Pitch > 0) 
		{
			if (nextSave.rotation.Pitch < 0 && prevSave.rotation.Pitch - nextSave.rotation.Pitch > 16364)
			{
				nextSave.rotation.Pitch += 16364 * 2;
			}
			if (nextNextSave.rotation.Pitch < 0 && prevSave.rotation.Pitch - nextNextSave.rotation.Pitch > 16364)
			{
				nextNextSave.rotation.Pitch += 16364 * 2;
			}
		}
		
		

		/*if (prevSave.rotation.Roll < -32768 / 2 && nextSave.rotation.Roll > 32768 / 2) {
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
		}*/


		Vector newLoc; 
		Rotator newRot;
		float newFOV = 0;
		float percElapsed = 0;

		switch (interp_mode) {
		case Linear:
			percElapsed = timeElapsed / frameDiff;
			newLoc = prevSave.location + ((nextSave.location - prevSave.location) * timeElapsed) / snap;
			newRot.Pitch = prevSave.rotation.Pitch + ((nextSave.rotation.Pitch - prevSave.rotation.Pitch) * percElapsed);
			newRot.Yaw = prevSave.rotation.Yaw + ((nextSave.rotation.Yaw - prevSave.rotation.Yaw) * percElapsed);
			newRot.Roll = prevSave.rotation.Roll + ((nextSave.rotation.Roll - prevSave.rotation.Roll) * percElapsed);
			newFOV = prevSave.FOV + ((nextSave.FOV - prevSave.FOV) * percElapsed);
			break;
		case QuadraticBezier:
			if (timeElapsed < 0.0001)
				timeElapsed = 0.0001;
			percElapsed = (timeElapsed / (nextNextSave.timeStamp - prevSave.timeStamp));
			if (percElapsed > 1)
				percElapsed = 1;
			newLoc = quadraticBezierCurve(prevSave.location, nextSave.location, nextNextSave.location, percElapsed);
			newRot.Pitch = calculateParam(prevSave.rotation.Pitch, nextSave.rotation.Pitch, nextNextSave.rotation.Pitch, percElapsed);
			newRot.Yaw = calculateParam(prevSave.rotation.Yaw, nextSave.rotation.Yaw, nextNextSave.rotation.Yaw, percElapsed);
			newRot.Roll = calculateParam(prevSave.rotation.Roll, nextSave.rotation.Roll, nextNextSave.rotation.Roll, percElapsed);
			newFOV = calculateParam(prevSave.FOV, nextSave.FOV, nextNextSave.FOV, percElapsed);
			break;
		}
		
		POV p;
		p.location = newLoc + Vector(0);
		p.rotation = newRot + Rotator(0);
		p.FOV = newFOV;
		//camlog << "ID: " + to_string(prevSave.id) + ", [" + to_string_with_precision(currentTimeInMs, 4) + "][" + to_string_with_precision(timeElapsed, 4) + "][" + to_string_with_precision(p.FOV, 2) + "] (" + vector_to_string(p.location) + ") (" + rotator_to_string(p.rotation) + " )\n";
		//camlog.flush();
		//camlog << to_string(prevSave.id) + "," + to_string_with_precision(currentTimeInMs, 10) + ", " + to_string_with_precision(timeElapsed, 10) + "," + to_string_with_precision(usedSave.FOV, 10) + "," + vector_to_string(usedSave.location) + "," + rotator_to_string(usedSave.rotation) + "," + to_string_with_precision(p.FOV, 10) + "," + vector_to_string(p.location) + "," + rotator_to_string(p.rotation) + "," + rotator_to_string(usedSave.rotation) + "\n";
		//camlog.flush();
		gw->GetCamera().SetPOV(p);
	}
	else {
		sw.SetSecondsElapsed(sw.GetReplayTimeElapsed());
		firstFrame = true;
	}

	return 1.f;
}

void AddKeyFrame(int frame) 
{
	return;
	if (!gw->IsInReplay())
		return;
	ReplayWrapper sw = gw->GetGameEventAsReplay();
	sw.AddKeyFrame(frame, "Team0Joined");
}

void RemoveKeyFrame(int frame) {
	return;
	if (!gw->IsInReplay())
		return;
	ReplayWrapper sw = gw->GetGameEventAsReplay();
	sw.RemoveKeyFrame(frame);
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

void apply_chaikin() {
	std::map<float, CameraSnapshot> newPath;
	newPath.insert(std::make_pair(currentPath.saves.begin()->second.timeStamp, currentPath.saves.begin()->second));
	auto it = currentPath.saves.begin();
	while (it != (--currentPath.saves.end()))
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
		newPath.insert(std::make_pair(q1.timeStamp, q1));
		newPath.insert(std::make_pair(q2.timeStamp, q2));
	}


	
	currentPath.saves = newPath;
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
		camlog.open("camlog.txt");
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
	else if (command.compare("dolly_path_clear") == 0)
	{
		currentPath.saves.clear();
	}
	else if (command.compare("dolly_interpmode") == 0) 
	{
		if (params.size() < 2) {
			cons->log("Current dolly interp mode is " + to_string(interp_mode));
			cons->log("0=linear, 1=quadratic bezier");
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
				currentPath.saves.erase(it);
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
				if (params.size() == 3) {
					string action = params.at(2);
					if (action.compare("set") == 0) {
						gw->GetCamera().SetLocation(it->second.location);
						gw->GetCamera().SetRotation(it->second.rotation);
						gw->GetCamera().SetFOV(it->second.FOV);
					}
				}

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
		cons->log("Time: " + to_string_with_precision(gw->GetGameEventAsReplay().GetReplayTimeElapsed(), 5));
		cons->log("FOV: " + to_string_with_precision(cam.GetFOV(), 5));
		cons->log("Location " + vector_to_string(location));
		cons->log("Rotation " + rotator_to_string(rotation));
	} 
	else if (gw->GetCamera().GetCameraState().compare("CameraState_ReplayFly_TA") != 0) 
	{
		cons->log("You need to be in fly camera mode to use this command");
	}
	else if (command.compare("dolly_snapshot_override") == 0)
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
				ReplayWrapper sw = gw->GetGameEventAsReplay();
				CameraWrapper flyCam = gw->GetCamera();

				CameraSnapshot snapshot;
				snapshot.id = id;
				snapshot.timeStamp = it->second.timeStamp;

				currentPath.saves.erase(it);
				snapshot.frame = sw.GetCurrentReplayFrame();
				snapshot.location = flyCam.GetLocation();
				snapshot.rotation = flyCam.GetRotation();
				snapshot.FOV = flyCam.GetFOV();
				currentPath.saves.insert(std::make_pair(snapshot.timeStamp, snapshot));
				cons->log("Updated snapshot with id " + to_string(id));
				return;
			}
			else
			{
				++it;
			}
		}
	}
	else if (command.compare("dolly_snapshot_take") == 0)
	{
		ReplayWrapper sw = gw->GetGameEventAsReplay();
		CameraSnapshot save;
		save.timeStamp = sw.GetReplayTimeElapsed();
		CameraWrapper flyCam = gw->GetCamera();
		
		save.id = current_id;
		save.frame = sw.GetCurrentReplayFrame();
		save.location = flyCam.GetLocation();
		save.rotation = flyCam.GetRotation();
		save.FOV = flyCam.GetFOV();
		currentPath.saves.insert(std::make_pair(save.timeStamp, save));
		AddKeyFrame(save.frame);
		cons->log("Snapshot saved under id " + to_string(current_id));
		current_id++;
	}
	else if (command.compare("dolly_deactivate") == 0)
	{
		cons->log("Dolly cam deactived");
		CameraWrapper flyCam = gw->GetCamera();
		if (!flyCam.GetCameraAsActor().IsNull()) {
			flyCam.SetLockedFOV(false);
		}
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
	else if (command.compare("dolly_drawpath_enable") == 0)
	{
		renderPath = true;
	}
	else if (command.compare("dolly_drawpath_disable") == 0)
	{
		renderPath = true;
	}
	else if (command.compare("dolly_interp_chaikin") == 0)
	{
		int times = 1;
		if (params.size() == 2) {
			times = get_safe_int(params.at(1));
		}
		for (unsigned int i = 0; i < abs(times); i++) {
			apply_chaikin();
		}
	}
}

void DollyCamPlugin::onLoad()
{
	gw = gameWrapper;
	cons = console;

	cons->registerNotifier("dolly_snapshot_take", videoPlugin_onCommand);
	cons->registerNotifier("dolly_snapshot_list", videoPlugin_onCommand);
	cons->registerNotifier("dolly_snapshot_info", videoPlugin_onCommand);
	cons->registerNotifier("dolly_snapshot_override", videoPlugin_onCommand);
	cons->registerNotifier("dolly_snapshot_delete", videoPlugin_onCommand);

	cons->registerNotifier("dolly_activate", videoPlugin_onCommand);
	cons->registerNotifier("dolly_deactivate", videoPlugin_onCommand);

	cons->registerNotifier("dolly_path_save", videoPlugin_onCommand);
	cons->registerNotifier("dolly_path_load", videoPlugin_onCommand);
	cons->registerNotifier("dolly_path_clear", videoPlugin_onCommand);

	cons->registerNotifier("dolly_cam_show", videoPlugin_onCommand);
	cons->registerNotifier("dolly_cam_set_location", videoPlugin_onCommand);
	cons->registerNotifier("dolly_cam_set_rotation", videoPlugin_onCommand);
	cons->registerNotifier("dolly_cam_set_fov", videoPlugin_onCommand);
	cons->registerNotifier("dolly_cam_set_time", videoPlugin_onCommand);

	cons->registerNotifier("dolly_drawpath_enable", videoPlugin_onCommand);
	cons->registerNotifier("dolly_drawpath_disable", videoPlugin_onCommand);

	cons->registerNotifier("dolly_interpmode", videoPlugin_onCommand);
	cons->registerNotifier("dolly_interp_chaikin", videoPlugin_onCommand);
	/*gw->RegisterDrawable([](CanvasWrapper cw) {
		if (!renderPath)
			return;
		if (currentPath.saves.size() < 2)
			return;

		auto it = currentPath.saves.begin();
		while (it != --(currentPath.saves.end())) 
		{
			int id = it->second.id;
			float ts = it->second.timeStamp;
			Vector startLoc = it->second.location;
			Vector startLocText = it->second.location + Vector(0, 0, 50);
			Vector endLoc = (++it)->second.location;
			Vector2 startProj = cw.Project(startLoc);
			Vector2 startProjText = cw.Project(startLocText);
			Vector2 endProj = cw.Project(endLoc);

			cw.SetColor(255, 0, 0, 150);
			cw.DrawLine(startProj, endProj);
			cw.SetPosition(startProjText);
			cw.DrawString("ID: " + to_string(id) + " @ " + to_string_with_precision(ts, 4));
		}
	});*/
}

void DollyCamPlugin::onUnload()
{
}
