// ndi-server.cpp : This file contains the 'main' function. Program execution begins and ends there.
#include <windows.h>
#include <stdio.h>
#include <atomic>
#include <thread>
#include "..\src\ndi-shared.h"

// Include NDI SDK headers from lib/ndi
#include "..\lib\ndi\Processing.NDI.Lib.h"

// Helper to load and initialize the NDI runtime. Returns pointer to NDIlib_v6 or nullptr on failure.
static const NDIlib_v6 *load_ndi_runtime()
{
	// Use WinAPI to load the NDI runtime DLL directly so we don't depend on Qt here.
	HMODULE hLib = NULL;

	// First, check NDILIB_REDIST_FOLDER environment variable for a redistributable folder path
	char redistFolder[MAX_PATH] = {0};
	DWORD ret = GetEnvironmentVariableA("NDI_RUNTIME_DIR_V6", redistFolder, MAX_PATH);
	if (ret >0 && ret < MAX_PATH) {
		// Build full path: <folder>\NDILIB_LIBRARY_NAME
		char fullPath[MAX_PATH] = {0};
		snprintf(fullPath, MAX_PATH, "%s\\%s", redistFolder, NDILIB_LIBRARY_NAME);
		hLib = LoadLibraryA(fullPath);
		if (!hLib) {
			printf("Failed to load NDI runtime from '%s' (LoadLibraryA error=%lu). Will try system path.\n", fullPath,
			 GetLastError());
		} else {
			printf("Loaded NDI runtime from '%s'\n", fullPath);
		}
	}

	// Fallback: try loading by library name (system path)
	if (!hLib) {
		hLib = LoadLibraryA(NDILIB_LIBRARY_NAME);
		if (!hLib) {
			printf("Failed to load NDI runtime '%s' (LoadLibraryA error=%lu)\n", NDILIB_LIBRARY_NAME,
			 GetLastError());
			return nullptr;
		}
	}

	typedef const NDIlib_v6 *(*NDIlib_v6_load_t)(void);
	auto loader = reinterpret_cast<NDIlib_v6_load_t>(GetProcAddress(hLib, "NDIlib_v6_load"));
	if (!loader) {
		printf("NDIlib_v6_load not found in runtime '%s'\n", NDILIB_LIBRARY_NAME);
		// Keep library loaded but return null to indicate failure to resolve loader.
		return nullptr;
	}

	const NDIlib_v6 *ndi = loader();
	if (!ndi) {
		printf("NDIlib_v6_load returned null\n");
		return nullptr;
	}

	if (!ndi->initialize()) {
		printf("NDI runtime initialize() failed\n");
		// We still return the pointer; caller can decide.
	} else {
		printf("NDI runtime initialized: %s\n", ndi->version());
	}

	// Intentionally do not FreeLibrary(hLib) so runtime remains loaded for process lifetime.
	return ndi;
}

//    globals shared between the main loop and the monitor thread               
static std::atomic<bool> g_running{true};
static HANDLE g_hShutdownEvt = nullptr; // local (unsignalled) event

//                                                                              
//  Background thread: watches the client process.
//  When the client exits (for any reason), we signal g_hShutdownEvt so that
//  the main loop wakes up and exits cleanly.
//                                                                              
static void ClientMonitorThread(DWORD clientPid)
{
	HANDLE hClient = OpenProcess(SYNCHRONIZE, FALSE, clientPid);
	if (!hClient) {
		fprintf(stderr, "[Server] Cannot open client PID %u (%u) � will not auto-exit.\n", clientPid,
			GetLastError());
		return;
	}

	// Block until the client process terminates for any reason.
	WaitForSingleObject(hClient, INFINITE);
	CloseHandle(hClient);

	printf("[Server] Client process (PID %u) has exited � signalling shutdown.\n", clientPid);
	g_running = false;
	SetEvent(g_hShutdownEvt);
}

