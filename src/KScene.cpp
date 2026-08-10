#include "plugin.hpp"
#include <chrono>

struct KScene : Module {
	enum ParamIds {
		STORE_PLAY_PARAM,
		AUTO_MANUAL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		EOC_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	enum OperationState {
		OP_NONE,
		OP_WRITE_ARMED,
		OP_DELETE_ARMED
	};

	bool slotOccupied[16] = {
		false, false, false, false,
		false, false, false, false,
		false, false, false, false,
		false, false, false, false
	};

	KArpScene scenes[16];
	bool previousKArpRunning = false;
	bool previousPlayMode = true;
	int pendingSlot = -1;
	int lastSeenSceneCycleCounter = 0;
	bool autoLoop = true;
	bool eocOnRunReset = true;

	// -1 = aucun slot actif : état transparent de K-SCENE.
	int activeSlot = -1;
	int armedSlot = -1;
	OperationState operationState = OP_NONE;
	float blinkPhase = 0.f;
	float eocPulseTimer = 0.f;

	KScene() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configSwitch(STORE_PLAY_PARAM, 0.f, 1.f, 1.f, "Mode", {"Store", "Play"});
		configSwitch(AUTO_MANUAL_PARAM, 0.f, 1.f, 0.f, "Play mode", {"Auto", "Manual"});
		configOutput(EOC_OUTPUT, "End of cycle");
	}

	void fireEOCPulse() {
		eocPulseTimer = 0.005f;
	}

	KArpSceneHost* getKArpHost() {
		if (!leftExpander.module) {
			return NULL;
		}
		return dynamic_cast<KArpSceneHost*>(leftExpander.module);
	}

	bool isKArpRunning() {
		KArpSceneHost* host = getKArpHost();
		if (!host) {
			return false;
		}
		return host->isKArpRunningForScene();
	}

	int getKArpSceneCycleCounter() {
		KArpSceneHost* host = getKArpHost();
		if (!host) {
			return 0;
		}
		return host->getSceneCycleCounter();
	}

	int getNextAutoSlot() {
		int count = occupiedCount();
		if (count <= 0) {
			return -1;
		}

		if (activeSlot < 0 || activeSlot >= count) {
			return 0;
		}

		int next = activeSlot + 1;
		if (next < count) {
			return next;
		}

		if (autoLoop) {
			return 0;
		}

		return activeSlot;
	}

	void applyActiveSceneToKArpForced() {
		if (activeSlot < 0 || activeSlot >= 16) {
			return;
		}
		if (!slotOccupied[activeSlot]) {
			return;
		}
		if (!scenes[activeSlot].valid) {
			return;
		}

		KArpSceneHost* host = getKArpHost();
		if (!host) {
			return;
		}

		host->applyScene(scenes[activeSlot]);
	}

	void applyActiveSceneToKArp() {
		// Sécurité absolue :
		// en STORE, K-SCENE ne doit jamais rappeler ni appliquer de scène automatiquement.
		// Les rappels explicites de maintenance STORE utilisent applyActiveSceneToKArpForced().
		if (!isPlayMode()) {
			return;
		}

		applyActiveSceneToKArpForced();
	}

