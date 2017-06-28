#pragma once
#include "Models.h"
#include "helpers.h"
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

template <class Archive>
void serialize(Archive & ar, CameraSnapshot & snapshot)
{
	ar(CEREAL_NVP(snapshot.id), CEREAL_NVP(snapshot.timeStamp), CEREAL_NVP(snapshot.frame));
	ar(CEREAL_NVP(snapshot.location));
	ar(CEREAL_NVP(snapshot.rotation));
	ar(CEREAL_NVP(snapshot.FOV));
}

template <class Archive>
void serialize(Archive & ar, Path & path)
{
	ar(CEREAL_NVP(path.saves));
}


string vector_to_string(Vector v) {
	return to_string_with_precision(v.X, 2) + ", " + to_string_with_precision(v.Y, 2) + ", " + to_string_with_precision(v.Z, 2);
}

string rotator_to_string(Rotator r) {
	return to_string_with_precision(r.Pitch, 2) + ", " + to_string_with_precision(r.Yaw, 2) + ", " + to_string_with_precision(r.Roll, 2);
}