int main(int argc, char *argv[])
{
	if (false) {
	//if (!IsDebuggerPresent()) {
		// Avoid using DebugBreak() unconditionally - if JIT dialog is dismissed the process can be
		// terminated. Instead, print PID and wait for a debugger to attach.
		DWORD pid = GetCurrentProcessId();

		// Ensure we have a console to print to when launched from a GUI process (like OBS)
		if (!GetConsoleWindow()) {
			AllocConsole();
			FILE *dummy;
			freopen_s(&dummy, "CONOUT$", "w", stdout);
			freopen_s(&dummy, "CONOUT$", "w", stderr);
		}

		printf("ndi-server PID=%lu - waiting for debugger to attach...\n", (unsigned long)pid);
		fflush(stdout);

		// Wait until a debugger is attached. This loop keeps the process alive so you can attach
		// from Visual Studio (Debug -> Attach to Process) using the printed PID.
		while (!IsDebuggerPresent()) {
			Sleep(250);
		}

		printf("Debugger attached; breaking into debugger.\n");
		fflush(stdout);
		DebugBreak();
	}

	NDIlib_recv_instance_t ndi_receiver = nullptr;
	NDIlib_video_frame_v2_t video_frame;
	NDIlib_metadata_frame_t metadata_frame;
	NDIlib_audio_frame_v3_t audio_frame;

	// Attempt to load NDI runtime early so errors are visible in the helper.
	const NDIlib_v6 *ndi = load_ndi_runtime();
	if (!ndi) {
		printf("Warning: Could not load or initialize NDI runtime. ndi-server may not function correctly.\n");
		// continue anyway the shared-memory signalling portion can still be used without NDI.
	}

	// --- Validate and convert argument ---
	if (argc < 2) {
		printf("Usage: ndi-server.exe <shared_memory_name>\n");
		return 1;
	}

	WCHAR wclientid[256] = {0};
	WCHAR connectionName[256] = {0};
	WCHAR requestShmName[256] = {0};
	WCHAR responseShmName[256] = {0};
	WCHAR commandEventName[256] = {0};
	WCHAR readyEventName[256] = {0};
	WCHAR responseEventName[256] = {0};

	// argv[1] is the shared memory name (narrow). Convert to wide.
	MultiByteToWideChar(CP_ACP, 0, argv[1], -1, wclientid, 256);
	swprintf_s(connectionName, 256, L"%s%s", NDI_MEM_NAME_PREFIX, wclientid);
	swprintf_s(requestShmName, 256, L"%s%s", connectionName, NDI_REQUEST_SHM_SUFFIX);
	swprintf_s(responseShmName, 256, L"%s%s", connectionName, NDI_RESPONSE_SHM_SUFFIX);
	swprintf_s(commandEventName, 256, L"%s%s", connectionName, NDI_COMMAND_EVENT_SUFFIX);
	swprintf_s(readyEventName, 256, L"%s%s", connectionName, NDI_READY_EVENT_SUFFIX);
	swprintf_s(responseEventName, 256, L"%s%s", connectionName, NDI_RESPONSE_EVENT_SUFFIX);

 //    Boost process and thread priority                                     
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	printf("[Server] Starting up...\n");

	//    Open shared-memory regions (created by the client)                    
	HANDLE hShmReq = OpenFileMapping(FILE_MAP_READ, FALSE, requestShmName);
	if (!hShmReq)
		fatal("OpenFileMapping(request)");

	HANDLE hShmRsp = OpenFileMapping(FILE_MAP_WRITE, FALSE, responseShmName);
	if (!hShmRsp)
		fatal("OpenFileMapping(response)");

	//    Open named events                                                     
	HANDLE hEvtCmd = OpenEvent(SYNCHRONIZE, FALSE, commandEventName);
	if (!hEvtCmd)
		fatal("OpenEvent(cmd)");

	HANDLE hEvtRsp = OpenEvent(EVENT_MODIFY_STATE, FALSE, responseEventName);
	if (!hEvtRsp)
		fatal("OpenEvent(rsp)");

	HANDLE hEvtReady = OpenEvent(EVENT_MODIFY_STATE, FALSE, readyEventName);
	if (!hEvtReady)
		fatal("OpenEvent(ready)");

	// Map views
	const RequestBlock *pReq =
		static_cast<const RequestBlock *>(MapViewOfFile(hShmReq, FILE_MAP_READ, 0, 0, sizeof(RequestBlock)));
	if (!pReq)
		fatal("MapViewOfFile(request)");

	ResponseBlock *pRsp = static_cast<ResponseBlock *>(MapViewOfFile(hShmRsp, FILE_MAP_WRITE, 0, 0, sizeof(ResponseBlock)));
	if (!pRsp)
		fatal("MapViewOfFile(response)");

	//    Pre-fault the 48 MB response buffer                                   
	PrefaultRegion(pRsp, sizeof(ResponseBlock));

	//    Local (unnamed) shutdown event for the monitor thread                 
	g_hShutdownEvt = CreateEvent(nullptr, TRUE /*manual*/, FALSE, nullptr);
	if (!g_hShutdownEvt)
		fatal("CreateEvent(shutdown)");

	//    Start the client-monitor thread   
	// client PID is set by the client at startup and doesn't change, so it's safe to read it here before the loop starts.                                   
	DWORD clientPid = pReq->client_pid;
	std::thread monitor(ClientMonitorThread, clientPid);
	monitor.detach();

	//    Signal the client that we are ready                                   
	SetEvent(hEvtReady);
	printf("[Server] Ready and listening (client PID %u).\n", clientPid);

	//    Main request/response loop                                             
	// Wait on TWO handles simultaneously so we react instantly to either:
	//   [0] = IPC_EVT_CMD      a new command from the client
	//   [1] = g_hShutdownEvt   client process died
	HANDLE waitSet[2] = {hEvtCmd, g_hShutdownEvt};

	LARGE_INTEGER freq, t0, t1;
	QueryPerformanceFrequency(&freq);
	uint64_t frameCount = 0;
	NDIlib_recv_create_v3_t recv_desc{};
	std::vector<std::string> strings;

	while (g_running) {
		DWORD w = WaitForMultipleObjects(2, waitSet, FALSE, INFINITE);

		if (w == WAIT_OBJECT_0 + 1 || !g_running) {
			// Shutdown event or flag set by monitor thread
			printf("[Server] Shutdown event received.\n");
			break;
		}

		if (w != WAIT_OBJECT_0) {
			fprintf(stderr, "[Server] WaitForMultipleObjects error %u\n", GetLastError());
			break;
		}

		//    hEvtCmd was signalled (auto-reset already cleared it)             
		QueryPerformanceCounter(&t0);

		switch (pReq->command)
		{
		case NDI_CREATE_RECEIVER:
			deserialize_recv_desc(&pReq->payload, sizeof(pReq->payload), recv_desc, strings);
			if (ndi_receiver != nullptr) {
				ndi->recv_destroy(ndi_receiver);
				ndi_receiver = nullptr;
			}
			ndi_receiver = ndi->recv_create_v3(&recv_desc);

			printf("Received ndi name: '%s'\n", recv_desc.source_to_connect_to.p_ndi_name);
			if (ndi_receiver) {
				printf("NDI Receiver created successfully!\n");
			} else {
				printf("Failed to create NDI Receiver.\n");
			}
			break;
		case NDI_CAPTURE_FRAME: {
			NDIlib_frame_type_e frame_received =
				ndi->recv_capture_v3(ndi_receiver, &video_frame, &audio_frame, nullptr, 100);
			switch (frame_received) {
			case NDIlib_frame_type_video:
				serialize_frame(frame_received, (const void *)&video_frame, &pRsp->payload,
						sizeof(pRsp->payload));
				ndi->recv_free_video_v2(ndi_receiver, &video_frame);
				break;
			case NDIlib_frame_type_audio:
				serialize_frame(frame_received, (const void *)&audio_frame, &pRsp->payload,
						sizeof(pRsp->payload));
				ndi->recv_free_audio_v3(ndi_receiver, &audio_frame);
				break;
			default:				
				break;
			}					
			break;
		}
		case NDI_SHUTDOWN:
			printf("Received shutdown command.\n");
			g_running = false;
			break;
		default:
			printf("Received unknown command code: %u\n", pReq->command);
		}

		//    Respond                                                           
		SetEvent(hEvtRsp);

		QueryPerformanceCounter(&t1);
		double ms = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)freq.QuadPart;
		++frameCount;

		if (frameCount % 60 == 0)
			printf("[Server] frame %-8llu dispatch=%.3f ms\n",
			       (unsigned long long)frameCount,  ms);
	}

	//    Cleanup                                                                
	printf("[Server] Cleaning up and exiting.\n");

	if (pReq) UnmapViewOfFile(const_cast<RequestBlock *>(pReq));
	if (pRsp) UnmapViewOfFile(pRsp);
	CloseHandle(hShmReq);
	CloseHandle(hShmRsp);
	CloseHandle(hEvtCmd);
	CloseHandle(hEvtRsp);
	if (hEvtReady) CloseHandle(hEvtReady);
	if (g_hShutdownEvt) CloseHandle(g_hShutdownEvt);

	return 0;
}
