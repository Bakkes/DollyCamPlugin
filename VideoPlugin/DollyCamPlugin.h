#pragma once
#pragma comment(lib, "BakkesMod.lib")
#include "bakkesmod\plugin\bakkesmodplugin.h"

class DollyCamPlugin : public BakkesMod::Plugin::BakkesModPlugin
{
public:
	virtual void onLoad();
	virtual void onUnload();
	void onTick(std::string ignoredParam);
};

template<typename T>
void write_pod(std::ofstream& out, T& t)
{
	out.write(reinterpret_cast<char*>(&t), sizeof(T));
}

template<typename T>
void read_pod(std::ifstream& in, T& t)
{
	in.read(reinterpret_cast<char*>(&t), sizeof(T));
}