	void process(const ProcessArgs& args) override {
		if (eocPulseTimer > 0.f) {
			eocPulseTimer -= args.sampleTime;
			if (eocPulseTimer < 0.f) {
				eocPulseTimer = 0.f;
			}
		}

		bool karpRunning = isKArpRunning();

		bool playMode = isPlayMode();

		// AUTO/MANUAL n'a de sens qu'en mode PLAY.
		// En mode STORE, le switch inférieur est forcé sur MANUAL.
		// Au retour de STORE vers PLAY, K-SCENE repasse automatiquement en AUTO,
		// qui est le mode de lecture principal. L'utilisateur peut ensuite
		// sélectionner MANUAL explicitement s'il le souhaite.
		if (!playMode) {
			params[AUTO_MANUAL_PARAM].setValue(1.f);
		}
		else {
			if (!previousPlayMode) {
				params[AUTO_MANUAL_PARAM].setValue(0.f);
			}

			// Revenir en PLAY annule toute opération STORE en attente.
			cancelOperation();
		}

		// Au démarrage de K-ARP, K-SCENE n'applique une scène que s'il est en PLAY.
		// En STORE, K-SCENE reste totalement passif : l'utilisateur peut éditer
		// et écouter K-ARP librement avant de mémoriser.
		if (isPlayMode() && karpRunning && !previousKArpRunning) {
			if (!isManualMode() && occupiedCount() > 0) {
				activeSlot = 0;
				pendingSlot = -1;
			}

			applyActiveSceneToKArp();
			lastSeenSceneCycleCounter = getKArpSceneCycleCounter();

			// Option contextuelle : EOC on RUN/RST.
			// Quand elle est activee, EOC sert aussi de pulse de synchronisation
			// de debut de cycle au passage strict de K-ARP de STOP a RUN.
			// Quand elle est desactivee, EOC reste strictement une impulsion
			// de fin de scene, utile notamment pour piloter un sequenceur externe
			// sans le faire avancer d'un pas au demarrage.
			if (eocOnRunReset && occupiedCount() > 0) {
				fireEOCPulse();
			}
		}

		// PLAY :
		// - MANUAL : un clic prépare pendingSlot ; le changement se fait en fin de scène.
		// - AUTO   : à chaque fin de scène, K-SCENE avance au slot occupé suivant.
		if (isPlayMode() && karpRunning) {
			int currentCounter = getKArpSceneCycleCounter();
			if (currentCounter != lastSeenSceneCycleCounter) {
				lastSeenSceneCycleCounter = currentCounter;

				// EOC = End Of Current Scene.
				// Impulsion envoyée à chaque fin de scène, avant changement
				// ou répétition de scène, en PLAY/MANUAL comme en PLAY/AUTO.
				if (occupiedCount() > 0) {
					fireEOCPulse();
				}

				if (isManualMode()) {
					if (pendingSlot >= 0 && pendingSlot < 16 && slotOccupied[pendingSlot]) {
						activeSlot = pendingSlot;
						pendingSlot = -1;
						applyActiveSceneToKArp();
						lastSeenSceneCycleCounter = getKArpSceneCycleCounter();
					}
				}
				else {
					pendingSlot = -1;

					int nextSlot = getNextAutoSlot();
					if (nextSlot >= 0 && nextSlot != activeSlot) {
						activeSlot = nextSlot;
						applyActiveSceneToKArp();
						lastSeenSceneCycleCounter = getKArpSceneCycleCounter();
					}
				}
			}
		}
		else {
			lastSeenSceneCycleCounter = getKArpSceneCycleCounter();
		}

		previousKArpRunning = karpRunning;
		previousPlayMode = playMode;

		blinkPhase += args.sampleTime * 2.0f;
		if (blinkPhase >= 1.f) {
			blinkPhase -= 1.f;
		}

		clampActiveSlot();

		outputs[EOC_OUTPUT].setVoltage(eocPulseTimer > 0.f ? 10.f : 0.f);
	}

	bool isPlayMode() {
		return params[STORE_PLAY_PARAM].getValue() >= 0.5f;
	}

	bool isStoreMode() {
		return !isPlayMode();
	}

	bool isManualMode() {
		return params[AUTO_MANUAL_PARAM].getValue() >= 0.5f;
	}

	bool isAutoMode() {
		return !isManualMode();
	}

	bool isSlotOccupied(int index) const {
		if (index < 0 || index >= 16) return false;
		return slotOccupied[index];
	}

	int occupiedCount() const {
		int count = 0;
		for (int i = 0; i < 16; i++) {
			if (slotOccupied[i]) count++;
			else break;
		}
		return count;
	}

	int firstOccupiedSlot() const {
		for (int i = 0; i < 16; i++) {
			if (slotOccupied[i]) return i;
		}
		return 0;
	}

	void clampActiveSlot() {
		int count = occupiedCount();
		if (count <= 0) {
			activeSlot = -1;
			return;
		}
		if (activeSlot < 0) activeSlot = 0;
		if (activeSlot >= count) activeSlot = count - 1;
	}

	bool canWriteSlot(int index) const {
		if (index < 0 || index >= 16) return false;
		int count = occupiedCount();
		return index <= count;
	}

	void cancelOperation() {
		operationState = OP_NONE;
		armedSlot = -1;
	}

	void deleteSlotAndShift(int index) {
		int count = occupiedCount();
		if (index < 0 || index >= count) return;

		bool deletedOrShiftedActiveScene = (activeSlot >= index);

		for (int i = index; i < count - 1; i++) {
			slotOccupied[i] = slotOccupied[i + 1];
			scenes[i] = scenes[i + 1];
		}
		slotOccupied[count - 1] = false;
		scenes[count - 1] = KArpScene();

		// Si la scène unique a été supprimée, K-SCENE redevient transparent.
		if (count - 1 <= 0) {
			activeSlot = -1;
			pendingSlot = -1;
			return;
		}

		// Option A :
		// - si activeSlot == index, il reste à cet index et pointe donc
		//   vers l'ancienne scène suivante ;
		// - si activeSlot > index, la scène active réelle a été décalée
		//   vers le haut, donc activeSlot recule d'un cran ;
		// - si on supprime le dernier slot actif, on revient au dernier
		//   slot encore occupé.
		if (activeSlot > index) {
			activeSlot--;
		}
		clampActiveSlot();

		// Cohérence STORE : si la suppression modifie le contenu du slot actif
		// ou force K-SCENE à choisir un nouveau slot actif, K-ARP doit afficher
		// immédiatement la scène effectivement active.
		if (deletedOrShiftedActiveScene) {
			applyActiveSceneToKArpForced();
		}
	}

