#include "cloud.h"

#include <pthread.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>

#include "firebase/app_check/debug_provider.h"
#include "firebase/log.h"

#include "plugin-main.h"

// clang-format off
// Your Firebase project's Debug token goes here.
// You can get this from Firebase Console, in the App Check settings.
// This is only for dev/test purposes and should never be committed to version control.
const char kAppCheckDebugToken[] = "NEVER COMMIT THIS TO VERSION CONTROL";

// TODO:(pv) Inject these from Secrets...
const char kFirebaseProjectId[] = "NEVER COMMIT THIS TO VERSION CONTROL";
const char kFirebaseClientApiKey[] = "NEVER COMMIT THIS TO VERSION CONTROL";
const char kFirebaseClientAppId[] = "NEVER COMMIT THIS TO VERSION CONTROL";
// clang-format on

bool IsMainThread();
void PostToMainThread(std::function<void()> task);
bool ProcessEvents(int msec);
void WaitForCompletion(const firebase::FutureBase &future, const char *name);

class CloudCustomAppCheckProvider
	: public firebase::app_check::AppCheckProvider {
public:
	void SetToken(ProviderAppCheckToken provider_app_check_token)
	{
		provider_app_check_token_ = provider_app_check_token;
	}

	void GetToken(std::function<void(firebase::app_check::AppCheckToken,
					 int, const std::string &)>
			      completion_callback)
	{
		blog(LOG_INFO,
		     "[obs-ndi] cloud: +CloudCustomAppCheckProvider::GetToken(...)");
		completion_callback(provider_app_check_token_.app_check_token,
				    provider_app_check_token_.error_code,
				    provider_app_check_token_.error_text);
		blog(LOG_INFO,
		     "[obs-ndi] cloud: -CloudCustomAppCheckProvider::GetToken(...)");
	}

private:
	ProviderAppCheckToken provider_app_check_token_;
};

class CloudCustomAppCheckProviderFactory
	: public firebase::app_check::AppCheckProviderFactory {
public:
	static CloudCustomAppCheckProviderFactory *GetInstance()
	{
		static CloudCustomAppCheckProviderFactory instance;
		return &instance;
	}

	CloudCustomAppCheckProviderFactory *
	SetProvider(CloudCustomAppCheckProvider *app_check_provider)
	{
		app_check_provider_ = app_check_provider;
		return this;
	}

	firebase::app_check::AppCheckProvider *CreateProvider(firebase::App *)
	{
		blog(LOG_INFO,
		     "[obs-ndi] cloud: +CloudCustomAppCheckProviderFactory::CreateProvider(...)");
		auto app_check_provider = app_check_provider_;
		blog(LOG_INFO,
		     "[obs-ndi] cloud: -CloudCustomAppCheckProviderFactory::CreateProvider(...)");
		return app_check_provider;
	}

private:
	CloudCustomAppCheckProvider *app_check_provider_;
	CloudCustomAppCheckProviderFactory() : app_check_provider_(nullptr) {}
	~CloudCustomAppCheckProviderFactory() {}
};

//
//
//

void CloudAppCheckListener::OnAppCheckTokenChanged(
	const firebase::app_check::AppCheckToken &token)
{
	blog(LOG_INFO,
	     "[obs-ndi] cloud: +CloudAppCheckListener::OnAppCheckTokenChanged(...)");
	last_token_ = token;
	num_token_changes_ += 1;
	blog(LOG_INFO,
	     "[obs-ndi] cloud: -CloudAppCheckListener::OnAppCheckTokenChanged(...)");
}

//
//
//

Cloud::Cloud() {}

Cloud::~Cloud()
{
	TerminateApp();
}

void Cloud::InitializeAppCheckProvider()
{
	blog(LOG_INFO, "[obs-ndi] cloud: +Cloud::InitializeAppCheckProvider()");
	if (UseEmulator()) {
		blog(LOG_INFO,
		     "EMULATOR: Initialize Firebase App Check with Debug Provider");
		firebase::app_check::DebugAppCheckProviderFactory::GetInstance()
			->SetDebugToken(kAppCheckDebugToken);
		firebase::app_check::AppCheck::SetAppCheckProviderFactory(
			firebase::app_check::DebugAppCheckProviderFactory::
				GetInstance());
	} else {
		blog(LOG_INFO,
		     "Non-Emulator: Initialize Firebase CloudCustomAppCheckProviderFactory with CloudCustomAppCheckProvider");
		app_check_provider_ =
			std::make_unique<CloudCustomAppCheckProvider>();
		firebase::app_check::AppCheck::SetAppCheckProviderFactory(
			CloudCustomAppCheckProviderFactory::GetInstance()
				->SetProvider(app_check_provider_.get()));
	}
	blog(LOG_INFO, "[obs-ndi] cloud: -Cloud::InitializeAppCheckProvider()");
}

