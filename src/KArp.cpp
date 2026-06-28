// KARP_5H2_CHORD_CLOCK_ARP_SUBDIV - panel CLK OUT placement
#include "plugin.hpp"
#include <cmath>
#include <cstdio>

struct KArp : Module, KArpSceneHost {
	enum ChordType {
		MAJOR_CHORD,
		MINOR_CHORD,
		SUS4_CHORD,
		DIM_CHORD,
		AUG_CHORD,
		SUS2_CHORD,
		NUM_CHORD_TYPES
	};

	enum ParamIds {
		CHORD_LEN_PARAM,
		ARP_LEN_PARAM,
		OCT_PARAM,
		BPM_PARAM,

		CHORD_1_PARAM,
		CHORD_2_PARAM,
		CHORD_3_PARAM,
		CHORD_4_PARAM,
		CHORD_5_PARAM,
		CHORD_6_PARAM,
		CHORD_7_PARAM,
		CHORD_8_PARAM,
		CHORD_9_PARAM,
		CHORD_10_PARAM,
		CHORD_11_PARAM,
		CHORD_12_PARAM,
		CHORD_13_PARAM,
		CHORD_14_PARAM,
		CHORD_15_PARAM,
		CHORD_16_PARAM,

		MODE_1_PARAM,
		MODE_2_PARAM,
		MODE_3_PARAM,
		MODE_4_PARAM,
		MODE_5_PARAM,
		MODE_6_PARAM,
		MODE_7_PARAM,
		MODE_8_PARAM,
		MODE_9_PARAM,
		MODE_10_PARAM,
		MODE_11_PARAM,
		MODE_12_PARAM,
		MODE_13_PARAM,
		MODE_14_PARAM,
		MODE_15_PARAM,
		MODE_16_PARAM,

		RUN_PARAM,

		NUM_PARAMS
	};

	enum InputIds {
		RUN_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};

	enum OutputIds {
		ARP_VOCT_OUTPUT,
		ARP_GATE_OUTPUT,
		CHORD_VOCT_OUTPUT,
		CHORD_GATE_OUTPUT,
		ROOT_VOCT_OUTPUT,
		CLK_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		MODE_1_LIGHT,
		MODE_2_LIGHT,
		MODE_3_LIGHT,
		MODE_4_LIGHT,
		MODE_5_LIGHT,
		MODE_6_LIGHT,
		MODE_7_LIGHT,
		MODE_8_LIGHT,
		MODE_9_LIGHT,
		MODE_10_LIGHT,
		MODE_11_LIGHT,
		MODE_12_LIGHT,
		MODE_13_LIGHT,
		MODE_14_LIGHT,
		MODE_15_LIGHT,
		MODE_16_LIGHT,
		NUM_LIGHTS
	};

    int activeChord = 0;
    int activeArpStep = 0;
	int chordOctaves[16] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
	int chordTypes[16] = {
		MAJOR_CHORD, MAJOR_CHORD, MAJOR_CHORD, MAJOR_CHORD,
		MAJOR_CHORD, MAJOR_CHORD, MAJOR_CHORD, MAJOR_CHORD,
		MAJOR_CHORD, MAJOR_CHORD, MAJOR_CHORD, MAJOR_CHORD,
		MAJOR_CHORD, MAJOR_CHORD, MAJOR_CHORD, MAJOR_CHORD
	};
	bool chordTypeVisible[16] = {
		false, false, false, false,
		false, false, false, false,
		false, false, false, false,
		false, false, false, false
	};
	float chordSliderValues[16] = {
		0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f
	};

	// Chapitre 5E-1 : etat visuel initial de la matrice d'arpege.
	// Lignes de bas en haut : R, 3, 5, ↑R, ↑3, ↑5.
	// Pour l'instant, ces boutons sont uniquement visuels.
	bool arpMatrix[6][16] = {
		{true,  false, false, false, false, false, false, false, false, false, false, false, false, false, false, false},
		{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false},
		{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false},
		{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false},
		{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false},
		{false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false}
	};
	int lastEditedChord = 0;

	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger runTrigger;
	dsp::SchmittTrigger resetTrigger;
	bool firstClockAfterStart = true;
	bool previousRunActive = false;

	// Architecture temporelle interne.
	// Le BPM interne definit la duree d'un accord.
	// ARP_LEN subdivise cette duree pour faire avancer la matrice d'arpege.
	bool chordClockSeen = false;
	bool haveClockPeriod = false;
	float timeSinceLastChordClock = 0.f;
	float lastChordClockPeriod = 0.f;
	float chordGateRetriggerTimer = 0.f;
	float clockOutPulseTimer = 0.f;
	float sceneChordGateMuteTimer = 0.f;
	float sceneChordGateHoldTimer = 0.f;
	bool pendingSceneStartRetrigger = false;

	// Derniere sortie ARP non vide.
	// Elle permet de garder la hauteur stable pendant une colonne de silence,
	// tandis que la gate retombe a 0 V.
	float lastArpVoltages[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
	int lastArpVoiceCount = 0;
	int sceneCycleCounter = 0;


	KArp() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(CHORD_LEN_PARAM, 1.f, 16.f, 16.f, "Chord length");
		configParam(ARP_LEN_PARAM, 1.f, 16.f, 1.f, "Arpeggio length");
		configParam(OCT_PARAM, 0.f, 8.f, 4.f, "Octave");
		configParam(BPM_PARAM, 1.f, 300.f, 120.f, "Clock tempo", " BPM");
		configSwitch(RUN_PARAM, 0.f, 1.f, 0.f, "Run", {"Off", "On"});
		configInput(RUN_INPUT, "Run");
		configInput(RESET_INPUT, "Reset sequence");

		for (int i = 0; i < 16; i++) {
			configParam(CHORD_1_PARAM + i, 0.f, 1.f, 0.f, "Chord slider");
			configButton(MODE_1_PARAM + i, "Chord type");
		}

		paramQuantities[CHORD_LEN_PARAM]->snapEnabled = true;
		paramQuantities[ARP_LEN_PARAM]->snapEnabled = true;
		paramQuantities[OCT_PARAM]->snapEnabled = true;
	}

	static const char* getChordTypeName(int type) {
		switch (type) {
			case MAJOR_CHORD: return "maj";
			case MINOR_CHORD: return "min";
			case SUS4_CHORD: return "sus4";
			case SUS2_CHORD: return "sus2";
			case DIM_CHORD: return "dim";
			case AUG_CHORD: return "aug";
			default: return "maj";
		}
	}

	int getChordRoot(int index) {
		if (index < 0) {
			index = 0;
		}
		if (index > 15) {
			index = 15;
		}

		int root = (int)std::round(params[CHORD_1_PARAM + index].getValue() * 11.f);
		if (root < 0) {
			root = 0;
		}
		if (root > 11) {
			root = 11;
		}
		return root;
	}


	int getChordThirdInterval(int type) {
		switch (type) {
			case MINOR_CHORD: return 3;
			case SUS2_CHORD: return 2;
			case SUS4_CHORD: return 5;
			case DIM_CHORD: return 3;
			case AUG_CHORD: return 4;
			case MAJOR_CHORD:
			default: return 4;
		}
	}

	int getChordFifthInterval(int type) {
		switch (type) {
			case DIM_CHORD: return 6;
			case AUG_CHORD: return 8;
			case MAJOR_CHORD:
			case MINOR_CHORD:
			case SUS2_CHORD:
			case SUS4_CHORD:
			default: return 7;
		}
	}

	float getRootVoltage(int root, int octave) {
		return (float)(octave - 4) + ((float)root / 12.f);
	}

	float getChordNoteVoltage(int index, int noteIndex) {
		if (index < 0) {
			index = 0;
		}
		if (index > 15) {
			index = 15;
		}

		int root = getChordRoot(index);
		int octave = chordOctaves[index];
		int type = chordTypes[index];

		int third = getChordThirdInterval(type);
		int fifth = getChordFifthInterval(type);

		int interval = 0;
		switch (noteIndex) {
			case 0: interval = 0; break;
			case 1: interval = third; break;
			case 2: interval = fifth; break;
			case 3: interval = 12; break;
			case 4: interval = third + 12; break;
			case 5: interval = fifth + 12; break;
			default: interval = 0; break;
		}

		return getRootVoltage(root, octave) + ((float)interval / 12.f);
	}

	bool isRunning() {
		return params[RUN_PARAM].getValue() >= 0.5f;
	}

	void setRunState(bool running) {
		params[RUN_PARAM].setValue(running ? 1.f : 0.f);
	}