	void handleSlotCtrlClick(int index) {
		if (index < 0 || index >= 16) return;
		if (!isStoreMode()) return;
		if (!slotOccupied[index]) return;
		if (!scenes[index].valid) return;

		// Raccourci d'édition en mode STORE :
		// Ctrl+Clic sur un slot occupé le rend actif et applique sa scène à K-ARP,
		// sans armer l'écriture, sans effacer, et sans modifier le contenu du slot.
		// On appelle directement host->applyScene() car applyActiveSceneToKArp()
		// est volontairement bloquée en STORE pour les rappels automatiques.
		cancelOperation();
		activeSlot = index;
		pendingSlot = -1;

		applyActiveSceneToKArpForced();
	}

	void handleSlotClick(int index) {
		if (index < 0 || index >= 16) return;

		if (isPlayMode()) {
			cancelOperation();

			// En mode AUTO, les clics sur les slots sont ignorés.
			if (isAutoMode()) {
				return;
			}

			// En mode PLAY MANUAL, seuls les slots occupés sont sélectionnables.
			if (slotOccupied[index]) {
				if (isKArpRunning()) {
					// Plus tard, ce pendingSlot sera appliqué en fin de cycle K-ARP.
					// Pour l'instant, il est mémorisé sans modifier la LED active.
					pendingSlot = index;
				}
				else {
					activeSlot = index;
					pendingSlot = -1;
					applyActiveSceneToKArp();
				}
			}
			return;
		}

		// Mode STORE.
		// K-SCENE ne rappelle/applique jamais de scène ici.
		// K-ARP reste libre : RUN/STOP/RST SEQ/édition restent possibles.
		// Si K-ARP est en lecture, STORE reste un mode d'écoute et d'édition
		// en temps réel : l'armement d'écriture ou de suppression est neutralisé.
		// Ctrl+Clic conserve son rôle de rappel explicite de scène via handleSlotCtrlClick().
		if (isKArpRunning()) {
			cancelOperation();
			return;
		}

		if (operationState == OP_WRITE_ARMED) {
			if (armedSlot == index) {
				if (canWriteSlot(index) && !isKArpRunning()) {
					KArpSceneHost* host = getKArpHost();
					if (host) {
						int previousCount = occupiedCount();

						host->captureScene(scenes[index]);
						scenes[index].valid = true;
						slotOccupied[index] = true;

						// Option B générale :
						// STORE ne modifie pas la scène active.
						// Exception nécessaire : première mémorisation.
						// K-SCENE sort de l'état transparent et le premier slot devient actif.
						if (previousCount == 0) {
							activeSlot = index;
						}
					}
				}
			}
			// Même slot = confirmation ; autre slot = annulation simple.
			cancelOperation();
			return;
		}

		if (operationState == OP_DELETE_ARMED) {
			if (armedSlot == index) {
				deleteSlotAndShift(index);
			}
			// Même slot = confirmation ; autre slot = annulation simple.
			cancelOperation();
			return;
		}

		// Aucune opération en cours :
		// clic = armement écriture si le slot respecte la continuité.
		if (canWriteSlot(index)) {
			operationState = OP_WRITE_ARMED;
			armedSlot = index;
		}
	}

	void handleSlotLongPress(int index) {
		if (index < 0 || index >= 16) return;
		if (!isStoreMode()) return;

		// En STORE avec K-ARP en lecture, les scènes peuvent être écoutées
		// et modifiées en temps réel, mais l'armement de suppression est désactivé.
		if (isKArpRunning()) {
			cancelOperation();
			return;
		}

		// Une opération déjà armée doit d'abord être annulée.
		// Un appui long ailleurs annule, mais n'arme rien.
		if (operationState != OP_NONE) {
			cancelOperation();
			return;
		}

		if (slotOccupied[index]) {
			operationState = OP_DELETE_ARMED;
			armedSlot = index;
		}
	}

	bool isStoreLedVisible(int index) {
		if (!isStoreMode()) return false;
		if (operationState != OP_WRITE_ARMED) return false;
		if (armedSlot != index) return false;
		return blinkPhase < 0.5f;
	}

	bool isDeleteSlotBlinkOn(int index) {
		if (!isStoreMode()) return false;
		if (operationState != OP_DELETE_ARMED) return false;
		if (armedSlot != index) return false;
		return blinkPhase < 0.5f;
	}

	bool isDeleteSlotArmed(int index) {
		if (!isStoreMode()) return false;
		if (operationState != OP_DELETE_ARMED) return false;
		return armedSlot == index;
	}

	bool isSlotLedOn(int index) {
		if (index < 0 || index >= 16) return false;

		// Règle prioritaire :
		// en STORE, le slot armé clignote toujours, même si c'est aussi
		// le slot actif. Le clignotement prime donc sur la LED fixe.
		if (isStoreMode() && armedSlot == index && operationState != OP_NONE) {
			return blinkPhase < 0.5f;
		}

		// Dans tous les autres cas :
		// LED fixe = slot actif.
		return activeSlot >= 0 && activeSlot == index;
	}

