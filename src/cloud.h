#pragma once

#include <memory>
#include <mutex>
#include <queue>

#include "firebase/app.h"
#include "firebase/app_check.h"
//#include "firebase/firestore.h"
#include "firebase/functions.h"

struct ProviderAppCheckToken {
	firebase::app_check::AppCheckToken app_check_token;
	int error_code;
	std::string error_text;

	ProviderAppCheckToken() { Reset(); }

	void Reset()
	{
		app_check_token = {"", 0};
		error_code = -1;
		error_text = "";
	}

	bool IsEmpty() const { return error_code == -1; }
};

class CloudCustomAppCheckProvider;

class CloudAppCheckListener : public firebase::app_check::AppCheckListener {
public:
	CloudAppCheckListener() : num_token_changes_(0) {}
	~CloudAppCheckListener() override {}

	void OnAppCheckTokenChanged(
		const firebase::app_check::AppCheckToken &token) override;
	int GetNumTokenChanges() const { return num_token_changes_; }
	const firebase::app_check::AppCheckToken &GetLastToken() const
	{
		return last_token_;
	}

private:
	int num_token_changes_;
	firebase::app_check::AppCheckToken last_token_;
};

class Cloud {
public:
	Cloud();
	~Cloud();

	void InitializeApp();
	void TerminateApp();

	int64_t Foo();
	firebase::Future<firebase::functions::HttpsCallableResult>
	FooAsync(std::function<void(int64_t)> completion_callback = nullptr);

	int64_t Bar();
	firebase::Future<firebase::functions::HttpsCallableResult>
	BarAsync(std::function<void(int64_t)> completion_callback = nullptr);

private:
	std::unique_ptr<firebase::App> app_;
	std::unique_ptr<firebase::app_check::AppCheck> app_check_;
	std::unique_ptr<firebase::functions::Functions> functions_;

	std::unique_ptr<CloudCustomAppCheckProvider> app_check_provider_;
	CloudAppCheckListener app_check_listener_;

	bool UseEmulator();
	void InitializeAppCheckProvider();
	void TerminateAppCheckProvider();

	firebase::Future<firebase::functions::HttpsCallableResult>
	FetchAppCheckTokenAsync(
		std::function<void(ProviderAppCheckToken)> completion_callback);

	void EnqueueAsyncCall(std::function<void()> call);
	void ExecutePendingCalls();
	void ClearPendingCalls();

	std::mutex mutex_;
	bool app_check_token_fetched_ = false;
	std::queue<std::function<void()>> async_call_queue_;

	firebase::Variant Call(const char *name, const firebase::Variant &data);
	firebase::Future<firebase::functions::HttpsCallableResult>
	CallAsync(const char *name, const firebase::Variant &data,
		  std::function<void(const firebase::Variant &data, int error,
				     const std::string &)>
			  completion_callback);
	firebase::Variant OnCallCompleted(
		const firebase::Future<firebase::functions::HttpsCallableResult>
			&future,
		std::function<void(const firebase::Variant &data, int error,
				   const std::string &)>
			completion_callback);
};