	int getChordLen() {
		int chordLen = (int)std::round(params[CHORD_LEN_PARAM].getValue());
		if (chordLen < 1) {
			chordLen = 1;
		}
		if (chordLen > 16) {
			chordLen = 16;
		}
		return chordLen;
	}

	int getArpLen() {
		int arpLen = (int)std::round(params[ARP_LEN_PARAM].getValue());
		if (arpLen < 1) {
			arpLen = 1;
		}
		if (arpLen > 16) {
			arpLen = 16;
		}
		return arpLen;
	}

	void captureScene(KArpScene& scene) override {
		scene.valid = true;

		scene.bpm = params[BPM_PARAM].getValue();
		if (scene.bpm < 1.f) scene.bpm = 1.f;
		if (scene.bpm > 300.f) scene.bpm = 300.f;

		scene.chordLen = getChordLen();
		scene.arpLen = getArpLen();

		for (int i = 0; i < 16; i++) {
			scene.chordSliderValues[i] = chordSliderValues[i];
			if (i < scene.chordLen) {
				scene.chordSliderValues[i] = params[CHORD_1_PARAM + i].getValue();
			}

			scene.chordTypes[i] = chordTypes[i];
			scene.chordOctaves[i] = chordOctaves[i];
			scene.chordTypeVisible[i] = chordTypeVisible[i];
		}

		for (int row = 0; row < 6; row++) {
			for (int col = 0; col < 16; col++) {
				scene.arpMatrix[row][col] = arpMatrix[row][col];
			}
		}
	}

	void applyScene(const KArpScene& scene) override {
		if (!scene.valid) {
			return;
		}

		float bpm = scene.bpm;
		if (bpm < 1.f) bpm = 1.f;
		if (bpm > 300.f) bpm = 300.f;

		int newChordLen = scene.chordLen;
		if (newChordLen < 1) newChordLen = 1;
		if (newChordLen > 16) newChordLen = 16;

		int newArpLen = scene.arpLen;
		if (newArpLen < 1) newArpLen = 1;
		if (newArpLen > 16) newArpLen = 16;

		params[BPM_PARAM].setValue(bpm);
		params[CHORD_LEN_PARAM].setValue((float)newChordLen);
		params[ARP_LEN_PARAM].setValue((float)newArpLen);

		for (int i = 0; i < 16; i++) {
			float sliderValue = scene.chordSliderValues[i];
			if (sliderValue < 0.f) sliderValue = 0.f;
			if (sliderValue > 1.f) sliderValue = 1.f;

			int type = scene.chordTypes[i];
			if (type < 0 || type >= NUM_CHORD_TYPES) type = MAJOR_CHORD;

			int octave = scene.chordOctaves[i];
			if (octave < 0) octave = 0;
			if (octave > 8) octave = 8;

			chordSliderValues[i] = sliderValue;
			params[CHORD_1_PARAM + i].setValue(sliderValue);
			chordTypes[i] = type;
			chordOctaves[i] = octave;
			chordTypeVisible[i] = scene.chordTypeVisible[i];
		}

		for (int row = 0; row < 6; row++) {
			for (int col = 0; col < 16; col++) {
				arpMatrix[row][col] = scene.arpMatrix[row][col];
			}
		}

		// Rappel de scène :
		// une scène rappelée repart toujours du début musical.
		// On ne restaure pas l'ancien état d'exécution.
		activeChord = 0;
		activeArpStep = 0;
		lastEditedChord = 0;

		params[OCT_PARAM].setValue((float)chordOctaves[0]);

		firstClockAfterStart = true;
		timeSinceLastChordClock = 0.f;
		lastChordClockPeriod = 0.f;
		chordGateRetriggerTimer = 0.f;
		clockOutPulseTimer = 0.f;

		// Correction ciblée CHORD_GATE :
		// au rappel d'une scène, le premier accord reçoit un vrai front
		// puis une gate tenue pendant toute sa durée.
		sceneChordGateMuteTimer = 0.010f;
		sceneChordGateHoldTimer = 60.f / bpm;

		if (isRunning()) {
			chordClockSeen = true;
			haveClockPeriod = true;
			lastChordClockPeriod = 60.f / bpm;
			clockOutPulseTimer = 0.005f;
			pendingSceneStartRetrigger = false;
		}
		else {
			chordClockSeen = false;
			haveClockPeriod = false;
			pendingSceneStartRetrigger = true;
		}

		lastArpVoiceCount = 0;
		sceneCycleCounter = 0;
		for (int v = 0; v < 6; v++) {
			lastArpVoltages[v] = 0.f;
		}
	}

	bool isKArpRunningForScene() override {
		return isRunning();
	}

	int getSceneCycleCounter() override {
		return sceneCycleCounter;
	}

	bool isChordInSequence(int index) {
		return index >= 0 && index < getChordLen();
	}

	void setActiveChordFromSequencer(int index) {
		int chordLen = getChordLen();
		if (index < 0) {
			index = 0;
		}
		if (index >= chordLen) {
			index = chordLen - 1;
		}
		if (index < 0) {
			index = 0;
		}

		activeChord = index;
		lastEditedChord = index;
		params[OCT_PARAM].setValue((float)chordOctaves[index]);
	}

void resetChordSequencer() {
        setActiveChordFromSequencer(0);
        activeArpStep = 0;
        firstClockAfterStart = true;
        chordClockSeen = false;
        haveClockPeriod = false;
        timeSinceLastChordClock = 0.f;
        lastChordClockPeriod = 0.f;
        chordGateRetriggerTimer = 0.f;
        clockOutPulseTimer = 0.f;
        sceneChordGateMuteTimer = 0.f;
        sceneChordGateHoldTimer = 0.f;
        pendingSceneStartRetrigger = false;
        sceneCycleCounter = 0;
}

void advanceChordOnly() {
        int chordLen = getChordLen();
        if (chordLen < 1) {
                chordLen = 1;
        }

        int nextChord = activeChord + 1;
        if (nextChord >= chordLen) {
                nextChord = 0;
                sceneCycleCounter++;
        }

        setActiveChordFromSequencer(nextChord);
}

void advanceChordSequencer() {
        int arpLen = getArpLen();
        if (arpLen < 1) {
                arpLen = 1;
        }

        activeArpStep++;

        if (activeArpStep >= arpLen) {
                activeArpStep = 0;

                int chordLen = getChordLen();
                if (chordLen < 1) {
                        chordLen = 1;
                }

                int nextChord = activeChord + 1;
                if (nextChord >= chordLen) {
                        nextChord = 0;
                        sceneCycleCounter++;
                }

                setActiveChordFromSequencer(nextChord);
        }
}

int buildActiveArpVoltages(float voltages[6]) {
        int arpLen = getArpLen();
        if (activeArpStep < 0) {
                activeArpStep = 0;
        }
        if (activeArpStep >= arpLen) {
                activeArpStep = arpLen - 1;
        }
        if (activeArpStep < 0) {
                activeArpStep = 0;
        }

        int voiceCount = 0;

        // Chapitre 5F-2 : lecture musicale de la colonne active.
        // Ordre des canaux Rack :
        // canal 1 = R, canal 2 = 3, canal 3 = 5,
        // canal 4 = R+octave, canal 5 = 3+octave, canal 6 = 5+octave.
        for (int row = 0; row < 6; row++) {
                if (arpMatrix[row][activeArpStep]) {
                        voltages[voiceCount] = getChordNoteVoltage(activeChord, row);
                        voiceCount++;
                }
        }

        return voiceCount;
}
	void resetChordState() {
         activeChord = 0;
         activeArpStep = 0;
         lastEditedChord = 0;
         firstClockAfterStart = true;
         previousRunActive = false;
         chordClockSeen = false;
         haveClockPeriod = false;
         timeSinceLastChordClock = 0.f;
         lastChordClockPeriod = 0.f;
         chordGateRetriggerTimer = 0.f;
         clockOutPulseTimer = 0.f;
         sceneChordGateMuteTimer = 0.f;
         sceneChordGateHoldTimer = 0.f;
         pendingSceneStartRetrigger = false;
         lastArpVoiceCount = 0;
         sceneCycleCounter = 0;
         for (int v = 0; v < 6; v++) {
             lastArpVoltages[v] = 0.f;
         }
		clockTrigger.reset();
		runTrigger.reset();
		resetTrigger.reset();

		params[CHORD_LEN_PARAM].setValue(16.f);
		params[ARP_LEN_PARAM].setValue(1.f);
		params[OCT_PARAM].setValue(4.f);
		params[BPM_PARAM].setValue(120.f);
		params[RUN_PARAM].setValue(0.f);

		for (int i = 0; i < 16; i++) {
			params[CHORD_1_PARAM + i].setValue(0.f);
			params[MODE_1_PARAM + i].setValue(0.f);
			chordSliderValues[i] = 0.f;
			chordOctaves[i] = 4;
			chordTypes[i] = MAJOR_CHORD;
			chordTypeVisible[i] = false;
			for (int row = 0; row < 6; row++) {
				arpMatrix[row][i] = (row == 0 && i == 0);
			}
		}
	}


	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_t* arpMatrixJ = json_array();
		for (int row = 0; row < 6; row++) {
			json_t* rowJ = json_array();
			for (int col = 0; col < 16; col++) {
				json_array_append_new(rowJ, json_boolean(arpMatrix[row][col]));
			}
			json_array_append_new(arpMatrixJ, rowJ);
		}
		json_object_set_new(rootJ, "arpMatrix", arpMatrixJ);

