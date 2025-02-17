#pragma once

const double epsilon = std::numeric_limits<double>::epsilon();

struct TargetFunction{
	const char *name;
	const wchar_t *module;
	void *handler;
};

enum class FunctionId{
	GetClipboardData = 0,
	SetClipboardData,
	//NtUserSetClipboardData,
	OleSetClipboard,
	OleGetClipboard,
	COUNT,
};

constexpr size_t get_clipboard_data_index = 0;
constexpr size_t set_clipboard_data_index = 1;
constexpr size_t ntuser_set_clipboard_data_index = 2;
constexpr size_t ole_set_clipboard_index = 3;
constexpr size_t ole_get_clipboard_index = 4;
extern const TargetFunction functions[];

struct unique_handle
{
	unique_handle(const HANDLE &h) : my_handle(h) {}
	~unique_handle() {
		CloseHandle(my_handle);
	}

private:
  const HANDLE &my_handle;
};

class CriticalSection{
	CRITICAL_SECTION cs;
public:
	CriticalSection(){
		InitializeCriticalSection(&this->cs);
	}
	CriticalSection(const CriticalSection &) = delete;
	CriticalSection &operator=(const CriticalSection &) = delete;
	CriticalSection(CriticalSection &&) = delete;
	CriticalSection &operator=(CriticalSection &&) = delete;
	~CriticalSection(){
		DeleteCriticalSection(&this->cs);
	}
	void lock(){
		EnterCriticalSection(&this->cs);
	}
	void unlock(){
		LeaveCriticalSection(&this->cs);
	}
};
