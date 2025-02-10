/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
******************************************************************************/

/******************************************************************************
Original File: https://github.com/obsproject/obs-studio/blob/master/UI/remote-text.hpp

DistroAV Changes:
- Renamed extraHeaders to headers
- Made headers public
- Commented out versionString; should be passed in or set as a header
- Commented out blocking/non-threaded GetRemoteFile
- Changed Result to `void Result(int httpStatusCode, const QString &responseData, const QString &errorText)`
******************************************************************************/

#pragma once

#include <QThread>
#include <vector>
#include <string>

class RemoteTextThread : public QThread {
	Q_OBJECT

	std::string url;
	std::string contentType;
	std::string postData;

	int timeoutSec = 0;

	void run() override;

signals:
	void Result(int httpStatusCode, const QString &responseData, const QString &errorData);

public:
	std::vector<std::string> headers;

	inline RemoteTextThread(std::string url_, std::string contentType_ = std::string(),
				std::string postData_ = std::string(), int timeoutSec_ = 0)
		: url(url_),
		  contentType(contentType_),
		  postData(postData_),
		  timeoutSec(timeoutSec_)
	{
	}

	inline RemoteTextThread(std::string url_, std::vector<std::string> &&headers_,
				std::string contentType_ = std::string(), std::string postData_ = std::string(),
				int timeoutSec_ = 0)
		: url(url_),
		  contentType(contentType_),
		  postData(postData_),
		  headers(std::move(headers_)),
		  timeoutSec(timeoutSec_)
	{
	}
};

//#define USE_GET_REMOTE_FILE
#ifdef USE_GET_REMOTE_FILE
bool GetRemoteFile(const char *url, std::string &str, std::string &error, long *responseCode = nullptr,
		   const char *contentType = nullptr, std::string request_type = "", const char *postData = nullptr,
		   std::vector<std::string> extraHeaders = std::vector<std::string>(), std::string *signature = nullptr,
		   int timeoutSec = 0, bool fail_on_error = true, int postDataSize = 0);
#endif
