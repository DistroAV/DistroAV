// ndi-server.cpp : This file contains the 'main' function. Program execution begins and ends there.
#include <windows.h>
#include <stdio.h>
#include <atomic>
#include <thread>
#include "..\src\ndi-shared.h"

// Container for advanced NDI function pointers (receiver-focused)
struct NDIAdvancedlib_t {
	NDIlib_frame_type_e (*recv_capture_v3)(NDIlib_recv_instance_t p_instance,
					 NDIlib_video_frame_v2_t* p_video_data,
					 NDIlib_audio_frame_v3_t* p_audio_data,
					 NDIlib_metadata_frame_t* p_metadata,
					 uint32_t timeout_in_ms);

	// Add recv_create_v3 and framesync_create
	NDIlib_recv_instance_t (*recv_create_v3)(const NDIlib_recv_create_v3_t* p_create_settings);
	NDIlib_framesync_instance_t (*framesync_create)(NDIlib_recv_instance_t p_receiver);

	void (*recv_free_video_v2)(NDIlib_recv_instance_t p_instance, const NDIlib_video_frame_v2_t* p_video_data);
	void (*recv_free_audio_v3)(NDIlib_recv_instance_t p_instance, const NDIlib_audio_frame_v3_t* p_audio_data);

	int (*recv_get_no_connections)(NDIlib_recv_instance_t p_instance);

	void (*framesync_capture_audio_v2)(NDIlib_framesync_instance_t p_instance, NDIlib_audio_frame_v3_t* p_audio_data,
					   int sample_rate, int no_channels, int no_samples);
	void (*framesync_free_audio_v2)(NDIlib_framesync_instance_t p_instance, NDIlib_audio_frame_v3_t* p_audio_data);

	void (*framesync_capture_video)(NDIlib_framesync_instance_t p_instance, NDIlib_video_frame_v2_t* p_video_data,
					NDIlib_frame_format_type_e field_type);
	void (*framesync_free_video)(NDIlib_framesync_instance_t p_instance, NDIlib_video_frame_v2_t* p_video_data);

	bool (*recv_send_metadata)(NDIlib_recv_instance_t p_instance, const NDIlib_metadata_frame_t* p_metadata);

	void (*framesync_destroy)(NDIlib_framesync_instance_t p_instance);
	void (*recv_destroy)(NDIlib_recv_instance_t p_instance);

	void (*recv_connect)(NDIlib_recv_instance_t p_instance, const NDIlib_source_t* p_src);

	bool (*initialize)(void);
	void (*destroy)(void);

	// Custom allocator setters (Processing.NDI.Advanced.h)
	void (*recv_set_video_allocator)(NDIlib_recv_instance_t p_instance, void* p_opaque,
					bool (*p_allocator)(void* p_opaque, NDIlib_video_frame_v2_t* p_video_data),
					bool (*p_deallocator)(void* p_opaque, const NDIlib_video_frame_v2_t* p_video_data));

	void (*recv_set_audio_allocator)(NDIlib_recv_instance_t p_instance, void* p_opaque,
					bool (*p_allocator)(void* p_opaque, NDIlib_audio_frame_v3_t* p_audio_data),
					bool (*p_deallocator)(void* p_opaque, const NDIlib_audio_frame_v3_t* p_audio_data));
};

