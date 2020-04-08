/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

#include "AkaifireCHOP.hpp"

void callback(double deltatime, std::vector<unsigned char>* message, void* userData)
{
	// pads
	if (message->at(1) >= 54 && message->at(1) <= 117)
	{
		int pos = message->at(1) - 54;
		int row = 3 - pos / 16;
		int col = pos % 16;
		int index = row * 16 + col;
		chan_values[index] = message->at(0) == 144 ? 1 : 0;
	}

	// select / bank
	else if (message->at(1) >= 25 && message->at(1) <= 26)
	{
		int index = message->at(1) + 39;
		chan_values[index] = message->at(0) == 144 ? 1 : 0;
	}

	// others
	else if (message->at(1) >= 31 && message->at(1) <= 39)
	{
		int index = message->at(1) + 35;
		chan_values[index] = message->at(0) == 144 ? 1 : 0;
	}
	else if (message->at(1) >= 44 && message->at(1) <= 53)
	{
		int index = message->at(1) + 31;
		chan_values[index] = message->at(0) == 144 ? 1 : 0;
	}

	// rotary encoder
	else if (message->at(0) >= 176)
	{
		if (message->at(1) >= 16 && message->at(1) <= 19)
		{
			int index = message->at(1) + 69;
			if (message->at(2) > 0x40)
				chan_values[index] -= (0x80 - message->at(2));
			else
				chan_values[index] += message->at(2);
		}
		else if (message->at(1) == 118)
		{
			int index = 89;
			if (message->at(2) > 0x40)
				chan_values[index] -= (0x80 - message->at(2));
			else 
				chan_values[index] += message->at(2);
		}
	}

}

Midi::Midi()
{
	midiin = new RtMidiIn();
	midiout = new RtMidiOut();

	midiin->ignoreTypes(false, false, false);

	in_devices.clear();
	out_devices.clear();

	unsigned nPorts = midiin->getPortCount();
	for (int i = 0; i < nPorts; i++) {
		string portName = midiin->getPortName(i);
		in_devices.push_back(portName);
	}

	nPorts = midiout->getPortCount();
	for (int i = 0; i < nPorts; i++) {
		string portName = midiout->getPortName(i);
		out_devices.push_back(portName);
	}
}

Midi::~Midi()
{
	delete midiin;
	delete midiout;
}

bool Midi::setupIn(unsigned int index)
{
	unsigned nPorts = midiin->getPortCount();
	if (midiin->isPortOpen()) {
		midiin->closePort();
	}
	if (nPorts > index) {
		string name = midiin->getPortName(index);
		if (!strncmp(name.c_str(), "FL STUDIO FIRE", 14)) {
			midiin->openPort(index);
			midiin->setCallback(&callback);
			return true;
		}
	}

	if (midiin->isPortOpen()) {
		midiin->closePort();
	}
	return false;
}

bool Midi::setupOut(unsigned int index)
{
	unsigned nPorts = midiout->getPortCount();
	if (midiout->isPortOpen()) {
		midiout->closePort();
	}
	if (nPorts > index) {
		string name = midiout->getPortName(index);
		if (!strncmp(name.c_str(), "FL STUDIO FIRE", 14)) {
			midiout->openPort(index);
			return true;
		}
	}

	if (midiout->isPortOpen()) {
		midiout->closePort();
	}
	return false;
}

void Midi::sendMessage(vector<unsigned char>* message)
{
	if (midiout->isPortOpen()) {
		midiout->sendMessage(message);
	}
}

bool Midi::isInPortOpen()
{
	return midiin->isPortOpen();
}

bool Midi::isOutPortOpen()
{
	return midiout->isPortOpen();
}

