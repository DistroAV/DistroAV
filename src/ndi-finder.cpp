#include "ndi-finder.h"

std::vector<std::string> NDIFinder::NDISourceList;
std::chrono::time_point<std::chrono::steady_clock>
	NDIFinder::lastRefreshTime;
std::mutex NDIFinder::listMutex;
bool NDIFinder::isRefreshing = false;

std::vector<std::string> NDIFinder::getNDISourceList(Callback callback)
{
	std::lock_guard<std::mutex> lock(listMutex);
	auto now = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::seconds>(
				now - lastRefreshTime)
				.count();

	if (duration > 5 && !isRefreshing) {
		isRefreshing = true;
		std::thread(refreshNDISourceList, callback).detach();
	}

	return NDISourceList;
}

void NDIFinder::refreshNDISourceList(Callback callback)
{
	retrieveNDISourceList();
	std::lock_guard<std::mutex> lock(listMutex);
	lastRefreshTime = std::chrono::steady_clock::now();
	callback(&NDISourceList);		
	isRefreshing = false;
}

void NDIFinder::retrieveNDISourceList()
{
	NDIlib_find_create_t find_desc = {0};
	find_desc.show_local_sources = true;
	find_desc.p_groups = NULL;
	NDIlib_find_instance_t ndi_find = ndiLib->find_create_v2(&find_desc);

	if (!ndi_find) {
		return;
	}

	uint32_t n_sources = 0;
	uint32_t last_n_sources = 0;
	const NDIlib_source_t *sources = NULL;
	do {
		ndiLib->find_wait_for_sources(ndi_find, 1000);
		last_n_sources = n_sources;
		sources =
			ndiLib->find_get_current_sources(ndi_find, &n_sources);
	} while (n_sources > last_n_sources);

	std::vector<std::string> newList;
	for (uint32_t i = 0; i < n_sources; ++i) {
		newList.push_back(sources[i].p_ndi_name);
	}

	{
		std::lock_guard<std::mutex> lock(listMutex);
		NDISourceList = newList;
	}

	ndiLib->find_destroy(ndi_find);
}
