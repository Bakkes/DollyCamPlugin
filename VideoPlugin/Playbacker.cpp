#include "Playbacker.h"
#include "calculations.h"

std::shared_ptr<Path> currentPath(new Path());
IGameApplier* gameApplier;

int current_id = 0;
byte interp_mode = 2;
bool renderPath = false;

bool playbackActive = false;
bool firstFrame = false;
int previousID = -1;

savetype::iterator currentSnapshotIter;
CameraSnapshot currentSnapshot;

//Two types of interp, frame by frame (n-1, n, n+1) or full interp, 0...N
std::shared_ptr<Path> currentUsedPath(new Path());
bool needsNewCopy = true;

bool apply_frame(float replaySeconds, float currentTimeInMs)
{
	bool needsRefresh = true;
	if (currentSnapshotIter != currentUsedPath->saves->end())
	{
		savetype::iterator current_next = std::next(currentSnapshotIter, 1);
		if (current_next != currentUsedPath->saves->end() && currentSnapshotIter->second.timeStamp <= replaySeconds && current_next->second.timeStamp >= replaySeconds) {
			needsRefresh = false;
		}
		savetype::iterator current_next_next = std::next(current_next, 1);
		if (interp_mode != Linear && interp_mode != nBezier && current_next_next != currentUsedPath->saves->end() && current_next->second.timeStamp <= replaySeconds && current_next_next->second.timeStamp >= replaySeconds) {
			needsRefresh = false;
		}
	}
	if (needsRefresh)
	{
		currentSnapshotIter = find_current_snapshot(currentTimeInMs); //optimize this
	}
	if (currentSnapshotIter == currentUsedPath->saves->end() || currentSnapshotIter->first > replaySeconds)
	{
		if (needsNewCopy)
		{
			currentUsedPath->saves->clear();
			currentUsedPath->saves->insert(currentPath->saves->begin(), currentPath->saves->end());
			if (currentUsedPath->saves->size() > 2)
			{
				for (auto it = (currentUsedPath->saves->begin()); it != --(--(currentUsedPath->saves->end())); it++)
				{
					auto prev = &(it->second.rotation);
					auto next = &((++it)->second.rotation);
					auto nextNext = &((++it)->second.rotation);
					//DollyCamCalculations::fix_rotators(prev, next, nextNext);
					--it;
					--it;
				}
			}

			reset_iterator();
			needsNewCopy = false;
		}
		previousID = -1;
		return false;
	}
	needsNewCopy = true;

	savetype::iterator nextSnapshot = std::next(currentSnapshotIter, 1);

	if (previousID != currentSnapshotIter->second.id) //new snapshot selected
	{
		currentSnapshot = currentSnapshotIter->second;
		if (currentSnapshotIter == currentUsedPath->saves->begin())//Determine if it just hit the first frame
		{
			gameApplier->SetPOV(currentSnapshot.location, currentSnapshot.rotation, currentSnapshot.FOV);
		}
		else if(interp_mode != nBezier)
		{
			POV curPov = gameApplier->GetPOV();
			currentSnapshot.location = curPov.location;
			currentSnapshot.rotation = curPov.rotation;
			currentSnapshot.FOV = curPov.FOV;
		}
	}



	CameraSnapshot prevSave = currentSnapshot;
	CameraSnapshot nextSave = nextSnapshot->second;
	float endTime = nextSave.timeStamp;

	CameraSnapshot nextNextSave;
	if (interp_mode == QuadraticBezier)
	{
		nextNextSave = std::next(nextSnapshot, 1)->second;
		endTime = nextNextSave.timeStamp;
	}
	else
	{
		nextNextSave.timeStamp = -1;
	}

	if (endTime < replaySeconds)
		return false;

	float frameDiff = nextSave.timeStamp - prevSave.timeStamp;
	float timeElapsed = currentTimeInMs - prevSave.timeStamp;

	Rotator snapR = Rotator(frameDiff);
	Vector snap = Vector(frameDiff);

	//DollyCamCalculations::fix_rotators(&prevSave.rotation, &nextSave.rotation, &nextNextSave.rotation);

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
		newLoc = DollyCamCalculations::quadraticBezierCurve(prevSave.location, nextSave.location, nextNextSave.location, percElapsed);
		newRot.Pitch = DollyCamCalculations::calculateBezierParam(prevSave.rotation.Pitch, nextSave.rotation.Pitch, nextNextSave.rotation.Pitch, percElapsed);
		newRot.Yaw = DollyCamCalculations::calculateBezierParam(prevSave.rotation.Yaw, nextSave.rotation.Yaw, nextNextSave.rotation.Yaw, percElapsed);
		newRot.Roll = DollyCamCalculations::calculateBezierParam(prevSave.rotation.Roll, nextSave.rotation.Roll, nextNextSave.rotation.Roll, percElapsed);
		newFOV = DollyCamCalculations::calculateBezierParam(prevSave.FOV, nextSave.FOV, nextNextSave.FOV, percElapsed);
		break;
	case nBezier:
		float total = (--currentUsedPath->saves->end())->first - currentUsedPath->saves->begin()->first;
		percElapsed = (currentTimeInMs - currentUsedPath->saves->begin()->first) / total;
		DollyCamCalculations::nBezierCurve(currentUsedPath->saves, percElapsed, newLoc, newRot, newFOV);
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
	gameApplier->SetPOV(p.location, p.rotation, p.FOV);
	previousID = currentSnapshot.id;
	return true;
}

savetype::iterator find_current_snapshot(float currentTime)
{
//If it reached this it is totally out of sync

	savetype::iterator current_it = currentUsedPath->saves->end();
	savetype::iterator next_it = currentUsedPath->saves->end();
	savetype::iterator next_next_it = currentUsedPath->saves->end();
	for (savetype::iterator it = currentUsedPath->saves->begin(); it != currentUsedPath->saves->end(); it++)
	{
		int it_incs = 0;
		current_it = it;
		if (std::next(it, 1) != currentUsedPath->saves->end())
		{
			next_it = std::next(it, 1);
			it_incs++;
			if (std::next(next_it, 1) != currentUsedPath->saves->end())
			{
				next_next_it = std::next(next_it, 1);
				it_incs++;
			}
		}


		if (current_it->second.timeStamp <= currentTime && next_it->second.timeStamp >= currentTime) //found our point in time
		{
			if (it_incs == 1 && interp_mode == QuadraticBezier) { //if there is no nextnext, just take last 3 items
				it = (currentUsedPath->saves->end());
				next_next_it = (--it);
				next_it = (--it);
				current_it = (--it);
				it_incs = 0;
			}
			return current_it;
		}
	}
	return currentUsedPath->saves->begin();
}

void reset_iterator()
{
	currentSnapshotIter = currentUsedPath->saves->end();
}