AkaifireCHOP::AkaifireCHOP(const OP_NodeInfo* info)
{
	chan_values.assign(chan_values.size(), 0.0f);
	last_pixels.assign(64, NULL);

	chan_names.assign(64, "b1");
	chan_values.assign(64, 0.0);

	chan_names.push_back("bselect");
	chan_values.push_back(0.0);

	chan_names.push_back("bbank");
	chan_values.push_back(0.0);

	for (int i = 0; i < button_led_names.size(); i++) {
		if (i == 5) {
			for (int j = 0; j < 4; j++) {
				chan_names.push_back("bsolo1");
				chan_values.push_back(0.0);
			}
		}
		auto name = "b" + button_led_names[i];
		chan_names.push_back(name);
		chan_values.push_back(0.0);
	}

	vector<string> knobs = { "volume", "pan", "filter", "resonance", "select" };
	for (auto name : knobs) {
		chan_names.push_back(name);
		chan_values.push_back(0.0);
	}
}

AkaifireCHOP::~AkaifireCHOP()
{
	stopThread();
}

void AkaifireCHOP::getGeneralInfo(CHOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved1)
{
	ginfo->cookEveryFrameIfAsked = true;
	ginfo->timeslice = false;
	ginfo->inputMatchIndex = 0;
}

bool AkaifireCHOP::getOutputInfo(CHOP_OutputInfo* info, const OP_Inputs* inputs, void* reserved1)
{
	info->numSamples = 1;
	info->numChannels = (int32_t)chan_names.size();
	return true;
}

void AkaifireCHOP::getChannelName(int32_t index, OP_String* name, const OP_Inputs* inputs, void* reserved1)
{
	name->setString(chan_names[index].c_str());
}

void AkaifireCHOP::execute(CHOP_Output* output, const OP_Inputs* inputs, void* reserved)
{
	executeHandleParameters(inputs);
	executeHandlePadsInputs(inputs);

	if (!thread_active) {
		startThread();
	}

	for (int i = 0; i < chan_values.size(); i++)
		for (int j = 0; j < output->numSamples; j++)
			output->channels[i][j] = chan_values[i];
}

int32_t AkaifireCHOP::getNumInfoCHOPChans(void* reserved1)
{
	return 0;
}

void AkaifireCHOP::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved1)
{
}

bool AkaifireCHOP::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
	infoSize->rows = midi.in_devices.size() + midi.out_devices.size();
	infoSize->cols = 3;
	infoSize->byColumn = false;
	return true;
}

void AkaifireCHOP::getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries* entries, void* reserved1)
{
	if (index < midi.in_devices.size()) {
		string device = midi.in_devices[index];
		entries->values[0]->setString("IN");
		entries->values[1]->setString(to_string(index).c_str());
		entries->values[2]->setString(device.c_str());
	}
	else {
		index -= midi.in_devices.size();
		string device = midi.out_devices[index];
		entries->values[0]->setString("OUT");
		entries->values[1]->setString(to_string(index).c_str());
		entries->values[2]->setString(device.c_str());
	}
}