		json_t* chordTypesJ = json_array();
		json_t* chordTypeVisibleJ = json_array();
		json_t* chordOctavesJ = json_array();
		json_t* chordSliderValuesJ = json_array();
		for (int i = 0; i < 16; i++) {
			json_array_append_new(chordTypesJ, json_integer(chordTypes[i]));
			json_array_append_new(chordTypeVisibleJ, json_boolean(chordTypeVisible[i]));
			json_array_append_new(chordOctavesJ, json_integer(chordOctaves[i]));
			json_array_append_new(chordSliderValuesJ, json_real(chordSliderValues[i]));
		}
		json_object_set_new(rootJ, "chordTypes", chordTypesJ);
		json_object_set_new(rootJ, "chordTypeVisible", chordTypeVisibleJ);
		json_object_set_new(rootJ, "chordOctaves", chordOctavesJ);
		json_object_set_new(rootJ, "chordSliderValues", chordSliderValuesJ);

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* arpMatrixJ = json_object_get(rootJ, "arpMatrix");
		if (arpMatrixJ) {
			for (int row = 0; row < 6; row++) {
				json_t* rowJ = json_array_get(arpMatrixJ, row);
				if (!rowJ) {
					continue;
				}
				for (int col = 0; col < 16; col++) {
					json_t* cellJ = json_array_get(rowJ, col);
					if (cellJ) {
						arpMatrix[row][col] = json_boolean_value(cellJ);
					}
				}
			}
		}

		json_t* chordTypesJ = json_object_get(rootJ, "chordTypes");
		if (chordTypesJ) {
			for (int i = 0; i < 16; i++) {
				json_t* typeJ = json_array_get(chordTypesJ, i);
				if (typeJ) {
					int type = (int)json_integer_value(typeJ);
					if (type < 0 || type >= NUM_CHORD_TYPES) {
						type = MAJOR_CHORD;
					}
					chordTypes[i] = type;
				}
			}
		}

		json_t* chordTypeVisibleJ = json_object_get(rootJ, "chordTypeVisible");
		if (chordTypeVisibleJ) {
			for (int i = 0; i < 16; i++) {
				json_t* visibleJ = json_array_get(chordTypeVisibleJ, i);
				if (visibleJ) {
					chordTypeVisible[i] = json_boolean_value(visibleJ);
				}
			}
		}

		json_t* chordOctavesJ = json_object_get(rootJ, "chordOctaves");
		if (chordOctavesJ) {
			for (int i = 0; i < 16; i++) {
				json_t* octaveJ = json_array_get(chordOctavesJ, i);
				if (octaveJ) {
					int octave = (int)json_integer_value(octaveJ);
					if (octave < 0) {
						octave = 0;
					}
					if (octave > 8) {
						octave = 8;
					}
					chordOctaves[i] = octave;
				}
			}
		}

		json_t* chordSliderValuesJ = json_object_get(rootJ, "chordSliderValues");
		if (chordSliderValuesJ) {
			for (int i = 0; i < 16; i++) {
				json_t* sliderJ = json_array_get(chordSliderValuesJ, i);
				if (sliderJ) {
					float value = (float)json_number_value(sliderJ);
					if (value < 0.f) {
						value = 0.f;
					}
					if (value > 1.f) {
						value = 1.f;
					}
					chordSliderValues[i] = value;
				}
			}
		}