void Cloud::TerminateAppCheckProvider()
{
	blog(LOG_INFO, "[obs-ndi] cloud: +Cloud::TerminateAppCheckProvider()");
	firebase::app_check::AppCheck::SetAppCheckProviderFactory(nullptr);
	app_check_provider_.reset();
	blog(LOG_INFO, "[obs-ndi] cloud: -Cloud::TerminateAppCheckProvider()");
}

bool Cloud::UseEmulator()
{
	return strncmp(PLUGIN_UPDATE_HOST, "127.0.0.1", 9) == 0;
}

void Cloud::InitializeApp()
{
	blog(LOG_INFO, "[obs-ndi] cloud: +Cloud::InitializeApp()");
	if (!app_) {
		firebase::SetLogLevel(firebase::kLogLevelVerbose);

		InitializeAppCheckProvider();

		firebase::AppOptions app_options;
		app_options.set_project_id(kFirebaseProjectId);
		app_options.set_api_key(kFirebaseClientApiKey);
		app_options.set_app_id(kFirebaseClientAppId);

		app_.reset(firebase::App::Create(app_options));

		//firestore_ = firebase::firestore::Firestore::GetInstance(app_);

		functions_.reset(firebase::functions::Functions::GetInstance(
			app_.get()));
		if (UseEmulator()) {
			auto update_host =
				std::string(PLUGIN_UPDATE_HOST) + ":5001";
			auto update_host_cstr = update_host.c_str();
			blog(LOG_INFO,
			     "[obs-ndi] cloud: InitializeApp: Using Functions Emulator at %s",
			     update_host_cstr);
			functions_->UseFunctionsEmulator(update_host_cstr);
		} else {
			blog(LOG_INFO,
			     "[obs-ndi] cloud: InitializeApp: Using Functions Non-Emulator");
		}

		// app_check_ requires functions_ in order to get the app check token,
		// so app_check_ must be initialized after functions_.
		app_check_.reset(
			firebase::app_check::AppCheck::GetInstance(app_.get()));
		app_check_->AddAppCheckListener(&app_check_listener_);

		if (app_check_provider_) {
			FetchAppCheckTokenAsync([this](ProviderAppCheckToken
							       provider_app_check_token) {
				blog(LOG_INFO,
				     "[obs-ndi] cloud: InitializeApp: FetchAppCheckTokenAsync: provider_app_check_token.error_code=%d, provider_app_check_token.error_text=\"%s\"",
				     provider_app_check_token.error_code,
				     provider_app_check_token.error_text
					     .c_str());
				if (provider_app_check_token.error_code == 0) {
					blog(LOG_INFO,
					     "[obs-ndi] cloud: InitializeApp: FetchAppCheckTokenAsync: provider_app_check_token.app_check_token.expire_time_millis=%lld, provider_app_check_token.app_check_token.token=\"%s\"",
					     provider_app_check_token
						     .app_check_token
						     .expire_time_millis,
					     provider_app_check_token
						     .app_check_token.token
						     .c_str());
				}

				std::unique_lock<std::mutex> lock(mutex_);
				app_check_token_fetched_ =
					(provider_app_check_token.error_code ==
					 0);

				ExecutePendingCalls();
			});
		}
	}
	blog(LOG_INFO, "[obs-ndi] cloud: -Cloud::InitializeApp()");
}

void Cloud::TerminateApp()
{
	blog(LOG_INFO, "[obs-ndi] cloud: +Cloud::TerminateApp()");
	if (app_check_) {
		app_check_->RemoveAppCheckListener(&app_check_listener_);
		blog(LOG_INFO, "Shutdown the App Check library.");
		app_check_.reset();
	}
	TerminateAppCheckProvider();
	if (functions_) {
		blog(LOG_INFO, "Shutdown the Functions library.");
		functions_.reset();
	}
	// if (firestore_) {
	// 	blog(LOG_INFO, "Shutdown the Firestore library.");
	//    firestore_.reset();
	// }
	if (app_) {
		blog(LOG_INFO, "Shutdown the Firebase App.");
		app_.reset();
	}
	blog(LOG_INFO, "[obs-ndi] cloud: -Cloud::TerminateApp()");
}

