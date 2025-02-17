#include <Windows.h>
#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <iostream>

typedef std::uint8_t u8;

class Clipboard{
	bool ok;
public:
	Clipboard(){
		this->ok = !!OpenClipboard(nullptr);
	}
	Clipboard(const Clipboard &) = delete;
	Clipboard &operator=(const Clipboard &) = delete;
	Clipboard(Clipboard &&) = delete;
	Clipboard &operator=(Clipboard &&) = delete;
	~Clipboard(){
		if (this->ok)
			CloseClipboard();
	}
	operator bool() const{
		return this->ok;
	}
	std::unique_ptr<std::wstring> read() const{
		if (!this->ok)
			return {};
		auto handle = GetClipboardData(CF_UNICODETEXT);
		if (!handle || handle == INVALID_HANDLE_VALUE)
			return {};
		auto data = (const wchar_t *)GlobalLock(handle);
		if (!data)
			return {};
		std::wstring ret(wcslen(data), 0);
		if (ret.size())
			memcpy(&ret[0], data, ret.size() * sizeof(wchar_t));
		return std::make_unique<std::wstring>(std::move(ret));
	}
};

int main(){
	try{
		while (true){
			{
				Clipboard c;
				if (c){
					auto data = c.read();
					if (data)
						std::wcout << *data << std::endl;
				}
			}
			Sleep(1000);
		}
	}catch (std::exception &e){
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
