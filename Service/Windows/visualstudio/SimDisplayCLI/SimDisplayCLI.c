/*
simdisplay - A simracing dashboard created using Arduino to show shared memory
             telemetry from Assetto Corsa Competizione.

Copyright (C) 2020  Filippo Erik Negroni

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <windows.h>
#include <string.h>

#include <stdio.h>
#include <math.h>
#include <stdint.h>

#include "..\include\ACCSharedMemory.h"
#include "..\include\SimDisplayProtocol.h"

enum mapAcpmf_action {
	MAPACPMF_CREATE,
	MAPACPMF_OPEN_EXISTING
};

const wchar_t acpmf_physics[] = L"Local\\acpmf_physics";
const wchar_t acpmf_graphics[] = L"Local\\acpmf_graphics";
const wchar_t acpmf_static[] = L"Local\\acpmf_static";

int mapAcpmf(enum mapAcpmf_action action, struct ACCPhysics **phy, struct ACCGraphics **gra, struct ACCStatic **sta)
{
	int err = 0;

	HANDLE phyMap, graMap, staMap;

	if (MAPACPMF_OPEN_EXISTING == action) {
		while (!(phyMap = OpenFileMapping(FILE_MAP_READ, FALSE, acpmf_physics))) {
			fprintf(stderr, "Waiting: open file mapping for ACCPhysics.\n");
			Sleep(1000);
			// TODO: don't try forever: exit after 5 minutes?
		}
		graMap = OpenFileMapping(FILE_MAP_READ, FALSE, acpmf_graphics); 
		staMap = OpenFileMapping(FILE_MAP_READ, FALSE, acpmf_static);
	} else {
		phyMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(struct ACCPhysics), acpmf_physics);
		graMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(struct ACCGraphics), acpmf_graphics);
		staMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(struct ACCStatic), acpmf_static);
	}
	if (!phyMap) {
		fprintf(stderr, "Error: %d: file mapping ACCPhysics.\n", action);
		err = 1;
	}
	if (!graMap) {
		fprintf(stderr, "Error: %d: file mapping ACCGraphics.\n", action);
		err = 1;
	}
	if (!staMap) {
		fprintf(stderr, "Error: %d: file mapping ACCStatic.\n", action);
		err = 1;
	}
	if (err) {
		return err;
	}

	*phy = (struct ACCPhysics *) MapViewOfFile(phyMap, (action == MAPACPMF_OPEN_EXISTING) ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0);
	if (!phy) {
		fprintf(stderr, "Error: mapping view ACCPhysics.\n");
		err = 1;
	}
	*gra = (struct ACCGraphics *) MapViewOfFile(graMap, (action == MAPACPMF_OPEN_EXISTING) ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0);
	if (!gra) {
		fprintf(stderr, "Error: mapping view ACCGraphics.\n");
		err = 1;
	}
	*sta = (struct ACCStatic *) MapViewOfFile(staMap, (action == MAPACPMF_OPEN_EXISTING) ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0);
	if (!sta) {
		err = 1;
		fprintf(stderr, "Error: mapping view ACCStatic.\n");
	}

	return err;
}

float lookupBBOffset(wchar_t *carModel)
{
	static struct DictElem {
		float bbOffset;
		wchar_t* carModel;
	} dict[] = {
		{ -70.0f,	L"amr_v12_vantage_gt3" },
		{ -70.0f,	L"amr_v8_vantage_gt3" },
		{ -140.0f,	L"audi_r8_lms" },
		{ -140.0f,	L"audi_r8_lms_evo" },
		{ -70.0f,	L"bentley_continental_gt3_2016" },
		{ -70.0f,	L"bentley_continental_gt3_2018" },
		{ -150.0f,	L"bmw_m6_gt3" },
		{ -70.0f,	L"jaguar_g3" },
		{ -170.0f,	L"ferrari_488_gt3" },
		{ -140.0f,	L"honda_nsx_gt3" },
		{ -140.0f,	L"honda_nsx_gt3_evo" },
		{ -140.0f,	L"lamborghini_gallardo_rex" },
		{ -150.0f,	L"lamborghini_huracan_gt3" },
		{ -140.0f,	L"lamborghini_huracan_gt3_evo" },
		{ -140.0f,	L"lamborghini_huracan_st" },
		{ -140.0f,	L"lexus_rc_f_gt3" },
		{ -170.0f,	L"mclaren_650s_gt3" },
		{ -170.0f,	L"mclaren_720s_gt3" },
		{ -150.0f,	L"mercedes_amg_gt3" },
		{ -150.0f,	L"nissan_gt_r_gt3_2017" },
		{ -150.0f,	L"nissan_gt_r_gt3_2018" },
		{ -60.0f,	L"porsche_991_gt3_r" },
		{ -150.0f,	L"porsche_991ii_gt3_cup" },
		{ -210.0f,	L"porsche_991ii_gt3_r" },		
	};
	for (int i = 0; i < (sizeof(dict) / sizeof (struct DictElem)); ++i) {
		if (!wcscmp(dict[i].carModel, carModel)) {
			return dict[i].bbOffset;
		}
	}
	return 0.0;
}

int doSend(int argc, const wchar_t *argv[])
{
	const wchar_t *comPortName = argv[0];

	if (!comPortName) {
		fprintf(stderr, "usage: send <serial_port>\n\n");
		fprintf(stderr, "<serial_port> is the name of the serial port the device is attached to.\n");
		return 1;
	}

	struct ACCPhysics *phy;
	struct ACCGraphics *gra;
	struct ACCStatic *sta;

	if (mapAcpmf(MAPACPMF_OPEN_EXISTING, &phy, &gra, &sta)) {
		return 1;
	}

	HANDLE comPort = CreateFile(comPortName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (comPort == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error: open serial port %S\n", comPortName);
		return 1;
	}

	DCB comPortDCB;

	if (!GetCommState(comPort, &comPortDCB)) { //retreives  the current settings
		fprintf(stderr, "Error: get COM state.");
		return 1;
	}

	comPortDCB.BaudRate = CBR_9600;
	comPortDCB.ByteSize = 8;
	comPortDCB.StopBits = ONESTOPBIT;
	comPortDCB.Parity = NOPARITY;

	if (!SetCommState(comPort, &comPortDCB)) {
		fprintf(stderr, "Error: set COM state.");
		return 1;
	}

	HANDLE sendTimer = CreateWaitableTimer(NULL, FALSE, NULL);
	if (NULL == sendTimer) {
		fprintf(stderr, "Error: create timer.\n");
		return 1;
	}
	LARGE_INTEGER dueTime;
	dueTime.QuadPart = -400000LL; // 40ms == 25Hz
	if (!SetWaitableTimer(sendTimer, &dueTime, 40, NULL, NULL, FALSE)) {
		printf("Error: SetWaitableTimer: %d\n", GetLastError());
		return 1;
	}

	struct SimDisplayPacket packet;
	float bbOffset = 0.0f;
	int prevStatus = ACC_OFF; // TODO: we could use packet-> status as the previous status...
	while (WaitForSingleObject(sendTimer, INFINITE) == WAIT_OBJECT_0) {
		if (ACC_LIVE != gra->status && prevStatus == gra->status) continue;
		if (gra->status != prevStatus && ACC_LIVE == gra->status ) {
			bbOffset = lookupBBOffset(sta->carModel);
		}
		packet.status = prevStatus = gra->status;
		packet.rpm = phy->rpms;
		packet.maxrpm = sta->maxRpm;
		packet.pitlimiter = phy->pitLimiterOn;
		packet.gear = phy->gear; // 0 = Reverse, 1 = Neutra, 2 = 1st, 3 = 2nd, ..., 7 = 6th.
		packet.tc = gra->TC;
		packet.tcc = gra->TCCut;
		packet.tcaction = (uint8_t)phy->tc;
		packet.abs = gra->ABS;
		packet.absaction = (uint8_t)phy->abs;
		packet.bb = phy->brakeBias ? (uint16_t)(phy->brakeBias * 1000.0f + bbOffset) : 0);
		packet.remlaps = (uint8_t)gra->fuelEstimatedLaps; // Only full laps are useful to the driver.
		packet.map = gra->EngineMap + 1;
		packet.airt = (uint8_t)(phy->airTemp+0.5f); // would be nice to track temps going down/up
		packet.roadt = (uint8_t)(phy->roadTemp+0.5f);

		DWORD bytesWritten;
		WriteFile(comPort, &packet, sizeof(packet), &bytesWritten, NULL);
		// FIXME: validate bytes written and return status.
		// TODO: if error, stop writing completely or pause?
		// TODO: will the serial port be open?
		// TODO: what if I disconnect and reconnect the arduino?
	}
	fprintf(stderr, "Error: WaitForSingleObject: %d\n", GetLastError());
	return 1;
}

int doSave(void)
{
	struct ACCPhysics *phy;
	struct ACCGraphics *gra;
	struct ACCStatic *sta;

	if (mapAcpmf(MAPACPMF_OPEN_EXISTING, &phy, &gra, &sta)) {
		return 1;
	}

	HANDLE dumpFile = CreateFile(TEXT("accdump.bin"), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == dumpFile) {
		fprintf(stderr, "Error: create accdump.bin\n");
		return 1;
	}
	HANDLE dumpTimer = CreateWaitableTimer(NULL, FALSE, NULL);
	if (NULL == dumpTimer) {
		fprintf(stderr, "Error: create timer.\n");
		return 1;
	}
	LARGE_INTEGER dueTime;
	dueTime.QuadPart = -200000LL; // 20ms == 50Hz
	if (!SetWaitableTimer(dumpTimer, &dueTime, 20, NULL, NULL, FALSE)) {
		printf("Error: SetWaitableTimer: %d\n", GetLastError());
		return 1;
	}
	DWORD bytesWritten;
	while (WaitForSingleObject(dumpTimer, INFINITE) == WAIT_OBJECT_0) {
		WriteFile(dumpFile, phy, sizeof(*phy), &bytesWritten, NULL);
		WriteFile(dumpFile, gra, sizeof(*gra), &bytesWritten, NULL);
		WriteFile(dumpFile, sta, sizeof(*sta), &bytesWritten, NULL);
	}
	fprintf(stderr, "Error: WaitForSingleObject: %d\n", GetLastError());
	return 1;
}

int doCsv(void)
{
	fprintf(stderr, "Read accdump.bin contents and write into accdump.csv\n");
	HANDLE csvFile = CreateFile(TEXT("accdump.csv"), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == csvFile) {
		fprintf(stderr, "Error: create accdump.csv: %d\n", GetLastError());
		return 1;
	}
	HANDLE binFile = CreateFile(TEXT("accdump.bin"), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == binFile) {
		fprintf(stderr, "Error: open accdump.bin: %d\n", GetLastError());
		return 1;
	}
	int maxCsvRecord = 8192;
	char *csvRecord = malloc(maxCsvRecord);
	if (!csvRecord) ExitProcess(1);
	DWORD writtenBytes;
	if (!WriteFile(csvFile, csvRecord,
			snprintf(csvRecord, maxCsvRecord,
					"status,rpm,maxrpm,pitlimiteron,gear,"
					"tc,tccut,tcaction,itcaction,abs,absaction,iabsaction,"
					"bb,ibb,fuellaps,map,airt,roadt\n"),
			&writtenBytes, NULL)) {
		fprintf(stderr, "Error: write CSV header: %d\n", GetLastError());
		return 1;
	}
	int binBufferSize = sizeof(struct ACCPhysics) + sizeof(struct ACCGraphics) + sizeof(struct ACCStatic);
	char *binBuffer = malloc(binBufferSize);
	if (!binBuffer) ExitProcess(1);
	struct ACCPhysics *phy = (struct ACCPhysics *)binBuffer;
	struct ACCGraphics *gra = (struct ACCGraphics *)(binBuffer + sizeof(struct ACCPhysics));
	struct ACCStatic *sta = (struct ACCStatic *)(binBuffer + sizeof(struct ACCPhysics) + sizeof(struct ACCGraphics));
	DWORD readBytes;
	while (ReadFile(binFile, binBuffer, binBufferSize, &readBytes, NULL) && readBytes == binBufferSize) {
		if (!WriteFile(csvFile, csvRecord,
				snprintf(csvRecord, maxCsvRecord,
					"%d,%d,%d,%d,%d,"
					"%d,%d,%f,%u,%d,%f,%u,"
					"%f,%u,%f,%d,%f,%f\n",
					gra->status, phy->rpms, sta->maxRpm, phy->pitLimiterOn, phy->gear,
					gra->TC, gra->TCCut, phy->tc, (uint8_t)phy->tc, gra->ABS, phy->abs, (uint8_t)phy->abs,
					phy->brakeBias, (uint16_t)(phy->brakeBias * 1000.0f + lookupBBOffset(sta->carModel)), gra->fuelEstimatedLaps, gra->EngineMap, phy->airTemp, phy->roadTemp),
				&writtenBytes, NULL)) {
			fprintf(stderr, "Error: write CSV record: %d\n", GetLastError());
			return 1;
		}
	}
	return 0;
}

int doReplay(int argc, const wchar_t *argv[])
{
	HANDLE stdinh;
	HANDLE *input;

	if (!argc) {
		stdinh = GetStdHandle(STD_INPUT_HANDLE);
		input = &stdinh;
		if (*input == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "Error: GetStdHandle STD_INPUT_HANDLE: %d\n", GetLastError());
			return 1;
		}
		argc = 1;
		argv[0] = L"stdin";
	} else {
		input = malloc(argc * sizeof(HANDLE));
		if (!input) ExitProcess(1);
		for (int i = 0; i < argc; ++i) {
			input[i] = CreateFile(argv[i], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (INVALID_HANDLE_VALUE == input[i]) {
				fprintf(stderr, "Error: open %S: %d\n", argv[i], GetLastError());
				return 1;
			}
		}
	}
	
	struct ACCPhysics *phy;
	struct ACCGraphics *gra;
	struct ACCStatic *sta;

	if (mapAcpmf(MAPACPMF_CREATE, &phy, &gra, &sta)) {
		return 1;
	}

	HANDLE replayTimer = CreateWaitableTimer(NULL, FALSE, NULL);
	if (NULL == replayTimer) {
		fprintf(stderr, "Error: create timer.\n");
		return 1;
	}

	for (int i = 0; i < argc; ++i) {
		LARGE_INTEGER dueTime;
		dueTime.QuadPart = -200000LL; // 20ms == 50Hz
		if (!SetWaitableTimer(replayTimer, &dueTime, 20, NULL, NULL, FALSE)) {
			printf("Error: SetWaitableTimer: %d\n", GetLastError());
			return 1;
		}
		DWORD phyBytesRead;
		DWORD graBytesRead;
		DWORD staBytesRead;
		do {
			if (WaitForSingleObject(replayTimer, INFINITE) != WAIT_OBJECT_0) {
				fprintf(stderr, "Error: WaitForSingleObject: %d\n", GetLastError());
				return 1;
			}
			if (!ReadFile(input[i], phy, sizeof(*phy), &phyBytesRead, NULL)
				|| !ReadFile(input[i], gra, sizeof(*gra), &graBytesRead, NULL)
				|| !ReadFile(input[i], sta, sizeof(*sta), &staBytesRead, NULL)) {
				fprintf(stderr, "Error: ReadFile %S: %d\n", argv[i], GetLastError());
				return 1;
			}
		} while (phyBytesRead || graBytesRead || staBytesRead);
	}

	return 0;
}

int doHelp(void)
{
	puts(
"usage: <command> [<args>]\n"
"\n"
"Commands are:\n"
"  send   transmit data to device over serial connection\n"
"  save   saves a gaming session to file\n"
"  csv    convert data from a saved session into a CSV format file\n"
"  replay reads a saved session and populates shared memory\n"
);
	return 1;
}

int wmain(int argc, const wchar_t *argv[])
{
	enum { SEND, SAVE, CSV, REPLAY, HELP } action = HELP;

	if (argc > 1) {
		if (!wcscmp(argv[1], L"send")) {
			action = SEND;
		} else if (!wcscmp(argv[1], L"save")) {
			action = SAVE;
		} else if (!wcscmp(argv[1], L"csv")) {
			action = CSV;
		} else if (!wcscmp(argv[1], L"replay")) {
			action = REPLAY;
		} else {
			action = HELP;
		}
	}

	switch (action) {
	case HELP:
		return doHelp();
	case SEND:
		return doSend(argc-2, argv+2);
	case SAVE:
		return doSave();
	case CSV:
		return doCsv();
	case REPLAY:
		return doReplay(argc-2, argv+2);
	}

	return 0;
}