// Global pointer accessible by other modules: call like NDIAdvlib->recv_capture_v3(...)
NDIAdvancedlib_t *NDIAdvlib = nullptr;
#ifdef NDILIB_LIBRARY_NAME
#undef NDILIB_LIBRARY_NAME
#endif
#define NDILIB_LIBRARY_NAME "Processing.NDI.Lib.Advanced.x64.dll"
NDIAdvancedlib_t *load_advanced_ndilib()
{
	NDIAdvancedlib_t *ndi_advanced_lib = nullptr;

	// Use WinAPI to load the Processing NDI library directly so we don't depend on Qt here.
	HMODULE hLib = NULL;

	// First, check NDILIB_REDIST_FOLDER environment variable for a redistributable folder path
	char redistFolder[MAX_PATH] = {0};
	DWORD ret = GetEnvironmentVariableA("NDI_RUNTIME_DIR_V6", redistFolder, MAX_PATH);
	if (ret >0 && ret < MAX_PATH) {
		char fullPath[MAX_PATH] = {0};
		snprintf(fullPath, MAX_PATH, "%s\\%s", redistFolder, NDILIB_LIBRARY_NAME);
		hLib = LoadLibraryA(fullPath);
		if (!hLib) {
			printf("Failed to load advanced NDI library from '%s' (LoadLibraryA error=%lu). Will try system path.\n",
			 fullPath, GetLastError());
		} else {
			printf("Loaded advanced NDI library from '%s'\n", fullPath);
		}
	}

	// Fallback: try loading by library name (system path)
	if (!hLib) {
		hLib = LoadLibraryA(NDILIB_LIBRARY_NAME);
		if (!hLib) {
			printf("Failed to load advanced NDI library '%s' (LoadLibraryA error=%lu)\n", NDILIB_LIBRARY_NAME,
			 GetLastError());
			return nullptr;
		}
	}

	// Resolve receiver & framesync related functions from the DLL
	auto resolved_recv_capture_v3 = reinterpret_cast<NDIlib_frame_type_e (*)(NDIlib_recv_instance_t, NDIlib_video_frame_v2_t*, NDIlib_audio_frame_v3_t*, NDIlib_metadata_frame_t*, uint32_t)>(
		GetProcAddress(hLib, "NDIlib_recv_capture_v3"));

	// Resolve recv_create_v3 and framesync_create
	auto resolved_recv_create_v3 = reinterpret_cast<NDIlib_recv_instance_t (*)(const NDIlib_recv_create_v3_t*)>(
		GetProcAddress(hLib, "NDIlib_recv_create_v3"));
	
	auto resolved_framesync_create = reinterpret_cast<NDIlib_framesync_instance_t (*)(NDIlib_recv_instance_t)>(
		GetProcAddress(hLib, "NDIlib_framesync_create"));

	auto resolved_recv_free_video_v2 = reinterpret_cast<void (*)(NDIlib_recv_instance_t, const NDIlib_video_frame_v2_t*)>(
		GetProcAddress(hLib, "NDIlib_recv_free_video_v2"));

	auto resolved_recv_free_audio_v3 = reinterpret_cast<void (*)(NDIlib_recv_instance_t, const NDIlib_audio_frame_v3_t*)>(
		GetProcAddress(hLib, "NDIlib_recv_free_audio_v3"));

	auto resolved_recv_get_no_connections = reinterpret_cast<int (*)(NDIlib_recv_instance_t)>(
		GetProcAddress(hLib, "NDIlib_recv_get_no_connections"));

	auto resolved_framesync_capture_audio_v2 = reinterpret_cast<void (*)(NDIlib_framesync_instance_t, NDIlib_audio_frame_v3_t*, int, int, int)>(
		GetProcAddress(hLib, "NDIlib_framesync_capture_audio_v2"));

	auto resolved_framesync_free_audio_v2 = reinterpret_cast<void (*)(NDIlib_framesync_instance_t, NDIlib_audio_frame_v3_t*)>(
		GetProcAddress(hLib, "NDIlib_framesync_free_audio_v2"));

	auto resolved_framesync_capture_video = reinterpret_cast<void (*)(NDIlib_framesync_instance_t, NDIlib_video_frame_v2_t*, NDIlib_frame_format_type_e)>(
		GetProcAddress(hLib, "NDIlib_framesync_capture_video"));

	auto resolved_framesync_free_video = reinterpret_cast<void (*)(NDIlib_framesync_instance_t, NDIlib_video_frame_v2_t*)>(
		GetProcAddress(hLib, "NDIlib_framesync_free_video"));

	auto resolved_recv_send_metadata = reinterpret_cast<bool (*)(NDIlib_recv_instance_t, const NDIlib_metadata_frame_t*)>(
		GetProcAddress(hLib, "NDIlib_recv_send_metadata"));

	auto resolved_framesync_destroy = reinterpret_cast<void (*)(NDIlib_framesync_instance_t)>(
		GetProcAddress(hLib, "NDIlib_framesync_destroy"));

	auto resolved_recv_destroy = reinterpret_cast<void (*)(NDIlib_recv_instance_t)>(
		GetProcAddress(hLib, "NDIlib_recv_destroy"));

	// Resolve recv_connect
	auto resolved_recv_connect = reinterpret_cast<void (*)(NDIlib_recv_instance_t, const NDIlib_source_t*)>(
		GetProcAddress(hLib, "NDIlib_recv_connect"));

	auto resolved_initialize = reinterpret_cast<bool (*)(void)>(
		GetProcAddress(hLib, "NDIlib_initialize"));

	auto resolved_destroy = reinterpret_cast<void (*)(void)>(
		GetProcAddress(hLib, "NDIlib_destroy"));

	// Resolve new allocator setters using explicit function pointer types
	auto resolved_recv_set_video_allocator = reinterpret_cast<void (*)(NDIlib_recv_instance_t, void*, bool (*)(void*, NDIlib_video_frame_v2_t*), bool (*)(void*, const NDIlib_video_frame_v2_t*))>(
		GetProcAddress(hLib, "NDIlib_recv_set_video_allocator"));

	auto resolved_recv_set_audio_allocator = reinterpret_cast<void (*)(NDIlib_recv_instance_t, void*, bool (*)(void*, NDIlib_audio_frame_v3_t*), bool (*)(void*, const NDIlib_audio_frame_v3_t*))>(
		GetProcAddress(hLib, "NDIlib_recv_set_audio_allocator"));

	if (resolved_recv_capture_v3 && resolved_recv_free_video_v2 && resolved_recv_free_audio_v3 &&
	    resolved_recv_get_no_connections && resolved_framesync_capture_audio_v2 && resolved_framesync_free_audio_v2 &&
	    resolved_framesync_capture_video && resolved_framesync_free_video && resolved_recv_send_metadata &&
	    resolved_framesync_destroy && resolved_recv_destroy && resolved_initialize && resolved_destroy &&
	    resolved_recv_create_v3 && resolved_framesync_create && resolved_recv_set_video_allocator && resolved_recv_set_audio_allocator && resolved_recv_connect) {

		ndi_advanced_lib = new NDIAdvancedlib_t();
		ndi_advanced_lib->recv_capture_v3 = resolved_recv_capture_v3;
		ndi_advanced_lib->recv_create_v3 = resolved_recv_create_v3;
		ndi_advanced_lib->framesync_create = resolved_framesync_create;
		ndi_advanced_lib->recv_free_video_v2 = resolved_recv_free_video_v2;
		ndi_advanced_lib->recv_free_audio_v3 = resolved_recv_free_audio_v3;
		ndi_advanced_lib->recv_get_no_connections = resolved_recv_get_no_connections;
		ndi_advanced_lib->framesync_capture_audio_v2 = resolved_framesync_capture_audio_v2;
		ndi_advanced_lib->framesync_free_audio_v2 = resolved_framesync_free_audio_v2;
		ndi_advanced_lib->framesync_capture_video = resolved_framesync_capture_video;
		ndi_advanced_lib->framesync_free_video = resolved_framesync_free_video;
		ndi_advanced_lib->recv_send_metadata = resolved_recv_send_metadata;
		ndi_advanced_lib->framesync_destroy = resolved_framesync_destroy;
		ndi_advanced_lib->recv_destroy = resolved_recv_destroy;
		// Assign recv_connect
		ndi_advanced_lib->recv_connect = resolved_recv_connect;
		ndi_advanced_lib->initialize = resolved_initialize;
		ndi_advanced_lib->destroy = resolved_destroy;

		// Set allocator function pointers
		ndi_advanced_lib->recv_set_video_allocator = reinterpret_cast<void (*)(NDIlib_recv_instance_t, void*, bool (*)(void*, NDIlib_video_frame_v2_t*), bool (*)(void*, const NDIlib_video_frame_v2_t*))>(resolved_recv_set_video_allocator);
		ndi_advanced_lib->recv_set_audio_allocator = reinterpret_cast<void (*)(NDIlib_recv_instance_t, void*, bool (*)(void*, NDIlib_audio_frame_v3_t*), bool (*)(void*, const NDIlib_audio_frame_v3_t*))>(resolved_recv_set_audio_allocator);

		// Keep the library loaded intentionally for process lifetime.
		NDIAdvlib = ndi_advanced_lib;
		return ndi_advanced_lib;
	} else {
		printf("Failed to resolve one or more advanced NDI functions in '%s'\n", NDILIB_LIBRARY_NAME);
		FreeLibrary(hLib);
		return nullptr;
	}
}