	bool isPendingSlotRingVisible(int index) {
		if (index < 0 || index >= 16) return false;

		// Anneau jaune uniquement en PLAY / MANUAL.
		// Il indique le slot demandé pour la prochaine scène.
		if (!isPlayMode()) return false;
		if (!isManualMode()) return false;
		if (!isKArpRunning()) return false;

		return pendingSlot == index && slotOccupied[index];
	}

	void resetSceneState() {
		for (int i = 0; i < 16; i++) {
			slotOccupied[i] = false;
			scenes[i] = KArpScene();
		}

		previousKArpRunning = false;
		previousPlayMode = true;
		pendingSlot = -1;
		lastSeenSceneCycleCounter = 0;
		autoLoop = true;
		eocOnRunReset = true;

		activeSlot = -1;
		armedSlot = -1;
		operationState = OP_NONE;
		blinkPhase = 0.f;
		eocPulseTimer = 0.f;

		params[STORE_PLAY_PARAM].setValue(1.f);
		params[AUTO_MANUAL_PARAM].setValue(0.f);
	}

	void onReset(const ResetEvent& e) override {
		(void)e;
		resetSceneState();
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_t* slotOccupiedJ = json_array();
		for (int i = 0; i < 16; i++) {
			json_array_append_new(slotOccupiedJ, json_boolean(slotOccupied[i]));
		}
		json_object_set_new(rootJ, "slotOccupied", slotOccupiedJ);

		json_t* scenesJ = json_array();
		for (int i = 0; i < 16; i++) {
			json_t* sceneJ = json_object();

			json_object_set_new(sceneJ, "valid", json_boolean(scenes[i].valid));
			json_object_set_new(sceneJ, "bpm", json_real(scenes[i].bpm));
			json_object_set_new(sceneJ, "chordLen", json_integer(scenes[i].chordLen));
			json_object_set_new(sceneJ, "arpLen", json_integer(scenes[i].arpLen));

			json_t* chordSliderValuesJ = json_array();
			json_t* chordTypesJ = json_array();
			json_t* chordOctavesJ = json_array();
			json_t* chordTypeVisibleJ = json_array();

			for (int c = 0; c < 16; c++) {
				json_array_append_new(chordSliderValuesJ, json_real(scenes[i].chordSliderValues[c]));
				json_array_append_new(chordTypesJ, json_integer(scenes[i].chordTypes[c]));
				json_array_append_new(chordOctavesJ, json_integer(scenes[i].chordOctaves[c]));
				json_array_append_new(chordTypeVisibleJ, json_boolean(scenes[i].chordTypeVisible[c]));
			}

			json_object_set_new(sceneJ, "chordSliderValues", chordSliderValuesJ);
			json_object_set_new(sceneJ, "chordTypes", chordTypesJ);
			json_object_set_new(sceneJ, "chordOctaves", chordOctavesJ);
			json_object_set_new(sceneJ, "chordTypeVisible", chordTypeVisibleJ);

			json_t* arpMatrixJ = json_array();
			for (int row = 0; row < 6; row++) {
				json_t* rowJ = json_array();
				for (int col = 0; col < 16; col++) {
					json_array_append_new(rowJ, json_boolean(scenes[i].arpMatrix[row][col]));
				}
				json_array_append_new(arpMatrixJ, rowJ);
			}
			json_object_set_new(sceneJ, "arpMatrix", arpMatrixJ);

			json_array_append_new(scenesJ, sceneJ);
		}
		json_object_set_new(rootJ, "scenes", scenesJ);

		json_object_set_new(rootJ, "activeSlot", json_integer(activeSlot));
		json_object_set_new(rootJ, "pendingSlot", json_integer(pendingSlot));
		json_object_set_new(rootJ, "autoLoop", json_boolean(autoLoop));
		json_object_set_new(rootJ, "eocOnRunReset", json_boolean(eocOnRunReset));

		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* slotOccupiedJ = json_object_get(rootJ, "slotOccupied");
		if (slotOccupiedJ) {
			for (int i = 0; i < 16; i++) {
				json_t* occupiedJ = json_array_get(slotOccupiedJ, i);
				if (occupiedJ) {
					slotOccupied[i] = json_boolean_value(occupiedJ);
				}
			}
		}

		json_t* scenesJ = json_object_get(rootJ, "scenes");
		if (scenesJ) {
			for (int i = 0; i < 16; i++) {
				json_t* sceneJ = json_array_get(scenesJ, i);
				if (!sceneJ) continue;

				json_t* validJ = json_object_get(sceneJ, "valid");
				if (validJ) scenes[i].valid = json_boolean_value(validJ);

				json_t* bpmJ = json_object_get(sceneJ, "bpm");
				if (bpmJ) {
					scenes[i].bpm = (float)json_number_value(bpmJ);
					if (scenes[i].bpm < 20.f) scenes[i].bpm = 20.f;
					if (scenes[i].bpm > 300.f) scenes[i].bpm = 300.f;
				}

				json_t* chordLenJ = json_object_get(sceneJ, "chordLen");
				if (chordLenJ) {
					scenes[i].chordLen = (int)json_integer_value(chordLenJ);
					if (scenes[i].chordLen < 1) scenes[i].chordLen = 1;
					if (scenes[i].chordLen > 16) scenes[i].chordLen = 16;
				}

				json_t* arpLenJ = json_object_get(sceneJ, "arpLen");
				if (arpLenJ) {
					scenes[i].arpLen = (int)json_integer_value(arpLenJ);
					if (scenes[i].arpLen < 1) scenes[i].arpLen = 1;
					if (scenes[i].arpLen > 16) scenes[i].arpLen = 16;
				}

				json_t* chordSliderValuesJ = json_object_get(sceneJ, "chordSliderValues");
				json_t* chordTypesJ = json_object_get(sceneJ, "chordTypes");
				json_t* chordOctavesJ = json_object_get(sceneJ, "chordOctaves");
				json_t* chordTypeVisibleJ = json_object_get(sceneJ, "chordTypeVisible");

				for (int c = 0; c < 16; c++) {
					if (chordSliderValuesJ) {
						json_t* vJ = json_array_get(chordSliderValuesJ, c);
						if (vJ) {
							float v = (float)json_number_value(vJ);
							if (v < 0.f) v = 0.f;
							if (v > 1.f) v = 1.f;
							scenes[i].chordSliderValues[c] = v;
						}
					}

					if (chordTypesJ) {
						json_t* tJ = json_array_get(chordTypesJ, c);
						if (tJ) {
							int type = (int)json_integer_value(tJ);
							if (type < 0) type = 0;
							if (type > 4) type = 0;
							scenes[i].chordTypes[c] = type;
						}
					}

					if (chordOctavesJ) {
						json_t* oJ = json_array_get(chordOctavesJ, c);
						if (oJ) {
							int octave = (int)json_integer_value(oJ);
							if (octave < 0) octave = 0;
							if (octave > 8) octave = 8;
							scenes[i].chordOctaves[c] = octave;
						}
					}

					if (chordTypeVisibleJ) {
						json_t* visibleJ = json_array_get(chordTypeVisibleJ, c);
						if (visibleJ) scenes[i].chordTypeVisible[c] = json_boolean_value(visibleJ);
					}
				}

				json_t* arpMatrixJ = json_object_get(sceneJ, "arpMatrix");
				if (arpMatrixJ) {
					for (int row = 0; row < 6; row++) {
						json_t* rowJ = json_array_get(arpMatrixJ, row);
						if (!rowJ) continue;

						for (int col = 0; col < 16; col++) {
							json_t* cellJ = json_array_get(rowJ, col);
							if (cellJ) {
								scenes[i].arpMatrix[row][col] = json_boolean_value(cellJ);
							}
						}
					}
				}
			}
		}

		json_t* activeSlotJ = json_object_get(rootJ, "activeSlot");
		if (activeSlotJ) activeSlot = (int)json_integer_value(activeSlotJ);

		json_t* pendingSlotJ = json_object_get(rootJ, "pendingSlot");
		if (pendingSlotJ) pendingSlot = (int)json_integer_value(pendingSlotJ);

		json_t* autoLoopJ = json_object_get(rootJ, "autoLoop");
		if (autoLoopJ) autoLoop = json_boolean_value(autoLoopJ);

		json_t* eocOnRunResetJ = json_object_get(rootJ, "eocOnRunReset");
		if (eocOnRunResetJ) eocOnRunReset = json_boolean_value(eocOnRunResetJ);

		cancelOperation();
		clampActiveSlot();
	}

};