void AkaifireCHOP::setupParameters(OP_ParameterManager* manager, void* reserved1)
{
	{
		OP_NumericParameter np;

		np.name = "Indevice";
		np.label = "In Device";
		np.minValues[0] = 0;
		np.maxValues[0] = midi.in_devices.size() - 1;
		np.minSliders[0] = 0;
		np.maxSliders[0] = midi.in_devices.size() - 1;
		np.clampMins[0] = true;
		np.clampMaxes[0] = true;

		OP_ParAppendResult res = manager->appendInt(np);
		assert(res == OP_ParAppendResult::Success);
	}
	{
		OP_NumericParameter np;

		np.name = "Outdevice";
		np.label = "Out Device";
		np.minValues[0] = 0;
		np.maxValues[0] = midi.out_devices.size() - 1;
		np.minSliders[0] = 0;
		np.maxSliders[0] = midi.out_devices.size() - 1;
		np.clampMins[0] = true;
		np.clampMaxes[0] = true;

		OP_ParAppendResult res = manager->appendInt(np);
		assert(res == OP_ParAppendResult::Success);
	}
	for (auto n : button_led_names) {
		OP_NumericParameter np;
		string name = n;
		name = "Led" + name;
		np.name = name.c_str();

		string label = n;
		label[0] = toupper(label[0]);
		label = "Led " + label;
		np.label = label.c_str();

		auto led = button_leds[np.name];
		np.minValues[0] = led.min_value;
		np.maxValues[0] = led.max_value;
		np.minSliders[0] = led.min_value;
		np.maxSliders[0] = led.max_value;
		np.clampMins[0] = true;
		np.clampMaxes[0] = true;

		OP_ParAppendResult res = manager->appendInt(np);
		assert(res == OP_ParAppendResult::Success);
	}
	{
		OP_NumericParameter np;

		np.name = "Ledsolo";
		np.label = "LED Solo";
		for (int i = 0; i < 4; i++) {
			np.minValues[i] = 0;
			np.maxValues[i] = 3;
			np.clampMins[i] = true;
			np.clampMaxes[i] = true;
		}

		OP_ParAppendResult res = manager->appendInt(np, 4);
		assert(res == OP_ParAppendResult::Success);
	}
	{
		OP_NumericParameter np;

		np.name = "Ledrect";
		np.label = "LED Rect";
		for (int i = 0; i < 4; i++) {
			np.minValues[i] = 0;
			np.maxValues[i] = 4;
			np.clampMins[i] = true;
			np.clampMaxes[i] = true;
		}

		OP_ParAppendResult res = manager->appendInt(np, 4);
		assert(res == OP_ParAppendResult::Success);
	}
	{
		OP_NumericParameter np;

		np.name = "Ledbank";
		np.label = "LED Bank";
		for (int i = 0; i < 4; i++) {
			np.minValues[i] = 0;
			np.maxValues[i] = 1;
			np.clampMins[i] = true;
			np.clampMaxes[i] = true;
		}

		OP_ParAppendResult res = manager->appendInt(np, 4);
		assert(res == OP_ParAppendResult::Success);
	}
	{
		OP_StringParameter sp;

		sp.name = "Padstop";
		sp.label = "Pads TOP";

		OP_ParAppendResult res = manager->appendTOP(sp);
		assert(res == OP_ParAppendResult::Success);
	}
}

void AkaifireCHOP::pulsePressed(const char* name, void* reserved1)
{
}

void AkaifireCHOP::startThread()
{
	cout << "thread start" << endl;
	thread_active = true;

	send_thread = thread([this]()
	{
		while (thread_active)
		{
			while (!que.empty())
			{
				//cout << "send message" << endl;
				auto message = que.front();
				midi.sendMessage(&message);
				que.pop();
			}
		}
	});
}

void AkaifireCHOP::stopThread()
{
	cout << "thread stop" << endl;
	thread_active = false;

	if (send_thread.joinable())
		send_thread.join();
}

void AkaifireCHOP::executeHandleParameters(const OP_Inputs* inputs)
{
	auto _in = inputs->getParInt("Indevice");
	if (in_device_id != _in) {
		in_device_id = _in;
		midi.setupIn(in_device_id);
	}

	auto _out = inputs->getParInt("Outdevice");
	if (out_device_id != _out) {
		out_device_id = _out;
		midi.setupOut(out_device_id);
	}

	/* Bank */
	{
		unsigned char bank = 0x10;
		int32_t banks[4];
		inputs->getParInt4("Ledbank", banks[0], banks[1], banks[2], banks[3]);
		for (int i = 0; i < 4; i++) {
			if (banks[i] >= 1) bank |= (1 << i);
			else bank &= ~(1 << i);
		}
		if (bank_leds != bank) {
			vector<unsigned char> message = { 0xb0, 0x1b, bank };
			que.push(message);
			bank_leds = bank;
		}
	}

	/* SOLO */
	{
		int32_t solos[4];
		char name[9];
		inputs->getParInt4("Ledsolo", solos[0], solos[1], solos[2], solos[3]);
		for (int i = 0; i < 4; i++) {
			sprintf_s(name, "Ledsolo%d", (i + 1));
			auto value = (unsigned char)solos[i];
			auto led = button_leds[name];
			if (led.value != value) {
				vector<unsigned char> message = { 0xb0, led.address, value };
				que.push(message);
				cout << "send solo" << endl;
			}
			button_leds[name].value = value;
		}
	}

	/* RECT */
	{
		int32_t v[4];
		char name[9];
		inputs->getParInt4("Ledrect", v[0], v[1], v[2], v[3]);
		for (int i = 0; i < 4; i++) {
			sprintf_s(name, "Ledrect%d", (i + 1));
			auto value = (unsigned char)v[i];
			auto led = button_leds[name];
			if (led.value != value) {
				vector<unsigned char> message = { 0xb0, led.address, value };
				que.push(message);
			}
			button_leds[name].value = value;
		}
	}

	/* Other LEDs play, stop, rec and more */
	{
		for (auto n : button_led_names) {
			auto name = "Led" + n;
			auto value = (unsigned char)inputs->getParInt(name.c_str());
			auto led = button_leds[name];
			if (led.value != value) {
				vector<unsigned char> message = { 0xb0, led.address, value };
				que.push(message);
			}
			button_leds[name].value = value;
		}
	}
}