// Helper to load and initialize the NDI runtime. Returns pointer to NDIlib_v6 or nullptr on failure.
static const NDIlib_v6 *load_ndi_runtime()
{
	// Use WinAPI to load the NDI runtime DLL directly so we don't depend on Qt here.
	HMODULE hLib = NULL;

	// First, check NDILIB_REDIST_FOLDER environment variable for a redistributable folder path
	char redistFolder[MAX_PATH] = {0};
	DWORD ret = GetEnvironmentVariableA("NDI_RUNTIME_DIR_V6", redistFolder, MAX_PATH);
	if (ret > 0 && ret < MAX_PATH) {
		// Build full path: <folder>\NDILIB_LIBRARY_NAME
		char fullPath[MAX_PATH] = {0};
		snprintf(fullPath, MAX_PATH, "%s\\%s", redistFolder, NDILIB_LIBRARY_NAME);
		hLib = LoadLibraryA(fullPath);
		if (!hLib) {
			printf("Failed to load NDI runtime from '%s' (LoadLibraryA error=%lu). Will try system path.\n",
			       fullPath, GetLastError());
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
struct memory_block_t {
	int n_ptrs;   // how many pointers to cycle through in this block (e.g. for double buffering)
	int next_video_ptr; // current pointer index to use for video (the allocator can update this as it cycles through buffers)  
	int next_audio_ptr; // current pointer index to use for audio (the allocator can update this as it cycles through buffers)  
	void *video_ptr;
	size_t video_size;
	void *audio_ptr;
	size_t audio_size;
};

// Assign p_data and line_stride_in_bytes for the given video frame based on its FourCC. Use the shared memory allocated by the client and 
// passed through the p_opaque pointer.
// Returns true on success, false on unsupported FourCC.
bool video_custom_allocator(void *p_opaque, NDIlib_video_frame_v2_t *p_video_data)
{
	auto mem_block = reinterpret_cast<memory_block_t *>(p_opaque);

	switch (p_video_data->FourCC) {
	case NDIlib_FourCC_video_type_UYVY:		
		p_video_data->line_stride_in_bytes = p_video_data->xres * 2;
		break;

	case NDIlib_FourCC_video_type_UYVA:
		p_video_data->line_stride_in_bytes = p_video_data->xres * 2;
		break;

	case NDIlib_FourCC_video_type_P216:
		p_video_data->line_stride_in_bytes = p_video_data->xres * 2 * sizeof(int16_t);
		break;

	case NDIlib_FourCC_video_type_PA16:
		p_video_data->line_stride_in_bytes = p_video_data->xres * 2 * sizeof(int16_t);
		break;

	case NDIlib_FourCC_video_type_BGRA:
	case NDIlib_FourCC_video_type_BGRX:
	case NDIlib_FourCC_video_type_RGBA:
	case NDIlib_FourCC_video_type_RGBX:
		p_video_data->line_stride_in_bytes = p_video_data->xres * 4;
		break;

	default:
		// Error, not a supported FourCC
		printf("Unsupported video FourCC: 0x%08X\n", p_video_data->FourCC);
		p_video_data->line_stride_in_bytes = 0;
		p_video_data->p_data = nullptr;
		return false;
	}

	p_video_data->p_data = (uint8_t *)mem_block->video_ptr;

	// Success
	return true;
}

bool video_custom_deallocator(void *p_opaque, const NDIlib_video_frame_v2_t *p_video_data)
{
	// Success
	return true;
}

bool audio_custom_allocator(void *p_opaque, NDIlib_audio_frame_v3_t *p_audio_data)
{
	auto mem_block = reinterpret_cast<memory_block_t *>(p_opaque);

	// Allocate uncompressed audio
	switch (p_audio_data->FourCC) {
	case NDIlib_FourCC_audio_type_FLTP:
		p_audio_data->channel_stride_in_bytes = sizeof(float) * p_audio_data->no_samples;
		p_audio_data->p_data = (uint8_t *)mem_block->audio_ptr;
		mem_block->next_audio_ptr =
			(mem_block->next_audio_ptr + 1) % mem_block->n_ptrs; // advance pointer index for next allocation
		break;

	default:
		p_audio_data->channel_stride_in_bytes = 0;
		p_audio_data->p_data = nullptr;
		return false;
	}

	// Success
	return true;
}

bool audio_custom_deallocator(void *p_opaque, const NDIlib_audio_frame_v3_t *p_audio_data)
{
	// Success
	return true;
}

int main(int argc, char *argv[])
{
    if (NDI_SERVER_DEBUG && !IsDebuggerPresent()) {
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
	NDIlib_video_frame_v2_t video_frame_test;
	NDIlib_metadata_frame_t metadata_frame;
	NDIlib_audio_frame_v3_t audio_frame;
	NDIlib_audio_frame_v3_t audio_frame_test;
	NDIlib_framesync_instance_t ndi_frame_sync = nullptr;

	// Attempt to load NDI runtime early so errors are visible in the helper.
	const NDIAdvancedlib_t *ndi = load_advanced_ndilib(); // load_ndi_runtime();
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

		// If --log is present on command line, redirect stdout/stderr to a file named "<wclientid>.log"
	bool log_to_file = false;
	for (int i = 2; i < argc; ++i) {
		if (strcmp(argv[i], "--log") == 0) {
			log_to_file = true;
			break;
		}
	}

	if (log_to_file) {
		// Build path: same folder as the running exe, filename = <wclientid>.log
		WCHAR modulePath[MAX_PATH] = {0};
		if (GetModuleFileNameW(NULL, modulePath, MAX_PATH) > 0) {
			// Extract directory
			WCHAR dirPath[MAX_PATH] = {0};
			wcscpy_s(dirPath, modulePath);
			WCHAR *lastSlash = wcsrchr(dirPath, L'\\');
			if (lastSlash) {
				*lastSlash = L'\0';
			} else {
				// fallback to current directory
				wcscpy_s(dirPath, L".");
			}

			WCHAR logPath[MAX_PATH] = {0};
			swprintf_s(logPath, MAX_PATH, L"%s\\%s.log", dirPath, wclientid);

			FILE *dummyOut = nullptr;
			errno_t ferr = _wfreopen_s(&dummyOut, logPath, L"w", stdout);
			if (ferr != 0 || !dummyOut) {
				// Unable to open log file; fall back to console.
				fprintf(stderr, "[Server] Failed to open log file '%S' (errno=%d)\n", logPath,
					(int)ferr);
			} else {
				// Redirect stderr to same file
				FILE *dummyErr = nullptr;
				_wfreopen_s(&dummyErr, logPath, L"w", stderr);
				// Make stdout/stderr unbuffered to ensure logs are flushed immediately.
				setvbuf(stdout, NULL, _IONBF, 0);
				setvbuf(stderr, NULL, _IONBF, 0);
				printf("[Server] Logging enabled; writing to '%S'\n", logPath);
			}
		} else {
			fprintf(stderr, "[Server] Failed to get module path (error=%lu); logging disabled.\n",
				GetLastError());
		}
	}

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

	ResponseBlock *pRsp =
		static_cast<ResponseBlock *>(MapViewOfFile(hShmRsp, FILE_MAP_WRITE, 0, 0, sizeof(ResponseBlock)));
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

	int videores = 0;
	int audiosamples = 0;
	
	memory_block_t *mem_block = (memory_block_t *)std::malloc(sizeof(memory_block_t));

	mem_block->video_ptr = pRsp->payload + VIDEO_FRAME_OFFSET;
	mem_block->video_size = VIDEO_FRAME_MAX_SIZE;
	mem_block->audio_ptr = pRsp->payload + AUDIO_FRAME_OFFSET;
	mem_block->audio_size = AUDIO_FRAME_MAX_SIZE;

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

		switch (pReq->command) {
		case NDI_CREATE_RECEIVER: {
			NDIlib_recv_create_v3_t recv_desc{};
			std::vector<std::string> strings;

			deserialize_recv_desc(&pReq->payload, sizeof(pReq->payload), recv_desc, strings);
			if (ndi_receiver != nullptr) {
				ndi->framesync_destroy(ndi_frame_sync);
				ndi->recv_destroy(ndi_receiver);
				ndi_receiver = nullptr;
			}

			ndi_receiver = ndi->recv_create_v3(&recv_desc);

			printf("Received ndi name: '%s'\n", recv_desc.source_to_connect_to.p_ndi_name);
			if (ndi_receiver) {
				ndi_frame_sync = ndi->framesync_create(ndi_receiver);
				printf("NDI Receiver created successfully!\n");
			} else {
				printf("Failed to create NDI Receiver.\n");
			}
			
			ndi->recv_set_video_allocator(ndi_receiver, (void*)mem_block,
							video_custom_allocator, video_custom_deallocator);
			ndi->recv_set_audio_allocator(ndi_receiver, (void *)mem_block,
							audio_custom_allocator, audio_custom_deallocator);
			printf("Custom allocators set for NDI Receiver.\n");

			// ndi->recv_connect(ndi_receiver, &recv_desc.source_to_connect_to);
			// printf("NDI Receiver connected to source '%s'\n", recv_desc.source_to_connect_to.p_ndi_name);

			break;
		}
		case NDI_CAPTURE_FRAME: {
			if (ndi->recv_get_no_connections(ndi_receiver) == 0) {
				pRsp->payload_type = NDI_NO_FRAME;
				break;
			}
			NDIlib_frame_type_e frame_received =
				ndi->recv_capture_v3(ndi_receiver, &video_frame, &audio_frame, nullptr, 100);
			switch (frame_received) {
			case NDIlib_frame_type_video:
				serialize_frame(frame_received, (const void *)&video_frame, (uint8_t *)&pRsp->payload,
						sizeof(pRsp->payload));
				ndi->recv_free_video_v2(ndi_receiver, &video_frame);
				pRsp->payload_type = NDI_VIDEO_FRAME;
				break;
			case NDIlib_frame_type_audio:
				serialize_frame(frame_received, (const void *)&audio_frame, (uint8_t *)&pRsp->payload,
						sizeof(pRsp->payload));
				ndi->recv_free_audio_v3(ndi_receiver, &audio_frame);
				pRsp->payload_type = NDI_AUDIO_FRAME;
				break;
			default:
				break;
			}
			break;
		}
		case NDI_CAPTURE_FRAME_SYNC: {
			if (ndi->recv_get_no_connections(ndi_receiver) == 0)
			{
				printf("No NDI connections available; skipping frame capture.\n");
				pRsp->payload_type = NDI_NO_FRAME;
				break;
			}
			audio_frame = {};
			ndi->framesync_capture_audio_v2(ndi_frame_sync, &audio_frame,
							0, // "The desired sample rate. 0 to get the source value."
							0, // "The desired channel count. 0 to get the source value."
							1024); // "The desired sample count. 0 to get the source value."
			audiosamples = audio_frame.no_samples;

			// Prepare output buffer pointer/size
			uint8_t *out_buf = pRsp->payload;
			size_t out_capacity = sizeof(pRsp->payload);

			// Serialize audio into the start of the buffer
			size_t audio_size = serialize_frame(NDIlib_frame_type_audio, (const void *)&audio_frame,
							    out_buf, out_capacity);
			ndi->framesync_free_audio_v2(ndi_frame_sync, &audio_frame);

			if (audio_size == 0) {
				// Serialization failed (insufficient space or error) — respond with empty payload
				break;
			}

			// Capture video and serialize it immediately after the audio bytes
			video_frame = {};
			ndi->framesync_capture_video(ndi_frame_sync, &video_frame,
						     NDIlib_frame_format_type_progressive);
			videores = video_frame.xres * video_frame.yres;

			size_t remaining = out_capacity - audio_size;
			size_t video_size = 0;
			if (remaining > 0) {
				video_size = serialize_frame(NDIlib_frame_type_video, (const void *)&video_frame,
							     out_buf + audio_size, remaining);
			}

			ndi->framesync_free_video(ndi_frame_sync, &video_frame);
			pRsp->payload_type = NDI_MULTI_FRAME;

			// verify serialization succeeded and we didn't exceed buffer capacity
			
			NDIlib_frame_type_e frame_received = NDIlib_frame_type_none;
			uint8_t *in_buf = pRsp->payload;
			size_t frame_len = deserialize_frame(in_buf, sizeof(pRsp->payload),
							     frame_received, &video_frame_test, &audio_frame_test);
			if (frame_received != NDIlib_frame_type_audio || audio_frame.no_samples != audio_frame_test.no_samples || audio_frame.timecode != audio_frame_test.timecode) {
				printf("Audio deserialization failed or size mismatch (received type %d, size %zu, expected type %d, size %zu, )\n",
					   frame_received, frame_len, NDIlib_frame_type_audio, audio_size);
				break;
			}
			in_buf += frame_len;
			frame_len = deserialize_frame(in_buf, sizeof(pRsp->payload) - frame_len, frame_received,
					  &video_frame_test, &audio_frame_test);
			if (frame_received != NDIlib_frame_type_video || video_size != frame_len ||
			    video_frame_test.xres != video_frame.xres || video_frame_test.yres != video_frame.yres) {
				printf("Video deserialization failed or resolution mismatch (received type %d, res %dx%d, expected type %d, res %dx%d)\n",
				       frame_received, video_frame_test.xres, video_frame_test.yres,
				       NDIlib_frame_type_video, video_frame.xres, video_frame.yres);
			}
			// printf("Deserialize test: video %dx%d audio %d samples\n", video_frame_test.xres, video_frame_test.yres, audio_frame_test.no_samples);
			break;
		}
		case NDI_SHUTDOWN:
			printf("Received shutdown command.\n");
			g_running = false;
			break;
		case NDI_HARDWARE_ACCELERATION: {
			NDIlib_metadata_frame_t hwAccelMetadata;
			hwAccelMetadata.p_data = (char *)"<ndi_video_codec type=\"hardware\"/>";
			ndi->recv_send_metadata(ndi_receiver, &hwAccelMetadata);
			break;
		}
		default:
			printf("Received unknown command code: %u\n", pReq->command);
		}

		//    Respond
		SetEvent(hEvtRsp);

		QueryPerformanceCounter(&t1);

		++frameCount;

		if (frameCount % 60 == 0) {
			double ms = ((double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart) * 1000.0;
			printf("[Server] frame %-8llu n_audio=%d vres=%d dispatch=%.3f ms\n",
			       (unsigned long long)frameCount, audiosamples, videores, ms);
		}
	}

	//    Cleanup
	printf("[Server] Cleaning up and exiting.\n");

	if (pReq)
		UnmapViewOfFile(const_cast<RequestBlock *>(pReq));
	if (pRsp)
		UnmapViewOfFile(pRsp);
	CloseHandle(hShmReq);
	CloseHandle(hShmRsp);
	CloseHandle(hEvtCmd);
	CloseHandle(hEvtRsp);
	if (hEvtReady)
		CloseHandle(hEvtReady);
	if (g_hShutdownEvt)
		CloseHandle(g_hShutdownEvt);

	if (ndi_receiver != nullptr) {
		ndi->framesync_destroy(ndi_frame_sync);
		ndi->recv_destroy(ndi_receiver);
		ndi_receiver = nullptr;
	}

	if (ndi && ndi->destroy) {
		ndi->destroy();
	}

	return 0;
}