static void drawKSceneBoldText(NVGcontext* vg, float x, float y, const char* text) {
	nvgText(vg, x, y, text, NULL);
	nvgText(vg, x + 0.45f, y, text, NULL);
}

struct KScenePanelScrew : TransparentWidget {
	KScenePanelScrew(Vec posMm) {
		box.pos = mm2px(posMm);
		box.size = mm2px(Vec(3.8f, 3.8f));
	}

	void draw(const DrawArgs& args) override {
		float cx = box.size.x * 0.5f;
		float cy = box.size.y * 0.5f;
		float r = mm2px(1.65f);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, r);
		nvgFillColor(args.vg, nvgRGB(45, 49, 48));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(105, 110, 108));
		nvgStrokeWidth(args.vg, 0.75f);
		nvgStroke(args.vg);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, cx - r * 0.55f, cy);
		nvgLineTo(args.vg, cx + r * 0.55f, cy);
		nvgMoveTo(args.vg, cx, cy - r * 0.55f);
		nvgLineTo(args.vg, cx, cy + r * 0.55f);
		nvgStrokeColor(args.vg, nvgRGB(135, 140, 138));
		nvgStrokeWidth(args.vg, 0.55f);
		nvgStroke(args.vg);
	}
};

struct KSceneTitleLabel : TransparentWidget {
	KSceneTitleLabel() {
		box.size = mm2px(Vec(30.48f, 128.5f));
	}

