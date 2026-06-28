#pragma once
#include <rack.hpp>

using namespace rack;

extern Plugin* pluginInstance;

struct KArpScene {
	bool valid = false;

	float bpm = 120.f;
	int chordLen = 16;
	int arpLen = 1;

	float chordSliderValues[16] = {
		0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f
	};

	int chordTypes[16] = {
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0
	};

	int chordOctaves[16] = {
		4, 4, 4, 4, 4, 4, 4, 4,
		4, 4, 4, 4, 4, 4, 4, 4
	};

	bool chordTypeVisible[16] = {
		false, false, false, false,
		false, false, false, false,
		false, false, false, false,
		false, false, false, false
	};

	bool arpMatrix[6][16] = {
		{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false},
		{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false},
		{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false},
		{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false},
		{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false},
		{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}
	};
};

struct KArpSceneHost {
	virtual void captureScene(KArpScene& scene) = 0;
	virtual void applyScene(const KArpScene& scene) = 0;
	virtual bool isKArpRunningForScene() = 0;
	virtual int getSceneCycleCounter() = 0;
	virtual ~KArpSceneHost() {}
};

extern Model* modelKArp;
extern Model* modelKScene;