		// A la reouverture, le sequencer repart du debut, mais les donnees
		// musicales restaurees restent intactes.
		activeChord = 0;
		activeArpStep = 0;
		lastEditedChord = 0;
		params[OCT_PARAM].setValue((float)chordOctaves[0]);
		firstClockAfterStart = true;
		previousRunActive = false;
		chordClockSeen = false;
		haveClockPeriod = false;
		timeSinceLastChordClock = 0.f;
		lastChordClockPeriod = 0.f;
		chordGateRetriggerTimer = 0.f;
		clockOutPulseTimer = 0.f;
		sceneChordGateMuteTimer = 0.f;
		sceneChordGateHoldTimer = 0.f;
		pendingSceneStartRetrigger = false;
		lastArpVoiceCount = 0;
		for (int v = 0; v < 6; v++) {
			lastArpVoltages[v] = 0.f;
		}
	}

	void onReset(const ResetEvent& e) override {
		resetChordState();
	}

	void process(const ProcessArgs& args) override {
		int chordLen = getChordLen();
		if (activeChord >= chordLen) {
			activeChord = chordLen - 1;
		}
		if (activeChord < 0) {
			activeChord = 0;
		}

		// Architecture temporelle interne.
		//
		// - RUN lance / arrete l'horloge interne.
		// - BPM definit la duree d'un accord.
		// - A chaque impulsion d'horloge, l'accord suivant devient actif.
		// - ARP_LEN divise cette duree et fait avancer la matrice d'arpege.
		// - CLK OUT recopie les impulsions de l'horloge interne.
		// Commandes externes de transport.
		// RUN_INPUT est une entree Trigger toggle : chaque front montant bascule RUN ON/OFF.
		// RESET_INPUT est une entree Trigger : front montant = retour au debut des sequenceurs.
		if (inputs[RUN_INPUT].isConnected() && runTrigger.process(inputs[RUN_INPUT].getVoltage())) {
			setRunState(!isRunning());
		}

		if (inputs[RESET_INPUT].isConnected() && resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			resetChordSequencer();
		}

		bool runActive = isRunning();
		bool runJustStarted = runActive && !previousRunActive;
		previousRunActive = runActive;

		float bpm = params[BPM_PARAM].getValue();
		if (bpm < 1.f) {
			bpm = 1.f;
		}
		if (bpm > 300.f) {
			bpm = 300.f;
		}

		float chordPeriod = 60.f / bpm;
		if (chordPeriod < 0.001f) {
			chordPeriod = 0.001f;
		}

		lastChordClockPeriod = chordPeriod;
		haveClockPeriod = true;

		if (runJustStarted) {
			// RUN lance l'horloge interne sans reinitialiser les sequenceurs,
			// sauf lorsqu'une scène vient d'être rappelée : dans ce cas,
			// la scène doit repartir proprement du début musical.
			if (pendingSceneStartRetrigger) {
				activeChord = 0;
				activeArpStep = 0;
				lastEditedChord = 0;
				params[OCT_PARAM].setValue((float)chordOctaves[0]);
				timeSinceLastChordClock = 0.f;

				// Premier accord d'une scène rappelée puis lancée :
				// vrai front de gate + tenue pendant la durée du premier accord.
				sceneChordGateMuteTimer = 0.010f;
				sceneChordGateHoldTimer = chordPeriod;

				pendingSceneStartRetrigger = false;
			}
			else {
				int arpLen = getArpLen();
				float subPeriod = chordPeriod / (float)arpLen;
				if (activeArpStep < 0) {
					activeArpStep = 0;
				}
				if (activeArpStep >= arpLen) {
					activeArpStep = arpLen - 1;
				}
				timeSinceLastChordClock = subPeriod * (float)activeArpStep;
			}

			chordClockSeen = true;
			haveClockPeriod = true;
			lastChordClockPeriod = chordPeriod;
			clockOutPulseTimer = 0.005f;
			chordGateRetriggerTimer = 0.f;
		}

		if (!runActive) {
			// RUN OFF arrete seulement l'horloge et les gates.
			// Il ne remet plus activeChord ni activeArpStep a zero.
			chordGateRetriggerTimer = 0.f;
			clockOutPulseTimer = 0.f;
			sceneChordGateMuteTimer = 0.f;
			sceneChordGateHoldTimer = 0.f;
			chordClockSeen = false;
		}
		else {
			timeSinceLastChordClock += args.sampleTime;

			if (clockOutPulseTimer > 0.f) {
				clockOutPulseTimer -= args.sampleTime;
				if (clockOutPulseTimer < 0.f) {
					clockOutPulseTimer = 0.f;
				}
			}

			if (chordGateRetriggerTimer > 0.f) {
				chordGateRetriggerTimer -= args.sampleTime;
				if (chordGateRetriggerTimer < 0.f) {
					chordGateRetriggerTimer = 0.f;
				}
			}

			if (sceneChordGateMuteTimer > 0.f) {
				sceneChordGateMuteTimer -= args.sampleTime;
				if (sceneChordGateMuteTimer < 0.f) {
					sceneChordGateMuteTimer = 0.f;
				}
			}

			if (sceneChordGateHoldTimer > 0.f) {
				sceneChordGateHoldTimer -= args.sampleTime;
				if (sceneChordGateHoldTimer < 0.f) {
					sceneChordGateHoldTimer = 0.f;
				}
			}

			while (timeSinceLastChordClock >= chordPeriod) {
				timeSinceLastChordClock -= chordPeriod;
				advanceChordOnly();
				activeArpStep = 0;
				chordClockSeen = true;
				clockOutPulseTimer = 0.005f;
				chordGateRetriggerTimer = 0.005f;
			}

			int arpLen = getArpLen();
			float subPeriod = chordPeriod / (float)arpLen;
			if (subPeriod > 0.0001f) {
				int step = (int)std::floor(timeSinceLastChordClock / subPeriod);
				if (step < 0) {
					step = 0;
				}
				if (step >= arpLen) {
					step = arpLen - 1;
				}
				activeArpStep = step;
			}
			else {
				activeArpStep = 0;
			}
		}

		for (int i = 0; i < 16; i++) {
			if (i < chordLen) {
				chordSliderValues[i] = params[CHORD_1_PARAM + i].getValue();
			}
			else {
				params[CHORD_1_PARAM + i].setValue(chordSliderValues[i]);
			}
		}

		int currentOctave = (int)std::round(params[OCT_PARAM].getValue());
		if (activeChord >= 0 && activeChord < 16) {
			chordOctaves[activeChord] = currentOctave;
			lastEditedChord = activeChord;
		}

		for (int i = 0; i < 16; i++) {
			lights[MODE_1_LIGHT + i].setBrightness(i == activeChord ? 1.f : 0.f);
		}

		// Sorties ARP avec repos propres.
		float arpVoltages[6];
		int arpVoiceCount = buildActiveArpVoltages(arpVoltages);
		bool activeColumnHasNotes = (arpVoiceCount > 0);

		if (activeColumnHasNotes) {
			lastArpVoiceCount = arpVoiceCount;
			for (int voice = 0; voice < arpVoiceCount; voice++) {
				lastArpVoltages[voice] = arpVoltages[voice];
			}
		}

		int arpOutputVoiceCount = activeColumnHasNotes ? arpVoiceCount : lastArpVoiceCount;
		outputs[ARP_VOCT_OUTPUT].setChannels(arpOutputVoiceCount);
		outputs[ARP_GATE_OUTPUT].setChannels(arpOutputVoiceCount);

		bool chordCycleActive = runActive && chordClockSeen;

		bool arpSubGateHigh = false;
		if (chordCycleActive) {
			int arpLen = getArpLen();
			float subPeriod = chordPeriod / (float)arpLen;
			if (subPeriod > 0.0001f) {
				float subPhase = timeSinceLastChordClock - std::floor(timeSinceLastChordClock / subPeriod) * subPeriod;
				arpSubGateHigh = (subPhase < subPeriod * 0.85f);
			}
		}

		float arpGateVoltage = (activeColumnHasNotes && arpSubGateHigh) ? 10.f : 0.f;

		for (int voice = 0; voice < arpOutputVoiceCount; voice++) {
			float voltage = activeColumnHasNotes ? arpVoltages[voice] : lastArpVoltages[voice];
			outputs[ARP_VOCT_OUTPUT].setVoltage(voltage, voice);
			outputs[ARP_GATE_OUTPUT].setVoltage(arpGateVoltage, voice);
		}

		for (int voice = arpOutputVoiceCount; voice < 6; voice++) {
			outputs[ARP_VOCT_OUTPUT].setVoltage(0.f, voice);
			outputs[ARP_GATE_OUTPUT].setVoltage(0.f, voice);
		}

		// CHORD_V/OCT sort 6 canaux polyphoniques :
		// Root, 3rd, 5th, Root+12, 3rd+12, 5th+12.
		outputs[CHORD_VOCT_OUTPUT].setChannels(3);
		for (int c = 0; c < 3; c++) {
			outputs[CHORD_VOCT_OUTPUT].setVoltage(getChordNoteVoltage(activeChord, c), c);
		}

		int activeRoot = getChordRoot(activeChord);
		float rootVoltage = getRootVoltage(activeRoot, chordOctaves[activeChord]);
		outputs[ROOT_VOCT_OUTPUT].setVoltage(rootVoltage);

		// CHORD_GATE suit CHORD_V/OCT en polyphonie.
		// Les canaux portent la meme gate : aucun changement de timing,
		// mais les traitements externes peuvent appairer V/OCT et GATE canal par canal.
		outputs[CHORD_GATE_OUTPUT].setChannels(3);
		float chordGateVoltage = 0.f;
		if (sceneChordGateMuteTimer > 0.f) {
			chordGateVoltage = 0.f;
		}
		else if (sceneChordGateHoldTimer > 0.f) {
			chordGateVoltage = 10.f;
		}
		else {
			chordGateVoltage = (chordCycleActive && chordGateRetriggerTimer <= 0.f) ? 10.f : 0.f;
		}
		for (int c = 0; c < 3; c++) {
			outputs[CHORD_GATE_OUTPUT].setVoltage(chordGateVoltage, c);
		}

		outputs[CLK_OUTPUT].setChannels(1);
		float clkVoltage = (runActive && clockOutPulseTimer > 0.f) ? 10.f : 0.f;
		outputs[CLK_OUTPUT].setVoltage(clkVoltage);
	}

	void selectChord(int index) {
		if (!isChordInSequence(index)) {
			return;
		}

		activeChord = index;
		lastEditedChord = index;
		params[OCT_PARAM].setValue((float)chordOctaves[index]);
	}

	void cycleChordType(int index) {
		if (!isChordInSequence(index)) {
			return;
		}

		selectChord(index);

		if (!chordTypeVisible[index]) {
			chordTypeVisible[index] = true;
			return;
		}

		switch (chordTypes[index]) {
			case MAJOR_CHORD:
				chordTypes[index] = MINOR_CHORD;
				break;
			case MINOR_CHORD:
				chordTypes[index] = SUS2_CHORD;
				break;
			case SUS2_CHORD:
				chordTypes[index] = SUS4_CHORD;
				break;
			case SUS4_CHORD:
				chordTypes[index] = DIM_CHORD;
				break;
			case DIM_CHORD:
				chordTypes[index] = AUG_CHORD;
				break;
			case AUG_CHORD:
			default:
				chordTypes[index] = MAJOR_CHORD;
				break;
		}
	}

	void clearArpMatrix() {
		for (int row = 0; row < 6; row++) {
			for (int col = 0; col < 16; col++) {
				arpMatrix[row][col] = false;
			}
		}

		// Retour au premier pas d'arpege, sans modifier ARP_LEN ni les accords.
		activeArpStep = 0;
		timeSinceLastChordClock = 0.f;
		lastArpVoiceCount = 0;
		for (int v = 0; v < 6; v++) {
			lastArpVoltages[v] = 0.f;
		}
	}

	void clearChords() {
		for (int i = 0; i < 16; i++) {
			params[CHORD_1_PARAM + i].setValue(0.f);
			params[MODE_1_PARAM + i].setValue(0.f);
			chordSliderValues[i] = 0.f;
			chordOctaves[i] = 4;
			chordTypes[i] = MAJOR_CHORD;
			chordTypeVisible[i] = false;
		}

		// Retour au premier accord, sans modifier CHORD_LEN ni la matrice ARP.
		activeChord = 0;
		lastEditedChord = 0;
		params[OCT_PARAM].setValue(4.f);
	}
};


struct KArpChordSliderActiveBackground : TransparentWidget {
	KArp* module = NULL;

	KArpChordSliderActiveBackground(KArp* module) {
		this->module = module;
		box.size = mm2px(Vec(162.56f, 128.5f));
	}