	void draw(const DrawArgs& args) override {
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans-Bold.ttf"));
		if (!font) font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
		if (!font) return;

		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, 16.0f);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(args.vg, nvgRGB(235, 235, 235));

		nvgText(args.vg, mm2px(15.24f), mm2px(9.8f), "K-SCENE", NULL);
		nvgText(args.vg, mm2px(15.24f) + 0.55f, mm2px(9.8f), "K-SCENE", NULL);
	}
};

struct KSceneSwitch : app::ParamWidget {
	KScene* module = NULL;
	bool disabledWhenStore = false;

	KSceneSwitch() {
		box.size = mm2px(Vec(8.8f, 3.3f));
	}

	void draw(const DrawArgs& args) override {
		float value = 1.f;
		if (getParamQuantity()) {
			value = getParamQuantity()->getValue();
		}

		bool rightSelected = value >= 0.5f;

		float x = box.size.x * 0.5f;
		float y = box.size.y * 0.5f;

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, x - mm2px(4.4f), y - mm2px(1.65f), mm2px(8.8f), mm2px(3.3f), mm2px(1.4f));
		nvgFillColor(args.vg, nvgRGB(32, 34, 34));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(105, 110, 110));
		nvgStrokeWidth(args.vg, 0.80f);
		nvgStroke(args.vg);

		float knobX = x + mm2px(rightSelected ? 2.15f : -2.15f);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, knobX, y, mm2px(1.45f));
		nvgFillColor(args.vg, nvgRGB(228, 228, 220));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
		nvgStrokeWidth(args.vg, 0.70f);
		nvgStroke(args.vg);

	}

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (disabledWhenStore && module && !module->isPlayMode()) {
				e.consume(this);
				return;
			}

			if (getParamQuantity()) {
				float current = getParamQuantity()->getValue();
				getParamQuantity()->setValue(current >= 0.5f ? 0.f : 1.f);
			}
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
};

struct KSceneModeLabels : TransparentWidget {
	KScene* module = NULL;

	KSceneModeLabels(KScene* module) {
		this->module = module;
		box.size = mm2px(Vec(30.48f, 128.5f));
	}

	void draw(const DrawArgs& args) override {
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
		if (!font) return;

		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontSize(args.vg, 10.0f);

		// Indicateur visuel principal PLAY / STORE :
		// cartouche jaune vif (#dcff46) derrière le libellé actif.
		bool playActive = module && module->isPlayMode();
		bool storeActive = module && !module->isPlayMode();

		if (module) {
			float x = playActive ? 23.8f : 8.1f;
			float w = playActive ? 8.8f : 12.4f;

			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, mm2px(x - w * 0.5f), mm2px(18.2f - 3.0f), mm2px(w), mm2px(6.0f), mm2px(1.1f));
			nvgFillColor(args.vg, nvgRGB(220, 255, 70));
			nvgFill(args.vg);
		}

		nvgFillColor(args.vg, storeActive ? nvgRGB(20, 20, 20) : nvgRGB(235, 235, 235));
		drawKSceneBoldText(args.vg, mm2px(8.1f),  mm2px(storeActive ? 18.55f : 19.0f), "STORE");

		nvgFillColor(args.vg, playActive ? nvgRGB(20, 20, 20) : nvgRGB(235, 235, 235));
		drawKSceneBoldText(args.vg, mm2px(23.8f), mm2px(playActive ? 18.55f : 19.0f), "PLAY");

		nvgFillColor(args.vg, nvgRGB(235, 235, 235));
		drawKSceneBoldText(args.vg, mm2px(7.5f),  mm2px(32.0f), "AUTO");
		drawKSceneBoldText(args.vg, mm2px(22.4f), mm2px(32.0f), "MANUAL");
	}
};

struct KSceneSlotButton : Widget {
	KScene* module = NULL;
	int slotIndex = 0;
	bool pressed = false;
	std::chrono::steady_clock::time_point pressTime;

	KSceneSlotButton(KScene* module, int index) {
		this->module = module;
		this->slotIndex = index;
		box.size = mm2px(Vec(5.2f, 5.2f));
	}

