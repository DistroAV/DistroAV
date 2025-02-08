#pragma once
#include "plugin-main.h"
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <Processing.NDI.Lib.h>

class NDIFinder {
public:
	using Callback = std::function<void(void *)>;

	static std::vector<std::string> getNDISourceList(Callback callback);

private:
	static std::vector<std::string> NDISourceList;
	static std::chrono::time_point<std::chrono::steady_clock>
		lastRefreshTime;
	static std::mutex listMutex;
	static bool isRefreshing;
	static void refreshNDISourceList(Callback callback);
	static void retrieveNDISourceList();
};