	void draw(const DrawArgs& args) override {
		if (!module) {
			return;
		}

		int chordLen = module->getChordLen();
		if (chordLen < 1) chordLen = 1;
		if (chordLen > 16) chordLen = 16;

		for (int i = 0; i < chordLen; i++) {
			float x = 20.8f + i * 8.45f;

			// Trame lumineuse derrière les sliders actifs CHORD LEN.
			// La couleur reprend celle du poussoir d'accord actif.
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, mm2px(x - 2.15f), mm2px(32.2f), mm2px(4.30f), mm2px(28.5f), mm2px(1.2f));
			nvgFillColor(args.vg, nvgRGBA(130, 255, 130, 42));
			nvgFill(args.vg);

			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, mm2px(x), mm2px(32.8f));
			nvgLineTo(args.vg, mm2px(x), mm2px(60.0f));
			nvgStrokeColor(args.vg, nvgRGBA(215, 255, 215, 155));
			nvgStrokeWidth(args.vg, 0.85f);
			nvgStroke(args.vg);
		}
	}
};

struct KArpMatrixFrame : TransparentWidget {
	KArpMatrixFrame() {
		box.size = mm2px(Vec(162.56f, 128.5f));
	}

	void draw(const DrawArgs& args) override {
		// Cadre discret de la matrice ARP, rétabli pour structurer le panneau.
		nvgBeginPath(args.vg);
		// Largeur étendue à droite : la colonne 16 reste maintenant à l'intérieur du cadre.
		nvgRoundedRect(args.vg, mm2px(16.4f), mm2px(68.0f), mm2px(136.2f), mm2px(35.8f), mm2px(2.0f));
		nvgStrokeColor(args.vg, nvgRGBA(80, 220, 255, 180));
		nvgStrokeWidth(args.vg, 1.15f);
		nvgStroke(args.vg);

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, mm2px(16.8f), mm2px(68.4f), mm2px(135.4f), mm2px(35.0f), mm2px(1.8f));
		nvgStrokeColor(args.vg, nvgRGBA(30, 70, 85, 155));
		nvgStrokeWidth(args.vg, 0.55f);
		nvgStroke(args.vg);
	}
};

struct KArpChordSlider : VCVSlider {
	KArp* module = NULL;
	int chordIndex = 0;

	void draw(const DrawArgs& args) override {
		bool inSequence = true;
		if (module) {
			inSequence = module->isChordInSequence(chordIndex);
		}

		if (!inSequence) {
			nvgGlobalAlpha(args.vg, 0.32f);
		}

		VCVSlider::draw(args);

		if (!inSequence) {
			nvgGlobalAlpha(args.vg, 1.f);
		}
		else {
			// Liseré lumineux des sliders inclus dans CHORD LEN.
			// La teinte reprend le poussoir d'accord actif.
			float cx = box.size.x * 0.5f;

			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, cx - mm2px(2.25f), mm2px(0.6f), mm2px(4.5f), box.size.y - mm2px(1.2f), mm2px(1.4f));
			nvgStrokeColor(args.vg, nvgRGBA(215, 255, 215, 230));
			nvgStrokeWidth(args.vg, 1.25f);
			nvgStroke(args.vg);
		}
	}

	bool isLocked() {
		return module && !module->isChordInSequence(chordIndex);
	}

	void onButton(const event::Button& e) override {
		if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (isLocked()) {
				auto* pq = getParamQuantity();
				if (pq) {
					pq->setValue(module->chordSliderValues[chordIndex]);
				}
				e.consume(this);
				return;
			}
			module->selectChord(chordIndex);
		}

		VCVSlider::onButton(e);
	}

	void onDragStart(const event::DragStart& e) override {
		if (isLocked()) {
			e.consume(this);
			return;
		}
		VCVSlider::onDragStart(e);
	}

	void onDragMove(const event::DragMove& e) override {
		if (isLocked()) {
			auto* pq = getParamQuantity();
			if (pq) {
				pq->setValue(module->chordSliderValues[chordIndex]);
			}
			e.consume(this);
			return;
		}
		VCVSlider::onDragMove(e);
	}

	void onDragEnd(const event::DragEnd& e) override {
		if (isLocked()) {
			e.consume(this);
			return;
		}
		VCVSlider::onDragEnd(e);
	}

	void onHoverScroll(const event::HoverScroll& e) override {
		if (isLocked()) {
			auto* pq = getParamQuantity();
			if (pq) {
				pq->setValue(module->chordSliderValues[chordIndex]);
			}
			e.consume(this);
			return;
		}
		VCVSlider::onHoverScroll(e);
	}
};

struct KArpModeButton : Widget {
	KArp* module = NULL;
	int chordIndex = 0;

	KArpModeButton() {
		// Poussoir discret, aligné sur le cercle dessiné dans le SVG.
		box.size = mm2px(Vec(5.2f, 5.2f));
	}

	void draw(const DrawArgs& args) override {
		int chordLen = 16;
		bool active = false;
		bool inSequence = true;

		if (module) {
			chordLen = module->getChordLen();
			active = (module->activeChord == chordIndex);
			inSequence = (chordIndex < chordLen);
		}

		NVGcolor fillColor;
		NVGcolor strokeColor;

		if (active) {
			// Accord actif / édité : vert clair.
			fillColor = nvgRGB(145, 255, 145);
			strokeColor = nvgRGB(215, 255, 215);
		}
		else if (inSequence) {
			// Accord dans la séquence : vert discret.
			fillColor = nvgRGB(65, 135, 75);
			strokeColor = nvgRGB(105, 180, 110);
		}
		else {
			// Accord hors séquence : gris sombre.
			fillColor = nvgRGB(35, 42, 38);
			strokeColor = nvgRGB(75, 85, 78);
		}

		float cx = box.size.x * 0.5f;
		float cy = box.size.y * 0.5f;
		


		// Petit halo seulement pour l'accord actif.
		if (active) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, mm2px(2.35f));
			nvgFillColor(args.vg, nvgRGBA(130, 255, 130, 55));
			nvgFill(args.vg);
		}

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, mm2px(1.65f));
		nvgFillColor(args.vg, fillColor);
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, mm2px(1.95f));
		nvgStrokeColor(args.vg, strokeColor);
		nvgStrokeWidth(args.vg, active ? 1.25f : 0.85f);
		nvgStroke(args.vg);
	}

	void onButton(const event::Button& e) override {
		if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			int chordLen = module->getChordLen();

			// Les poussoirs hors sequence sont visibles mais inactifs.
			if (chordIndex >= chordLen) {
				e.consume(this);
				return;
			}

			// Nouveau comportement :
			// - clic sur un poussoir non actif = selection de l'accord seulement ;
			// - clic sur le poussoir deja actif = changement du type d'accord.
			if (module->activeChord == chordIndex) {
				module->cycleChordType(chordIndex);
			}
			else {
				module->selectChord(chordIndex);
			}

			e.consume(this);
			return;
		}

		Widget::onButton(e);
	}
};




struct KArpRunButton : Widget {
	KArp* module = NULL;

	KArpRunButton() {
		// Bouton RUN volontairement plus grand que les poussoirs secondaires :
		// c'est une commande globale de transport, distincte des codes couleur ARP/CHORD.
		box.size = mm2px(Vec(10.2f, 10.2f));
	}

	bool isOn() {
		return module && module->params[KArp::RUN_PARAM].getValue() >= 0.5f;
	}

	void draw(const DrawArgs& args) override {
		float cx = box.size.x * 0.5f;
		float cy = box.size.y * 0.5f;
		bool on = isOn();

		// Bouton RUN ON/OFF : commande globale plus visible.
		// OFF = gris clair lisible ; ON = blanc lumineux avec halo renforcé.
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, mm2px(on ? 4.25f : 3.80f));
		nvgFillColor(args.vg, on ? nvgRGBA(255, 255, 255, 78) : nvgRGBA(220, 220, 210, 24));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, mm2px(3.25f));
		nvgFillColor(args.vg, on ? nvgRGB(84, 84, 76) : nvgRGB(72, 72, 68));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, mm2px(2.12f));
		nvgFillColor(args.vg, on ? nvgRGB(255, 255, 245) : nvgRGB(205, 205, 195));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, mm2px(2.88f));
		nvgStrokeColor(args.vg, on ? nvgRGB(255, 255, 255) : nvgRGB(230, 230, 220));
		nvgStrokeWidth(args.vg, on ? 1.45f : 1.10f);
		nvgStroke(args.vg);
	}

	void onButton(const event::Button& e) override {
		if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			module->setRunState(!isOn());
			e.consume(this);
			return;
		}

		Widget::onButton(e);
	}
};

struct KArpResetSeqButton : Widget {
	KArp* module = NULL;

	KArpResetSeqButton() {
		box.size = mm2px(Vec(7.2f, 7.2f));
	}