	void draw(const DrawArgs& args) override {
		bool occupied = false;
		bool deleteArmed = false;
		bool deleteBlinkOn = false;

		if (module) {
			occupied = module->isSlotOccupied(slotIndex);
			deleteArmed = module->isDeleteSlotArmed(slotIndex);
			deleteBlinkOn = module->isDeleteSlotBlinkOn(slotIndex);
		}

		if (deleteArmed && !deleteBlinkOn) {
			occupied = false;
		}

		float cx = box.size.x * 0.5f;
		float cy = box.size.y * 0.5f;

		bool pendingRing = false;
		if (module) {
			pendingRing = module->isPendingSlotRingVisible(slotIndex);
		}

		if (pendingRing) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, mm2px(2.70f));
			nvgStrokeColor(args.vg, nvgRGB(255, 220, 70));
			nvgStrokeWidth(args.vg, 1.15f);
			nvgStroke(args.vg);
		}

		NVGcolor fillColor = occupied ? nvgRGB(145, 255, 145) : nvgRGB(65, 135, 75);
		NVGcolor strokeColor = occupied ? nvgRGB(215, 255, 215) : nvgRGB(105, 180, 110);

		if (occupied) {
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
		nvgStrokeWidth(args.vg, occupied ? 1.25f : 0.85f);
		nvgStroke(args.vg);
	}

	void onButton(const event::Button& e) override {
		if (module && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (e.action == GLFW_PRESS) {
				if ((e.mods & GLFW_MOD_CONTROL) && module->isStoreMode()) {
					pressed = false;
					module->handleSlotCtrlClick(slotIndex);
					e.consume(this);
					return;
				}

				pressed = true;
				pressTime = std::chrono::steady_clock::now();
				e.consume(this);
				return;
			}

			if (e.action == GLFW_RELEASE && pressed) {
				pressed = false;

				auto now = std::chrono::steady_clock::now();
				float duration = std::chrono::duration<float>(now - pressTime).count();

				if (duration >= 0.75f) {
					module->handleSlotLongPress(slotIndex);
				}
				else {
					module->handleSlotClick(slotIndex);
				}

				e.consume(this);
				return;
			}
		}

		Widget::onButton(e);
	}
};

struct KSceneSlotLed : Widget {
	KScene* module = NULL;
	int slotIndex = 0;

	KSceneSlotLed(KScene* module, int index) {
		this->module = module;
		this->slotIndex = index;
		box.size = mm2px(Vec(3.0f, 3.0f));
	}

	void draw(const DrawArgs& args) override {
		bool on = false;

		if (module) {
			on = module->isSlotLedOn(slotIndex);
		}

		float cx = box.size.x * 0.5f;
		float cy = box.size.y * 0.5f;

		if (on) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, mm2px(1.45f));
			nvgFillColor(args.vg, nvgRGBA(150, 255, 150, 60));
			nvgFill(args.vg);
		}

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, mm2px(0.95f));
		nvgFillColor(args.vg, on ? nvgRGB(150, 255, 150) : nvgRGB(35, 42, 38));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, mm2px(1.10f));
		nvgStrokeColor(args.vg, on ? nvgRGB(215, 255, 215) : nvgRGB(75, 85, 78));
		nvgStrokeWidth(args.vg, 0.70f);
		nvgStroke(args.vg);
	}
};

struct KSceneBottomLabels : TransparentWidget {
	KSceneBottomLabels() {
		box.size = mm2px(Vec(30.48f, 128.5f));
	}

	void draw(const DrawArgs& args) override {
		std::shared_ptr<Font> font = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
		if (!font) return;

		nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontSize(args.vg, 10.0f);
		nvgFillColor(args.vg, nvgRGB(235, 235, 235));

		drawKSceneBoldText(args.vg, mm2px(23.0f), mm2px(109.2f), "EOC");

	}
};

struct KSceneLogoWidget : TransparentWidget {
	KSceneLogoWidget() {
		box.size = mm2px(Vec(30.48f, 128.5f));
	}

	void draw(const DrawArgs& args) override {
		const float minX = 1054.6789f;
		const float minY = 545.1527f;
		const float logoW = 687.2638f;
		const float logoH = 898.1945f;

		// Logo légèrement réduit et remonté pour mieux s’aligner
		// visuellement avec le logo de K-ARP lorsque les modules sont accolés.
		const float wMm = 7.8f;
		const float hMm = wMm * (logoH / logoW);
		const float xMm = 4.05f;
		const float yMm = 107.8f;

		nvgSave(args.vg);
		nvgTranslate(args.vg, mm2px(xMm), mm2px(yMm));
		nvgScale(args.vg, mm2px(wMm) / logoW, mm2px(hMm) / logoH);
		nvgTranslate(args.vg, -minX, -minY);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 1054.6789f, 545.1527f);
		nvgLineTo(args.vg, 1741.9427f, 545.1527f);
		nvgLineTo(args.vg, 1741.9427f, 1324.3003f);
		nvgLineTo(args.vg, 1054.6772f, 1443.3472f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgRGB(192, 192, 192));
		nvgFill(args.vg);

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

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 1303.0602f, 1117.1645f);
		nvgLineTo(args.vg, 1304.9121f, 1314.0937f);
		nvgLineTo(args.vg, 1318.0851f, 1344.3270f);
		nvgLineTo(args.vg, 1409.3598f, 1334.5125f);
		nvgLineTo(args.vg, 1402.7225f, 1296.5537f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, nvgRGB(192, 192, 192));
		nvgFill(args.vg);

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