void Cloud::EnqueueAsyncCall(std::function<void()> call)
{
	std::unique_lock<std::mutex> lock(mutex_);
	if (app_check_token_fetched_) {
		PostToMainThread(call);
	} else {
		async_call_queue_.push(call);
	}
}

void Cloud::ExecutePendingCalls()
{
	if (!IsMainThread()) {
		PostToMainThread([this]() { ExecutePendingCalls(); });
		return;
	}
	std::unique_lock<std::mutex> lock(mutex_);
	while (!async_call_queue_.empty()) {
		auto call = async_call_queue_.front();
		async_call_queue_.pop();
		call();
	}
}

firebase::Variant Cloud::Call(const char *name, const firebase::Variant &data)
{
	blog(LOG_INFO, "[obs-ndi] cloud: tid=%ld +Cloud::Call(`%s`, ...)",
	     pthread_self(), name);
	firebase::Variant result;
	if (functions_) {
		auto httpcallref = functions_->GetHttpsCallable(name);
		blog(LOG_INFO, "[obs-ndi] cloud: Call: `%s` +Call(...)", name);
		auto future = httpcallref.Call(data);
		blog(LOG_INFO, "[obs-ndi] cloud: Call: `%s` -Call(...)", name);
		WaitForCompletion(
			future,
			((std::string("call functions->`") + name) + "`")
				.c_str());
		result = OnCallCompleted(future, nullptr);
	}
	blog(LOG_INFO, "[obs-ndi] cloud: tid=%ld -Cloud::Call(`%s`, ...)",
	     pthread_self(), name);
	return result;
}

firebase::Future<firebase::functions::HttpsCallableResult>
Cloud::CallAsync(const char *name, const firebase::Variant &data,
		 std::function<void(const firebase::Variant &data, int error,
				    const std::string &)>
			 completion_callback)
{
	blog(LOG_INFO, "[obs-ndi] cloud: tid=%ld +Cloud::CallAsync(`%s`, ...)",
	     pthread_self(), name);
	firebase::Future<firebase::functions::HttpsCallableResult> future;
	if (functions_) {
		auto httpcallref = functions_->GetHttpsCallable(name);
		blog(LOG_INFO, "[obs-ndi] cloud: CallAsync: `%s` +Call(...)",
		     name);
		future = httpcallref.Call(data);
		blog(LOG_INFO, "[obs-ndi] cloud: CallAsync: `%s` -Call(...)",
		     name);
		future.OnCompletion(
			[this, completion_callback](
				const firebase::Future<
					firebase::functions::HttpsCallableResult>
					&future) {
				OnCallCompleted(future, completion_callback);
			});
	}
	blog(LOG_INFO, "[obs-ndi] cloud: tid=%ld -Cloud::CallAsync(`%s`, ...)",
	     pthread_self(), name);
	return future;
}

firebase::Variant Cloud::OnCallCompleted(
	const firebase::Future<firebase::functions::HttpsCallableResult> &future,
	std::function<void(const firebase::Variant &data, int error,
			   const std::string &)>
		completion_callback)
{
	blog(LOG_INFO, "[obs-ndi] cloud: tid=%ld +Cloud::OnCallCompleted(...)",
	     pthread_self());
	firebase::Variant data;
	if (future.status() == firebase::kFutureStatusComplete) {
		auto httpcallresult = future.result();
		auto error = future.error();
		auto error_message = future.error_message();
		if (error == 0) {
			data = httpcallresult->data();
		}
		if (completion_callback) {
			completion_callback(data, error, error_message);
		}
	}
	blog(LOG_INFO, "[obs-ndi] cloud: tid=%ld -Cloud::OnCallCompleted(...)",
	     pthread_self());
	return data;
}