	void draw(const DrawArgs& args) override {
		float cx = box.size.x * 0.5f;
		float cy = box.size.y * 0.5f;

		// Poussoir manuel RST SEQ : commande de transport sobre, blanc/gris clair.
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, mm2px(2.45f));
		nvgFillColor(args.vg, nvgRGB(34, 34, 34));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, mm2px(1.65f));
		nvgFillColor(args.vg, nvgRGB(228, 228, 220));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, mm2px(2.18f));
		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		nvgStrokeWidth(args.vg, 1.05f);
		nvgStroke(args.vg);
	}

	void onButton(const event::Button& e) override {
		if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			module->resetChordSequencer();
			e.consume(this);
			return;
		}

		Widget::onButton(e);
	}
};

struct KArpMatrixButton : Widget {
	KArp* module = NULL;
	int row = 0;   // 0 = R, 1 = 3, 2 = 5, 3 = ↑R, 4 = ↑3, 5 = ↑5
	int column = 0;

	KArpMatrixButton() {
		box.size = mm2px(Vec(4.4f, 4.4f));
	}

	void draw(const DrawArgs& args) override {
		bool active = false;
		bool inSequence = true;

		if (module) {
			if (row >= 0 && row < 6 && column >= 0 && column < 16) {
				active = module->arpMatrix[row][column];
			}
			inSequence = (column < module->getArpLen());
		}

		float cx = box.size.x * 0.5f;
		float cy = box.size.y * 0.5f;
		
bool currentStep = false;
if (module) {
        currentStep = (column == module->activeArpStep);
}

if (currentStep && inSequence) {
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, cx, cy, mm2px(2.28f));
        nvgStrokeColor(args.vg, nvgRGB(120, 235, 255));
        nvgStrokeWidth(args.vg, 1.25f);
        nvgStroke(args.vg);
}		

		if (!inSequence) {
			// Colonne hors ARP LEN : repère très discret, sans cyan.
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, mm2px(1.55f));
			nvgStrokeColor(args.vg, nvgRGB(42, 45, 47));
			nvgStrokeWidth(args.vg, 0.70f);
			nvgStroke(args.vg);
			return;
		}

		if (active) {
			// Case active : cyan lumineux, dessinée par un seul bouton propre.
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, mm2px(2.05f));
			nvgFillColor(args.vg, nvgRGBA(120, 235, 255, 45));
			nvgFill(args.vg);

			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, mm2px(1.35f));
			nvgFillColor(args.vg, nvgRGB(120, 235, 255));
			nvgFill(args.vg);

			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, mm2px(1.65f));
			nvgStrokeColor(args.vg, nvgRGB(215, 252, 255));
			nvgStrokeWidth(args.vg, 1.00f);
			nvgStroke(args.vg);
		}
		else {
			// Case inactive : un seul anneau gris, sans remplissage cyan.
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, mm2px(1.60f));
			nvgStrokeColor(args.vg, nvgRGB(135, 137, 140));
			nvgStrokeWidth(args.vg, 0.85f);
			nvgStroke(args.vg);
		}
	}

	void onButton(const event::Button& e) override {
		if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			// Edition de la matrice d'arpege.
			// Les colonnes hors ARP LEN restent visibles mais non editables.
			if (row >= 0 && row < 6 && column >= 0 && column < 16 && column < module->getArpLen()) {
				module->arpMatrix[row][column] = !module->arpMatrix[row][column];
			}

			e.consume(this);
			return;
		}

		Widget::onButton(e);
	}
};

struct KArpMatrixLabels : TransparentWidget {
	std::shared_ptr<Font> font;

KArp* module = NULL;

KArpMatrixLabels(KArp* module) {
        this->module = module;
        font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
}

	void draw(const DrawArgs& args) override {
		if (!font) {
			return;
		}

		static const char* ROW_LABELS[6] = {"R", "3", "5", "↑R", "↑3", "↑5"};
		static const float ROW_Y[6] = {101.0f, 95.0f, 89.0f, 83.0f, 77.0f, 71.0f};

		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 10.2f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, nvgRGB(120, 220, 255));

		for (int row = 0; row < 6; row++) {
			float x = mm2px(9.6f);
			float y = mm2px(ROW_Y[row]);
			nvgText(args.vg, x, y, ROW_LABELS[row], NULL);
			nvgText(args.vg, x + 0.35f, y, ROW_LABELS[row], NULL);
		}
		int chordLen = 16;
if (module) {
        chordLen = module->getChordLen();
}

nvgFontSize(args.vg, 8.2f);

for (int i = 0; i < 16; i++) {
        char stepText[4];
        snprintf(stepText, sizeof(stepText), "%d", i + 1);

        if (i < chordLen) {
                nvgFillColor(args.vg, nvgRGB(255, 255, 255));
        }
        else {
                nvgFillColor(args.vg, nvgRGB(82, 86, 90));
        }

        float x = mm2px(20.8f + i * 8.45f);
        float y = mm2px(65.3f);
        nvgText(args.vg, x, y, stepText, NULL);
        nvgText(args.vg, x + 0.45f, y, stepText, NULL);
}
	}
};

static void drawBoldText(NVGcontext* vg, float x, float y, const char* text) {
	nvgText(vg, x, y, text, NULL);
	nvgText(vg, x + 0.45f, y, text, NULL);
}

struct KArpPortLabels : TransparentWidget {
	std::shared_ptr<Font> font;

	KArpPortLabels() {
		font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
	}

	void draw(const DrawArgs& args) override {
		if (!font) {
			return;
		}

		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, nvgRGB(220, 220, 220));

		// Chapitre 5G-16 : separateurs verticaux de la zone de sorties.
		// Les axes sont calcules comme milieux entre les jacks voisins :
		// ARP GATE x=104 / CHORD V/OCT x=119 -> x=111.5
		// CHORD GATE x=132 / ROOT V/OCT x=148 -> x=140
		// Le cache est volontairement limite sous le cadre inferieur de la matrice
		// pour effacer les anciens traits du SVG sans mordre sur ce cadre.
		// Extension basse legerement augmentee pour supprimer les deux points residuels.
		static const float SEPARATOR_X[2] = {111.5f, 140.0f};
		for (int i = 0; i < 2; i++) {
			float x = SEPARATOR_X[i];

			nvgBeginPath(args.vg);
			nvgRect(args.vg, mm2px(x - 3.40f), mm2px(105.75f), mm2px(6.80f), mm2px(20.80f));
			nvgFillColor(args.vg, nvgRGB(24, 25, 25));
			nvgFill(args.vg);

			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, mm2px(x), mm2px(106.8f));
			nvgLineTo(args.vg, mm2px(x), mm2px(123.2f));
			nvgStrokeColor(args.vg, nvgRGB(75, 135, 95));
			nvgStrokeWidth(args.vg, 1.25f);
			nvgStroke(args.vg);
		}

		// Commande manuelle RST SEQ sur la bande inferieure.
		// Les jacks CLOCK et RST sont retires du panneau, mais la logique interne
		// n'est pas encore refondue a cette etape.
		nvgFillColor(args.vg, nvgRGB(220, 220, 220));
		nvgFontSize(args.vg, 10.0f);
		drawBoldText(args.vg, mm2px(34.0), mm2px(110.4), "RUN");
		drawBoldText(args.vg, mm2px(55.5), mm2px(110.4), "RST SEQ");
		drawBoldText(args.vg, mm2px(76.0), mm2px(110.4), "OUT CLK");

		// Sorties : nom du groupe au-dessus, type de sortie en dessous
		nvgFontSize(args.vg, 8.8f);
		// Sortie ARP : cyan
		nvgFillColor(args.vg, nvgRGB(120, 220, 255));
		drawBoldText(args.vg, mm2px(97.5), mm2px(106.4), "ARP");

		// Sorties harmoniques : vert
		nvgFillColor(args.vg, nvgRGB(150, 255, 150));
		drawBoldText(args.vg, mm2px(125.5), mm2px(106.4), "CHORD");
		drawBoldText(args.vg, mm2px(148.0), mm2px(106.4), "ROOT");

		nvgFontSize(args.vg, 10.0f);
		nvgFillColor(args.vg, nvgRGB(220, 220, 220));
		drawBoldText(args.vg, mm2px(91.0), mm2px(110.4), "V/OCT");
		drawBoldText(args.vg, mm2px(104.0), mm2px(110.4), "GATE");
		drawBoldText(args.vg, mm2px(119.0), mm2px(110.4), "V/OCT");
		drawBoldText(args.vg, mm2px(132.0), mm2px(110.4), "GATE");
		drawBoldText(args.vg, mm2px(148.0), mm2px(110.4), "V/OCT");
	}
};

struct KArpTopLabels : TransparentWidget {
	std::shared_ptr<Font> font;

	KArpTopLabels() {
		font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
	}