struct KSceneClearScenesItem : MenuItem {
	KScene* module = NULL;

	void onAction(const event::Action& e) override {
		(void)e;
		if (module) {
			module->resetSceneState();
		}
	}
};

struct KSceneWidget : ModuleWidget {
	void appendContextMenu(Menu* menu) override {
		KScene* module = dynamic_cast<KScene*>(this->module);

		menu->addChild(new MenuSeparator);

		if (module) {
			menu->addChild(createBoolPtrMenuItem("Loop AUTO", "", &module->autoLoop));
			menu->addChild(createBoolPtrMenuItem("EOC on RUN/RST", "", &module->eocOnRunReset));

			KSceneClearScenesItem* clearScenesItem = new KSceneClearScenesItem;
			clearScenesItem->text = "Clear all scenes";
			clearScenesItem->module = module;
			menu->addChild(clearScenesItem);
		}
	}

	bool isPointInsideSceneSlot(Vec p) {
		static const float ROW_Y[8] = {45, 52, 59, 66, 73, 80, 87, 94};
		static const float SLOT_X[2] = {6.6f, 17.3f};
		const float slotW = 5.2f;
		const float slotH = 5.2f;

		for (int col = 0; col < 2; col++) {
			for (int row = 0; row < 8; row++) {
				float x = mm2px(SLOT_X[col]);
				float y = mm2px(ROW_Y[row]);
				float w = mm2px(slotW);
				float h = mm2px(slotH);

				if (p.x >= x && p.x <= x + w && p.y >= y && p.y <= y + h) {
					return true;
				}
			}
		}

		return false;
	}

	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			KScene* kscene = dynamic_cast<KScene*>(this->module);
			if (kscene && kscene->isStoreMode() && kscene->operationState != KScene::OP_NONE) {
				// STORE: a click on a free area of the panel cancels
				// the currently armed STORE or DELETE operation.
				// Clicks inside Scene slots must keep their own behavior:
				// same slot = confirm, another slot = cancel.
				if (!isPointInsideSceneSlot(e.pos)) {
					kscene->cancelOperation();
					e.consume(this);
					return;
				}
			}
		}

		ModuleWidget::onButton(e);
	}

	KSceneWidget(KScene* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/KScene.svg")));

		addChild(new KScenePanelScrew(Vec(2.0f, 3.0f)));
		addChild(new KScenePanelScrew(Vec(2.0f, 122.0f)));

		addChild(new KSceneTitleLabel());
		addChild(new KSceneModeLabels(module));

		KSceneSwitch* storePlaySwitch = createParamCentered<KSceneSwitch>(
			mm2px(Vec(15.24f, 23.5f)),
			module,
			KScene::STORE_PLAY_PARAM
		);
		storePlaySwitch->module = module;
		addParam(storePlaySwitch);

		KSceneSwitch* autoManualSwitch = createParamCentered<KSceneSwitch>(
			mm2px(Vec(15.24f, 36.5f)),
			module,
			KScene::AUTO_MANUAL_PARAM
		);
		autoManualSwitch->module = module;
		autoManualSwitch->disabledWhenStore = true;
		addParam(autoManualSwitch);

		static const float ROW_Y[8] = {45, 52, 59, 66, 73, 80, 87, 94};

		for (int i = 0; i < 8; i++) {
			auto* leftLed = new KSceneSlotLed(module, i);
			leftLed->box.pos = mm2px(Vec(3.2f, ROW_Y[i] + 1.1f));
			addChild(leftLed);

			auto* leftSlot = new KSceneSlotButton(module, i);
			leftSlot->box.pos = mm2px(Vec(6.6f, ROW_Y[i]));
			addChild(leftSlot);

			auto* rightSlot = new KSceneSlotButton(module, i + 8);
			rightSlot->box.pos = mm2px(Vec(17.3f, ROW_Y[i]));
			addChild(rightSlot);

			auto* rightLed = new KSceneSlotLed(module, i + 8);
			rightLed->box.pos = mm2px(Vec(24.2f, ROW_Y[i] + 1.1f));
			addChild(rightLed);
		}

		addChild(new KSceneLogoWidget());
		addChild(new KSceneBottomLabels());

		addOutput(createOutputCentered<PJ301MPort>(
			mm2px(Vec(23.0f, 116.0f)),
			module,
			KScene::EOC_OUTPUT
		));
	}
};

Model* modelKScene = createModel<KScene, KSceneWidget>("KScene");
