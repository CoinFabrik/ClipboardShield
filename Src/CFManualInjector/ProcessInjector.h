#pragma once

#include "declarations.h"
#include "utility.h"
#include "../common/utility.h"
#include "../common/md5.h"
#include "../common/SharedMemory.h"
#include <string>
#include <atomic>

class InjectorConfigurationReader;

class ProcessInjector{
	autohandle_t process;
	std::wstring dll_path;
	std::uint32_t pid;
	RemoteMemory shared_memory;
	MANUAL_INJECT injection_struct;

	autohandle_t server_file_mapping;
	autohandle_t client_file_mapping;
	autohandle_t server_event;
	autohandle_t client_event;
	autoview_t<basic_shared_memory_block<shared_memory_size>> server_autoshared_memory;
	autoview_t<basic_shared_memory_block<return_shared_memory_size>> client_autoshared_memory;

	struct copy_injection_code_result{
		void *module_address;
		void *RtlPcToFileHeader_hook_position;
	};

	struct copy_image_to_remote_result{
		void *image_base;
		size_t image_size;
		IMAGE_DOS_HEADER dos_header;
		IMAGE_NT_HEADERS nt_header;
	};

	void print_statistics();
	copy_injection_code_result copy_injection_code(uintptr_t image_base, uintptr_t image_size);
	void copy_init_data(
		void *image_base,
		copy_injection_code_result,
		const IMAGE_DOS_HEADER &,
		const IMAGE_NT_HEADERS &,
		const std::uint8_t *dll_buffer,
		const IMAGE_DATA_DIRECTORY &);
	copy_image_to_remote_result copy_image_to_remote(const std::uint8_t *image, size_t image_size);
	static std::uint64_t generate64();
	InjectedIpcData open_shared_memories();
	void open_server_side();
	void open_client_side(std::uint64_t unique_pid);
public:
	ProcessInjector(const wchar_t *dll_path, const wchar_t *pid_string);
	~ProcessInjector();
	void inject();
	void monitor();
};