void AkaifireCHOP::executeHandleInputs(const OP_Inputs* inputs)
{
}

void AkaifireCHOP::executeHandlePadsInputs(const OP_Inputs* inputs)
{
	auto tinput = inputs->getParTOP("Padstop");
	if (tinput)
	{
		// different size
		if (tinput->width != 16 || tinput->height != 4) {
			return;
		}

		int npixels = tinput->width * tinput->height;

		OP_TOPInputDownloadOptions options;
		options.cpuMemPixelType = OP_CPUMemPixelType::RGBA8Fixed;
		options.verticalFlip = true;

		auto pixels = (const uint32_t*)inputs->getTOPDataInCPUMemory(tinput, &options);
		if (!pixels) return;

		vector<int> indexes;
		for (int i = 0; i < npixels; i++) {
			if (pixels[i] != last_pixels[i]) {
				indexes.push_back(i);
			}
			last_pixels[i] = pixels[i];
		}

		if (indexes.size() > 0) {
			handlePadsIndexes(indexes);
		}
	}
}

void AkaifireCHOP::handlePadsIndexes(vector<int> indexes)
{
	int max_length = 24;

	vector<unsigned char> message = { 0xf0, 0x47, 0x7f, 0x43, 0x65, 0x00, 0x00 };

	for (int i = 0; i < max_length && i < indexes.size(); i++) {
		int index = indexes[i];
		message.push_back(index);
		message.push_back(last_pixels[index] / 2 & 0x7f);         // R
		message.push_back((last_pixels[index] >> 8) / 2 & 0x7f);  // G
		message.push_back((last_pixels[index] >> 16) / 2 & 0x7f); // B
	}
	message.push_back(0xf7);

	int message_len = message.size() - 8;
	message[5] = message_len >> 7;
	message[6] = message_len & 0x7f;

	if (midi.isOutPortOpen()) {
		que.push(message);
	}

	if (indexes.size() > max_length) {
		indexes.erase(indexes.begin(), indexes.begin() + max_length);
		handlePadsIndexes(indexes);
	}
}

extern "C"
{
	DLLEXPORT void FillCHOPPluginInfo(CHOP_PluginInfo* info)
	{
		info->apiVersion = CHOPCPlusPlusAPIVersion;
		info->customOPInfo.opType->setString("Akaifire");
		info->customOPInfo.opLabel->setString("Akaifire");
		info->customOPInfo.authorName->setString("Akira Kamikura");
		info->customOPInfo.authorEmail->setString("akira.kamikura@gmail.com");
	}

	DLLEXPORT CHOP_CPlusPlusBase* CreateCHOPInstance(const OP_NodeInfo* info)
	{
		return new AkaifireCHOP(info);
	}

	DLLEXPORT void DestroyCHOPInstance(CHOP_CPlusPlusBase* instance)
	{
		delete (AkaifireCHOP*)instance;
	}

};