firebase::Future<firebase::functions::HttpsCallableResult>
Cloud::FetchAppCheckTokenAsync(
	std::function<void(ProviderAppCheckToken)> completion_callback)
{
	auto data = firebase::Variant(firebase::Variant::EmptyMap());
	data.map()["foo"] = "bar";

	auto future = CallAsync(
		"fetchAppCheckToken", data,
		[this, completion_callback](const firebase::Variant &data,
					    int error, const std::string &) {
			ProviderAppCheckToken provider_app_check_token;
			if (error == 0) {
				if (data.is_map()) {
					auto map = data.map();
					auto token = map["token"];
					if (token.is_null()) {
						auto error_code =
							map["errorCode"];
						auto error_text =
							map["errorText"];
						provider_app_check_token
							.error_code =
							error_code.int64_value();
						provider_app_check_token
							.error_text =
							error_text
								.string_value();
					} else {
						auto expire_time_millis =
							map["expireTimeMillis"];
						provider_app_check_token
							.app_check_token.token =
							token.string_value();
						provider_app_check_token
							.app_check_token
							.expire_time_millis =
							expire_time_millis
								.int64_value();
						provider_app_check_token
							.error_code = 0;
						provider_app_check_token
							.error_text = "";
					}
				}
			}
			blog(LOG_INFO,
			     "[obs-ndi] cloud: FetchAppCheckTokenAsync: provider_app_check_token.error_code=%d, provider_app_check_token.error_text=\"%s\"",
			     provider_app_check_token.error_code,
			     provider_app_check_token.error_text.c_str());
			if (provider_app_check_token.error_code == 0) {
				blog(LOG_INFO,
				     "[obs-ndi] cloud: FetchAppCheckTokenAsync: provider_app_check_token.app_check_token.expire_time_millis=%lld, provider_app_check_token.app_check_token.token=\"%s\"",
				     provider_app_check_token.app_check_token
					     .expire_time_millis,
				     provider_app_check_token.app_check_token
					     .token.c_str());
			}
			if (app_check_provider_) {
				app_check_provider_->SetToken(
					provider_app_check_token);
			}
			if (completion_callback) {
				completion_callback(provider_app_check_token);
			}
		});
	return future;
}

int Cloud::Foo()
{
	return Call("foo", firebase::Variant()).int64_value();
}

firebase::Future<firebase::functions::HttpsCallableResult>
Cloud::FooAsync(std::function<void(int)> completion_callback)
{
	blog(LOG_INFO, "[obs-ndi] cloud: +Cloud::FooAsync(...)");
	firebase::Future<firebase::functions::HttpsCallableResult> future;
	EnqueueAsyncCall([this, &future, completion_callback] {
		future = CallAsync(
			"foo", firebase::Variant(),
			[completion_callback](const firebase::Variant &data,
					      int error, const std::string &) {
				int value = (error == 0) ? data.int64_value()
							 : -1;
				if (completion_callback) {
					completion_callback(value);
				}
			});
	});
	blog(LOG_INFO, "[obs-ndi] cloud: -Cloud::FooAsync(...)");
	return future;
}

int Cloud::Bar()
{
	return Call("bar", firebase::Variant()).int64_value();
}

firebase::Future<firebase::functions::HttpsCallableResult>
Cloud::BarAsync(std::function<void(int)> completion_callback)
{
	blog(LOG_INFO, "[obs-ndi] cloud: +Cloud::BarAsync(...)");
	firebase::Future<firebase::functions::HttpsCallableResult> future;
	EnqueueAsyncCall([this, &future, completion_callback] {
		future = CallAsync(
			"bar", firebase::Variant(),
			[this,
			 completion_callback](const firebase::Variant &data,
					      int error, const std::string &) {
				int value = (error == 0) ? data.int64_value()
							 : -1;
				if (completion_callback) {
					completion_callback(value);
				}
			});
	});
	blog(LOG_INFO, "[obs-ndi] cloud: -Cloud::BarAsync(...)");
	return future;
}

//
//
//

bool IsMainThread()
{
	return QThread::currentThread() ==
	       QCoreApplication::instance()->thread();
}

void PostToMainThread(std::function<void()> task)
{
	if (IsMainThread()) {
		task();
	} else {
		blog(LOG_INFO, "[obs-ndi] cloud: PostToMainThread(...)");
		QMetaObject::invokeMethod(QCoreApplication::instance(),
					  [task]() { task(); });
	}
}

bool ProcessEvents(int msec)
{
	static bool quit = false;
#ifdef _WIN32
	Sleep(msec);
#else
	usleep(msec * 1000);
#endif // _WIN32
	return quit;
}

void WaitForCompletion(const firebase::FutureBase &future, const char *name)
{
	(void)name;
	while (future.status() == firebase::kFutureStatusPending) {
		ProcessEvents(100);
	}
}