	void draw(const DrawArgs& args) override {
		if (!font) {
			return;
		}

		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 10.0f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		// Commande RUN en dessous du potentiometre de tempo.
		nvgFillColor(args.vg, nvgRGB(235, 235, 235));
		drawBoldText(args.vg, mm2px(11.2), mm2px(52.5), "RUN");

		// Libelles superieurs : blanc sobre. Les afficheurs portent deja l'identite couleur.
		nvgFillColor(args.vg, nvgRGB(235, 235, 235));
		drawBoldText(args.vg, mm2px(13.5), mm2px(14.2), "BPM");
		drawBoldText(args.vg, mm2px(39.5), mm2px(14.2), "ARP LEN");
		drawBoldText(args.vg, mm2px(84.0), mm2px(14.2), "CHORDS LEN");
		drawBoldText(args.vg, mm2px(116.0), mm2px(14.2), "ACTIVE CHORD");
		drawBoldText(args.vg, mm2px(145.5), mm2px(14.2), "OCT");
	}
};

struct KArpLengthDisplays : TransparentWidget {
	KArp* module = NULL;
	std::shared_ptr<Font> font;

	KArpLengthDisplays(KArp* module) {
		this->module = module;
		font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
	}

	void drawDisplay(const DrawArgs& args, float x, float y, float w, float h, const char* value, float fontSize, NVGcolor textColor = nvgRGB(150, 255, 150), NVGcolor strokeColor = nvgRGB(60, 100, 75)) {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, mm2px(x), mm2px(y), mm2px(w), mm2px(h), mm2px(1.5));
		nvgFillColor(args.vg, nvgRGB(4, 12, 8));
		nvgFill(args.vg);

		nvgStrokeColor(args.vg, strokeColor);
		nvgStrokeWidth(args.vg, 1.2f);
		nvgStroke(args.vg);

		if (!font) {
			return;
		}

		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fontSize);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, textColor);

		float tx = mm2px(x + w / 2.f);
		float ty = mm2px(y + h / 2.f + 0.15f);

		nvgText(args.vg, tx, ty, value, NULL);
		nvgText(args.vg, tx + 0.45f, ty, value, NULL);
	}

	void draw(const DrawArgs& args) override {
		static const char* NOTE_NAMES[12] = {
			"C", "C#", "D", "D#", "E", "F",
			"F#", "G", "G#", "A", "A#", "B"
		};

		int chordLen = 16;
		int arpLen = 1;
		int bpm = 120;
		int activeChord = 0;
		int activeRoot = 0;
		int activeOctave = 4;
		int activeChordType = KArp::MAJOR_CHORD;
		bool activeChordTypeVisible = false;

		if (module) {
			chordLen = (int)std::round(module->params[KArp::CHORD_LEN_PARAM].getValue());
			arpLen = (int)std::round(module->params[KArp::ARP_LEN_PARAM].getValue());
			bpm = (int)std::round(module->params[KArp::BPM_PARAM].getValue());
			activeChord = module->activeChord;
			if (activeChord < 0) {
				activeChord = 0;
			}
			if (activeChord > 15) {
				activeChord = 15;
			}
			activeRoot = module->getChordRoot(activeChord);
			activeOctave = module->chordOctaves[activeChord];
			activeChordType = module->chordTypes[activeChord];
			activeChordTypeVisible = module->chordTypeVisible[activeChord];
		}

		char bpmText[8];
		char chordText[8];
		char arpText[8];
		char activeChordText[32];

		snprintf(bpmText, sizeof(bpmText), "%d", bpm);
		snprintf(chordText, sizeof(chordText), "%d", chordLen);
		snprintf(arpText, sizeof(arpText), "%d", arpLen);

		const char* noteName = NOTE_NAMES[activeRoot];

		if (activeChordTypeVisible) {
			const char* chordQuality = KArp::getChordTypeName(activeChordType);
			snprintf(activeChordText, sizeof(activeChordText), "%d - %s%d %s", activeChord + 1, noteName, activeOctave, chordQuality);
		}
		else {
			snprintf(activeChordText, sizeof(activeChordText), "%d - %s%d", activeChord + 1, noteName, activeOctave);
		}

		drawDisplay(args, 7.0f, 18.0f, 13.0f, 8.0f, bpmText, 14.0f, nvgRGB(150, 255, 150), nvgRGB(60, 100, 75));
		drawDisplay(args, 33.0f, 18.0f, 13.0f, 8.0f, arpText, 14.0f, nvgRGB(120, 220, 255), nvgRGB(45, 95, 115));
		drawDisplay(args, 77.5f, 18.0f, 13.0f, 8.0f, chordText, 14.0f);
		drawDisplay(args, 99.5f, 18.0f, 33.0f, 8.0f, activeChordText, 14.0f);
	}
};




struct KArpLogoWidget : TransparentWidget {
	KArpLogoWidget() {
		// Zone reservee au logo Kalliste, deplace en bas a gauche.
		box.size = mm2px(Vec(162.56f, 128.5f));
	}

	void draw(const DrawArgs& args) override {
		// Logo Kalliste dessine directement en NanoVG a partir de LOGO_ajuste.svg.
		// Cela evite les problemes de viewBox/chargement SVG rencontres dans Rack.
		const float minX = 1054.6789f;
		const float minY = 545.1527f;
		const float logoW = 687.2638f;
		const float logoH = 898.1945f;

		// Positionnement panneau : bas gauche.
		// Taille harmonisée avec le logo de K-SCENE.
		const float xMm = 6.0f;
		const float yMm = 106.8f;
		const float wMm = 8.5f;
		const float hMm = wMm * (logoH / logoW);

		nvgSave(args.vg);
		nvgTranslate(args.vg, mm2px(xMm), mm2px(yMm));
		nvgScale(args.vg, mm2px(wMm) / logoW, mm2px(hMm) / logoH);
		nvgTranslate(args.vg, -minX, -minY);

		// Fond gris du cartouche, legerement incline comme le dessin original.
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 1054.6789f, 545.1527f);
		nvgLineTo(args.vg, 1741.9427f, 545.1527f);
		nvgLineTo(args.vg, 1741.9427f, 1324.3003f);
		nvgLineTo(args.vg, 1054.6772f, 1443.3472f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgRGB(192, 192, 192));
		nvgFill(args.vg);

		// Corse stylisee bleu profond.
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 1423.8946f, 938.5552f);
		nvgLineTo(args.vg, 1624.7319f, 771.1897f);
		nvgLineTo(args.vg, 1642.0683f, 632.9923f);
		nvgLineTo(args.vg, 1677.7550f, 631.1811f);
		nvgLineTo(args.vg, 1679.2130f, 965.6200f);
		nvgLineTo(args.vg, 1651.2643f, 1256.4514f);
		nvgLineTo(args.vg, 1600.5430f, 1261.0794f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgRGB(0, 0, 127));
		nvgFill(args.vg);

		// Lettre K noire.
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 1161.2258f, 644.1335f);
		nvgLineTo(args.vg, 1301.3632f, 632.2507f);
		nvgLineTo(args.vg, 1301.3632f, 806.0470f);
		nvgLineTo(args.vg, 1511.8110f, 631.1811f);
		nvgLineTo(args.vg, 1625.1969f, 631.1811f);
		nvgLineTo(args.vg, 1611.8287f, 755.7060f);
		nvgLineTo(args.vg, 1398.4252f, 933.5433f);
		nvgLineTo(args.vg, 1577.0570f, 1265.2910f);
		nvgLineTo(args.vg, 1402.7225f, 1296.5537f);
		nvgLineTo(args.vg, 1303.0602f, 1117.1645f);
		nvgLineTo(args.vg, 1304.9121f, 1314.0937f);
		nvgLineTo(args.vg, 1132.4485f, 1344.6814f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgFill(args.vg);

		// Decoupes / filet gris issus du SVG original.
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 1301.3632f, 632.2507f);
		nvgLineTo(args.vg, 1301.3632f, 806.0470f);
		nvgLineTo(args.vg, 1511.8110f, 631.1811f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgRGB(192, 192, 192));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 1625.1969f, 631.1811f);
		nvgLineTo(args.vg, 1642.0683f, 632.9923f);
		nvgLineTo(args.vg, 1624.7319f, 771.1897f);
		nvgLineTo(args.vg, 1423.8946f, 938.5552f);
		nvgLineTo(args.vg, 1600.5430f, 1261.0794f);
		nvgLineTo(args.vg, 1577.0570f, 1265.2910f);
		nvgLineTo(args.vg, 1396.9333f, 933.4706f);
		nvgLineTo(args.vg, 1607.5518f, 757.1093f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgRGB(192, 192, 192));
		nvgFill(args.vg);

		// Petite decoupe grise en bas du K, conservee pour rester proche du DXF.
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 1303.0602f, 1117.1645f);
		nvgLineTo(args.vg, 1304.9121f, 1314.0937f);
		nvgLineTo(args.vg, 1318.0851f, 1344.3270f);
		nvgLineTo(args.vg, 1409.3598f, 1334.5125f);
		nvgLineTo(args.vg, 1402.7225f, 1296.5537f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgRGB(192, 192, 192));
		nvgFill(args.vg);

		// Bord noir du cartouche.
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 1054.6789f, 545.1527f);
		nvgLineTo(args.vg, 1741.9427f, 545.1527f);
		nvgLineTo(args.vg, 1741.9427f, 1324.3003f);
		nvgLineTo(args.vg, 1054.6772f, 1443.3472f);
		nvgClosePath(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(0, 0, 0));
		nvgStrokeWidth(args.vg, 18.0f);
		nvgStroke(args.vg);

		nvgRestore(args.vg);
	}
};

