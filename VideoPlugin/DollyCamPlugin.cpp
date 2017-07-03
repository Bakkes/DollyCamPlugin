#include "DollyCamPlugin.h"
#include "helpers.h"


#include "Playbacker.h"
#include "Models.h"
#include "calculations.h"
#include "Serialization.h"
#include "IGameApplier.h"


BAKKESMOD_PLUGIN(DollyCamPlugin, "Dollycam plugin", "0.2", 0)

//Known bug, cannot have path after goal if path started before goal
GameWrapper* gw = NULL;
ConsoleWrapper* cons = NULL;

extern std::shared_ptr<Path> currentPath;
extern IGameApplier* gameApplier;

extern int current_id;
extern byte interp_mode;
extern bool renderPath;


extern bool playbackActive;
extern bool firstFrame;
extern int previousID;
extern savetype::iterator currentSnapshotIter;


long long playback() {
	ReplayWrapper sw = gw->GetGameEventAsReplay();

	float replaySeconds = sw.GetReplayTimeElapsed(); //replay seconds are at 30 tickrate? so only updated once every 1/30 s
	float currentTimeInMs = sw.GetSecondsElapsed();
	if ((interp_mode == Linear && currentPath->saves->size() < 2) || (interp_mode == QuadraticBezier && currentPath->saves->size() < 3) || !apply_frame(replaySeconds, currentTimeInMs))
	{
		sw.SetSecondsElapsed(sw.GetReplayTimeElapsed());
	}
	else 
	{

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

void start_playback() 
{
	reset_iterator();
	run_playback();
}


void dolly_onAllCommand(std::vector<std::string> params) 
{
	string command = params.at(0);
	if (command.compare("dolly_path_load") == 0)
	{
		if (params.size() < 2)
		{
			cons->log("Usage: " + params.at(0) + " filename");
			return;
		}
		//camlog.open("camlog.txt");
		string filename = params.at(1);
		if (!file_exists("./bakkesmod/data/campaths/" + filename + ".json"))
		{
			cons->log("Campath " + filename + ".json does not exist");
			return;
		}
		try {
			{
				currentPath->saves->clear();
				std::ifstream ifs("./bakkesmod/data/campaths/" + filename + ".json"); 
				cereal::JSONInputArchive  iarchive(ifs); 
				iarchive(CEREAL_NVP(currentPath)); 
			}

			current_id = 0;
			//need to get the highest ID in the loaded path so we don't get ID collisions, doesn't work to just take the count
			for (savetype::const_iterator it = currentPath->saves->cbegin(); it != currentPath->saves->cend() /* not hoisted */; /* no increment */)
			{
				if (it->second.id >= current_id)
				{
					current_id = it->second.id + 1;
				}
				it++;
			}
			cons->log("Loaded " + filename + ".json with " + to_string(currentPath->saves->size()) + " snapshots. current_id is " + to_string(current_id));
			reset_iterator();
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
			std::ofstream ofs("./bakkesmod/data/campaths/" + filename + ".json", std::ios::out | std::ios::trunc); 
			cereal::JSONOutputArchive  oarchive(ofs); 
			oarchive(CEREAL_NVP(currentPath)); 
		}
	}
	else if (command.compare("dolly_path_clear") == 0)
	{
		currentPath->saves->clear();
		reset_iterator();
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
		for (savetype::const_iterator it = currentPath->saves->cbegin(); it != currentPath->saves->cend() /* not hoisted */; /* no increment */)
		{
			if (it->second.id == id)
			{
				currentPath->saves->erase(it);
				cons->log("Removed snapshot with id " + params.at(1));
				reset_iterator();
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
		for (savetype::const_iterator it = currentPath->saves->cbegin(); it != currentPath->saves->cend() /* not hoisted */; /* no increment */)
		{
			cons->log("ID: " + to_string(it->second.id) + ", [" + to_string_with_precision(it->second.timeStamp, 2) + "][" + to_string_with_precision(it->second.FOV, 2) + "] (" + vector_to_string(it->second.location) + ") (" + rotator_to_string(it->second.rotation) + " )");
			++it;
		}
		cons->log("Current path has " + to_string(currentPath->saves->size()) + " snapshots.");
	}
	else if (command.compare("dolly_snapshot_info") == 0)
	{
		if (params.size() < 2) {
			cons->log("Usage: " + params.at(0) + " id");
			return;
		}

		int id = get_safe_int(params.at(1));
		
		for (savetype::const_iterator it = currentPath->saves->cbegin(); it != currentPath->saves->cend() /* not hoisted */; /* no increment */)
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
}

void dolly_onReplayCommand(std::vector<std::string> params)
{
	string command = params.at(0);
	if (!gw->IsInReplay())
	{
		cons->log("You need to be watching a replay to execute this command");
		return;
	}
	if (command.compare("dolly_cam_show") == 0)
	{
		CameraWrapper cam = gw->GetCamera();
		auto location = cam.GetLocation();
		auto rotation = cam.GetRotation();
		cons->log("Time: " + to_string_with_precision(gw->GetGameEventAsReplay().GetReplayTimeElapsed(), 5));
		cons->log("FOV: " + to_string_with_precision(cam.GetFOV(), 5));
		cons->log("Location " + vector_to_string(location));
		cons->log("Rotation " + rotator_to_string(rotation));
	}
}

void dolly_onFlyCamCommand(std::vector<std::string> params)
{
	string command = params.at(0);
	if (!gw->IsInReplay())
	{
		cons->log("You need to be watching a replay to execute this command");
		return;
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
		for (auto it = currentPath->saves->cbegin(); it != currentPath->saves->cend() /* not hoisted */; /* no increment */)
		{
			if (it->second.id == id)
			{
				ReplayWrapper sw = gw->GetGameEventAsReplay();
				CameraWrapper flyCam = gw->GetCamera();

				CameraSnapshot snapshot;
				snapshot.id = id;
				snapshot.timeStamp = it->second.timeStamp;

				currentPath->saves->erase(it);
				snapshot.frame = sw.GetCurrentReplayFrame();
				snapshot.location = flyCam.GetLocation();
				snapshot.rotation = flyCam.GetRotation();
				snapshot.FOV = flyCam.GetFOV();
				currentPath->saves->insert(std::make_pair(snapshot.timeStamp, snapshot));
				cons->log("Updated snapshot with id " + to_string(id));
				reset_iterator();
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
		currentPath->saves->insert(std::make_pair(save.timeStamp, save));
		//AddKeyFrame(save.frame);
		cons->log("Snapshot saved under id " + to_string(current_id));
		current_id++;
		reset_iterator();
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
			start_playback();
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
		if (params.size() == 2) 
		{
			times = get_safe_int(params.at(1));
		}
		cons->log("Applying chaikin " + to_string(abs(times)) + " times");
		for (unsigned int i = 0; i < abs(times); i++) 
		{
			DollyCamCalculations::apply_chaikin(currentPath);
		}
		reset_iterator();
	}
}

void dolly_onSimulation(std::vector<std::string> params)
{
	string command = params.at(0);
	
	if (command.compare("dolly_simulate") == 0) 
	{
		if (params.size() < 2) {
			cons->log("Usage: " + command + " filename");
			return;
		}
		string filename = params.at(1);

		auto oldGameApplier = gameApplier;
		MockGameApplier* mga = new MockGameApplier("./bakkesmod/data/cam/" + filename + ".data");
		gameApplier = mga;
		int fps = 60;
		float currentTime = currentPath->saves->begin()->first;
		float endTime = (--(currentPath->saves->end()))->first;

		while (currentTime <= endTime) 
		{
			mga->SetTime(currentTime);
			apply_frame(currentTime, currentTime);
			currentTime += (1.f / (float)60);
		}

		delete mga;
		gameApplier = oldGameApplier;
	}
}

void DollyCamPlugin::onLoad()
{
	gw = gameWrapper;
	cons = console;

	gameApplier = new RealGameApplier(gw);

	cons->registerNotifier("dolly_snapshot_take", dolly_onFlyCamCommand);
	cons->registerNotifier("dolly_snapshot_override", dolly_onFlyCamCommand);

	cons->registerNotifier("dolly_snapshot_list", dolly_onAllCommand);
	cons->registerNotifier("dolly_snapshot_info", dolly_onAllCommand);
	cons->registerNotifier("dolly_snapshot_delete", dolly_onAllCommand);

	cons->registerNotifier("dolly_activate", dolly_onFlyCamCommand);
	cons->registerNotifier("dolly_deactivate", dolly_onFlyCamCommand);

	cons->registerNotifier("dolly_path_save", dolly_onAllCommand);
	cons->registerNotifier("dolly_path_load", dolly_onAllCommand);
	cons->registerNotifier("dolly_path_clear", dolly_onAllCommand);

	cons->registerNotifier("dolly_cam_show", dolly_onReplayCommand);
	cons->registerNotifier("dolly_cam_set_location", dolly_onFlyCamCommand);
	cons->registerNotifier("dolly_cam_set_rotation", dolly_onFlyCamCommand);
	cons->registerNotifier("dolly_cam_set_fov", dolly_onFlyCamCommand);
	cons->registerNotifier("dolly_cam_set_time", dolly_onFlyCamCommand);

	cons->registerNotifier("dolly_drawpath_enable", dolly_onFlyCamCommand);
	cons->registerNotifier("dolly_drawpath_disable", dolly_onFlyCamCommand);

	cons->registerNotifier("dolly_interpmode", dolly_onAllCommand);
	cons->registerNotifier("dolly_interp_chaikin", dolly_onFlyCamCommand);

	cons->registerNotifier("dolly_simulate", dolly_onSimulation);
	/*gw->RegisterDrawable([](CanvasWrapper cw) {
		if (!renderPath)
			return;
		if (currentPath->saves->size() < 2)
			return;

		auto it = currentPath->saves->begin();
		while (it != --(currentPath->saves->end())) 
		{
			int id = it->second->id;
			float ts = it->second->timeStamp;
			Vector startLoc = it->second->location;
			Vector startLocText = it->second->location + Vector(0, 0, 50);
			Vector endLoc = (++it)->second->location;
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
	delete gameApplier;
}