struct KArpPanelScrews : TransparentWidget {
	KArpPanelScrews() {
		// Panneau 32 HP : largeur approximative 162.56 mm, hauteur Rack 128.5 mm.
		box.size = mm2px(Vec(162.56f, 128.5f));
	}

	void drawScrew(const DrawArgs& args, float xMm, float yMm) {
		float x = mm2px(xMm);
		float y = mm2px(yMm);

		// Ombre très discrète pour détacher la vis du fond sombre.
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, x + mm2px(0.18f), y + mm2px(0.22f), mm2px(2.30f));
		nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 95));
		nvgFill(args.vg);

		// Corps acier foncé, visible mais non clinquant.
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, x, y, mm2px(2.18f));
		nvgFillColor(args.vg, nvgRGB(58, 60, 60));
		nvgFill(args.vg);

		// Léger liseré clair.
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, x, y, mm2px(2.18f));
		nvgStrokeColor(args.vg, nvgRGB(118, 122, 122));
		nvgStrokeWidth(args.vg, 0.75f);
		nvgStroke(args.vg);

		// Petit reflet interne, très sobre.
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, x - mm2px(0.45f), y - mm2px(0.45f), mm2px(0.55f));
		nvgFillColor(args.vg, nvgRGBA(170, 175, 175, 70));
		nvgFill(args.vg);

		// Empreinte cruciforme discrète.
		nvgStrokeColor(args.vg, nvgRGB(28, 30, 30));
		nvgStrokeWidth(args.vg, 0.85f);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, x - mm2px(1.15f), y);
		nvgLineTo(args.vg, x + mm2px(1.15f), y);
		nvgMoveTo(args.vg, x, y - mm2px(1.15f));
		nvgLineTo(args.vg, x, y + mm2px(1.15f));
		nvgStroke(args.vg);
	}

	void draw(const DrawArgs& args) override {
		// Vis de façade : finition visuelle uniquement.
		// Positionnement classique aux quatre coins, avec un léger retrait.
		drawScrew(args, 5.2f, 5.2f);
		drawScrew(args, 157.3f, 5.2f);
		drawScrew(args, 5.2f, 123.3f);
		drawScrew(args, 157.3f, 123.3f);
	}
};

struct KArpTitleLabel : TransparentWidget {
	std::shared_ptr<Font> font;

	KArpTitleLabel() {
		font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans-Bold.ttf"));
		if (!font) {
			font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
		}
	}

	void draw(const DrawArgs& args) override {
		if (!font) {
			return;
		}

		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 18.0f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, nvgRGB(235, 235, 235));

		// Nom du module : neutre, sobre, centré dans le bandeau supérieur.
		// Léger double tracé pour renforcer la présence sans effet décoratif excessif.
		float x = mm2px(80.0f);
		float y = mm2px(7.8f);
		nvgText(args.vg, x, y, "K-ARP", NULL);
		nvgText(args.vg, x + 0.55f, y, "K-ARP", NULL);
	}
};

struct KArpWidget : ModuleWidget {
	KArpWidget(KArp* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/KArp.svg")));

		addChild(new KArpChordSliderActiveBackground(module));
		addChild(new KArpMatrixFrame());

		addChild(new KArpPanelScrews());
		addChild(new KArpLogoWidget());
		addChild(new KArpLengthDisplays(module));
		addChild(new KArpTopLabels());
		addChild(new KArpTitleLabel());
		addChild(new KArpPortLabels());
	    addChild(new KArpMatrixLabels(module));

		addParam(createParamCentered<RoundBlackKnob>(
			mm2px(Vec(11.5, 37.0)),
			module,
			KArp::BPM_PARAM
		));

		addParam(createParamCentered<RoundBlackKnob>(
			mm2px(Vec(53.0, 21.0)),
			module,
			KArp::ARP_LEN_PARAM
		));

		addParam(createParamCentered<RoundBlackKnob>(
			mm2px(Vec(70.5, 21.0)),
			module,
			KArp::CHORD_LEN_PARAM
		));

		addParam(createParamCentered<RoundBlackKnob>(
			mm2px(Vec(145.5, 21.0)),
			module,
			KArp::OCT_PARAM
		));

		for (int i = 0; i < 16; i++) {
			float sliderX = 20.8f + i * 8.45f;
			float modeX = 20.8f + i * 8.45f;

			KArpChordSlider* slider = createParamCentered<KArpChordSlider>(
				mm2px(Vec(sliderX, 42.5f)),
				module,
				KArp::CHORD_1_PARAM + i
			);

			slider->module = module;
			slider->chordIndex = i;

			addParam(slider);

			KArpModeButton* modeButton = createWidgetCentered<KArpModeButton>(
				mm2px(Vec(modeX, 59.5f))
			);

			modeButton->module = module;
			modeButton->chordIndex = i;

			addChild(modeButton);
		}

		// Chapitre 5E-1 : matrice d'arpege 6 x 16, visuelle uniquement.
		// Coordonnees a ajuster apres capture si necessaire.
		static const float MATRIX_ROW_Y[6] = {101.0f, 95.0f, 89.0f, 83.0f, 77.0f, 71.0f};
		for (int col = 0; col < 16; col++) {
			float matrixX = 20.8f + col * 8.45f;
			for (int row = 0; row < 6; row++) {
				KArpMatrixButton* matrixButton = createWidgetCentered<KArpMatrixButton>(
					mm2px(Vec(matrixX, MATRIX_ROW_Y[row]))
				);

				matrixButton->module = module;
				matrixButton->row = row;
				matrixButton->column = col;

				addChild(matrixButton);
			}
		}


		KArpRunButton* runButton = createWidgetCentered<KArpRunButton>(
			mm2px(Vec(11.2, 59.5))
		);

		runButton->module = module;
		addChild(runButton);

		KArpResetSeqButton* resetSeqButton = createWidgetCentered<KArpResetSeqButton>(
			mm2px(Vec(62.0, 117.00))
		);

		resetSeqButton->module = module;
		addChild(resetSeqButton);


		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(34.0, 117.00)),
			module,
			KArp::RUN_INPUT
		));

		addInput(createInputCentered<PJ301MPort>(
			mm2px(Vec(49.0, 117.00)),
			module,
			KArp::RESET_INPUT
		));

		addOutput(createOutputCentered<PJ301MPort>(
			mm2px(Vec(76.0, 117.00)),
			module,
			KArp::CLK_OUTPUT
		));

		addOutput(createOutputCentered<PJ301MPort>(
			mm2px(Vec(91.0, 117.00)),
			module,
			KArp::ARP_VOCT_OUTPUT
		));

		addOutput(createOutputCentered<PJ301MPort>(
			mm2px(Vec(104.0, 117.00)),
			module,
			KArp::ARP_GATE_OUTPUT
		));

		addOutput(createOutputCentered<PJ301MPort>(
			mm2px(Vec(119.0, 117.00)),
			module,
			KArp::CHORD_VOCT_OUTPUT
		));

		addOutput(createOutputCentered<PJ301MPort>(
			mm2px(Vec(132.0, 117.00)),
			module,
			KArp::CHORD_GATE_OUTPUT
		));

		addOutput(createOutputCentered<PJ301MPort>(
			mm2px(Vec(148.0, 117.00)),
			module,
			KArp::ROOT_VOCT_OUTPUT
		));
	}

	void appendContextMenu(Menu* menu) override {
		ModuleWidget::appendContextMenu(menu);

		KArp* karp = dynamic_cast<KArp*>(module);
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Clear ARP Matrix", "", [=]() {
			if (karp) {
				karp->clearArpMatrix();
			}
		}));
		menu->addChild(createMenuItem("Clear Chords", "", [=]() {
			if (karp) {
				karp->clearChords();
			}
		}));
	}
};

Model* modelKArp = createModel<KArp, KArpWidget>("KArp");
