#include "ProcessInjector.h"
#include "declarations.h"
#include "CodeGenerator.h"
#include "loader.h"
#include "utility.h"
#include "../common/IpcStructures.h"
#include "../common/SharedMemory.h"
#include "NtMapViewOfSection.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <iomanip>
#include <random>
#include <type_traits>

#if _MSC_VER >= 1400
extern "C" IMAGE_DOS_HEADER __ImageBase;
HMODULE current_HMODULE(){
	return (HMODULE)&__ImageBase;
}
#else
#error
#endif

const size_t tz_info_size = 1 << 12;

std::vector<std::uint8_t> serialize_self(){
	auto pbuffer = (const std::uint8_t *)current_HMODULE();
	
	auto pIDH = (PIMAGE_DOS_HEADER)pbuffer;
 
	if (pIDH->e_magic != IMAGE_DOS_SIGNATURE)
		throw std::runtime_error("Invalid executable image");
 
	auto pINH = (PIMAGE_NT_HEADERS)(pbuffer + pIDH->e_lfanew);
 
	if (pINH->Signature != IMAGE_NT_SIGNATURE)
		throw std::runtime_error("Invalid PE header");
 
	std::vector<std::uint8_t> ret(pINH->OptionalHeader.SizeOfImage);
	if (!ret.size())
		return ret;

	memcpy(&ret[0], pbuffer, pINH->OptionalHeader.SizeOfHeaders);

	auto pISH = (PIMAGE_SECTION_HEADER)(pINH + 1);

	for (decltype(pINH->FileHeader.NumberOfSections) i = 0; i < pINH->FileHeader.NumberOfSections; i++){
		auto offset = pISH[i].VirtualAddress;
		auto size = pISH[i].SizeOfRawData;
		if (offset + size > ret.size()){
			static const char * const error_message = "Invalid own DLL? x86"
#ifdef _M_X64
				"-64"
#endif
			;
			throw std::runtime_error(error_message);
		}

		memcpy(&ret[offset], pbuffer + offset, size);
	}

	return ret;
}

ProcessInjector::ProcessInjector(const wchar_t *dll_path, const wchar_t *pid_string){
	this->dll_path = dll_path;
	this->pid = watoi(pid_string);

	this->process.reset(OpenProcess(PROCESS_ALL_ACCESS, false, this->pid));
	if (!this->process){
		auto error = GetLastError();
		throw std::runtime_error("Unable to open process: " + std::to_string(error));
	}
}

ProcessInjector::~ProcessInjector(){
	this->shared_memory.release();
}

ProcessInjector::copy_injection_code_result ProcessInjector::copy_injection_code(uintptr_t image_base, uintptr_t image_size){
	auto myself_serialized = serialize_self();
#ifdef _M_X64
	CodeGenerator generator;
	const size_t generated_code_size = generator.generate(image_base, image_size, (uintptr_t)RtlPcToFileHeader);
#else
	const size_t generated_code_size = 0;
#endif

	const size_t padding = 0;

	auto loader = allocate_remote_memory(this->process, integer_ceiling<size_t>(myself_serialized.size() + padding + generated_code_size, 4096));
	std::cout << "Loader allocated at " << to_hex(loader.get()) << std::endl;
	if (myself_serialized.size()){
		auto error = loader.copy_to_remote(&myself_serialized[0], myself_serialized.size(), 0);
		if (error)
			throw std::runtime_error("Failed to copy loader code: " + std::to_string(error));
	}
	
	if (padding){
		std::unique_ptr<char[]> temp_buffer(new char[padding]);
		memset(temp_buffer.get(), 0xCC, padding);
		loader.copy_to_remote(temp_buffer.get(), padding, myself_serialized.size());
	}
	
#ifdef _M_X64
	auto generated_code = generator.finalize((uintptr_t)loader.get());
#else
	std::vector<std::uint8_t> generated_code;
#endif

	const size_t RtlPcToFileHeader_hook_position = myself_serialized.size() + padding;

	if (generated_code.size()){
		auto error = loader.copy_to_remote(&generated_code[0], generated_code.size(), RtlPcToFileHeader_hook_position);
		if (error)
			throw std::runtime_error("Failed to copy generated code: " + std::to_string(error));
	}

	auto error = loader.remove_write_permission();
	if (error)
		throw std::runtime_error("Failed to remove write permission from loader page: " + std::to_string(error));
	auto address = (std::uint8_t *)loader.get();
	loader.release();
	copy_injection_code_result ret;
	ret.module_address = address;
	ret.RtlPcToFileHeader_hook_position = address + RtlPcToFileHeader_hook_position;
	return ret;
}

static std::wstring build_client_shared_path(DWORD pid, std::uint64_t id){
	std::wstringstream stream;
	stream << RETURN_SHARED_MEMORY_PATH_CLIENT << pid << '_' << id;
	return stream.str();
}

static std::wstring build_client_event_path(DWORD pid, std::uint64_t id){
	std::wstringstream stream;
	stream << RETURN_EVENT_PATH_CLIENT << pid << '_' << id;
	return stream.str();
}

const ULONG ViewShare = 1;
const ULONG ViewUnmap = 2;

void *map_section(HANDLE file_mapping, HANDLE target_process, size_t section_size){
	void *ret = nullptr;
	LARGE_INTEGER offset;
	offset.QuadPart = 0;
	SIZE_T viewsize = section_size;
	auto status = NktNtMapViewOfSection(file_mapping, target_process, &ret, 0, return_shared_memory_size, &offset, &viewsize, ViewShare, 0, PAGE_READWRITE);
	if (status){
		std::stringstream stream;
		stream << "NtMapViewOfSection() failed with error 0x" << std::hex << std::setw(8) << std::setfill('0') << (std::make_unsigned<decltype(status)>::type)status;
		throw std::runtime_error(stream.str());
	}

	return ret;
}

HANDLE duplicate_handle(HANDLE source, HANDLE self, HANDLE target, const char *context = nullptr){
	HANDLE ret;
	if (!DuplicateHandle(self, source, target, &ret, 0, false, DUPLICATE_SAME_ACCESS)){
		auto error = GetLastError();
		report_failed_win32_function("DuplicateHandle", error, context);
	}
	return ret;
}

autohandle_t open_event(const wchar_t *path){
	auto ret = OpenEventW(EVENT_ALL_ACCESS, false, GLOBAL_EVENT_PATH);
	auto error = GetLastError();
	if (invalid_handle(ret))
		report_failed_win32_function("OpenEvent", error, "Opening server event");
	return autohandle_t(ret);
}

void ProcessInjector::open_server_side(){
	auto server_file_map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, false, SHARED_MEMORY_PATH);
	auto error = GetLastError();
	if (invalid_handle(server_file_map))
		report_failed_win32_function("OpenFileMapping", error, "initializing server IPC");
	this->server_file_mapping.reset(server_file_map);
	auto server_shared_memory = (basic_shared_memory_block<shared_memory_size> *)MapViewOfFile(server_file_map, FILE_MAP_ALL_ACCESS, 0, 0, shared_memory_size);
	error = GetLastError();
	if (!server_shared_memory)
		report_failed_win32_function("MapViewOfFile", error, "initializing server IPC");
	this->server_autoshared_memory.reset(server_shared_memory);
}

void ProcessInjector::open_client_side(std::uint64_t unique_pid){
	auto client_path = build_client_shared_path(this->pid, unique_pid);
	AccessibleObject ao;
	auto client_file_map = CreateFileMappingW(INVALID_HANDLE_VALUE, &ao.sa, PAGE_READWRITE, 0, return_shared_memory_size, client_path.c_str());
	auto error = GetLastError();
	if (invalid_handle(client_file_map))
		report_failed_win32_function("CreateFileMapping", error, "initializing client IPC");
	this->client_file_mapping.reset(client_file_map);
	auto client_shared_memory = (basic_shared_memory_block<return_shared_memory_size> *)MapViewOfFile(client_file_map, FILE_MAP_ALL_ACCESS, 0, 0, return_shared_memory_size);
	error = GetLastError();
	if (!client_shared_memory)
		report_failed_win32_function("MapViewOfFile", error, "initializing client IPC");
	memset(client_shared_memory, 0, return_shared_memory_size);
	client_shared_memory->first_message = basic_shared_memory_block<return_shared_memory_size>::max_messages;
	client_autoshared_memory.reset(client_shared_memory);
}

InjectedIpcData ProcessInjector::open_shared_memories(){
	std::uint64_t unique_pid = this->generate64();
	this->open_server_side();
	this->open_client_side(unique_pid);

	auto self = GetCurrentProcess();
	auto target_process = this->process.get();

	auto duplicated_client_handle = duplicate_handle(this->client_file_mapping.get(), self, target_process, "Duplicating client handle");
	auto duplicated_server_handle = duplicate_handle(this->server_file_mapping.get(), self, target_process, "Duplicating server handle");
	
	InjectedIpcData ret;
	ret.process_unique_id = unique_pid;
	ret.outgoing.memory = map_section(this->server_file_mapping.get(), target_process, shared_memory_size);
	ret.incoming.memory = map_section(this->client_file_mapping.get(), target_process, return_shared_memory_size);

	auto event_client_path = build_client_event_path(this->pid, unique_pid);
	this->client_event = create_global_event(event_client_path.c_str());
	ret.incoming.event = duplicate_handle(client_event.get(), self, target_process, "Duplicating client event");

	this->server_event = open_event(GLOBAL_EVENT_PATH);
	ret.outgoing.event = duplicate_handle(server_event.get(), self, target_process, "Duplicating server event");

	return ret;
}

std::uint64_t ProcessInjector::generate64(){
	std::random_device dev;
	std::mt19937_64 rng((std::uint64_t)dev() | ((std::uint64_t)dev() << 32));
	return rng();
}

void ProcessInjector::copy_init_data(void *image_base, copy_injection_code_result cidr, const IMAGE_DOS_HEADER &dos_header, const IMAGE_NT_HEADERS &nt_headers, const std::uint8_t *dll_buffer, const IMAGE_DATA_DIRECTORY &exception_information){
	const int aux_buffer_size = 2048;
	const int extra_size = 256;

	const size_t base = 0;
	const size_t aux_buffer_position = base + sizeof(MANUAL_INJECT);
	const size_t total_size = integer_ceiling<size_t>(aux_buffer_position + aux_buffer_size + extra_size, 4096);

	this->shared_memory = allocate_remote_memory(this->process, total_size);

	zero_structure(this->injection_struct);

	this->injection_struct.image_base = image_base;
	this->injection_struct.buffer = (std::uint8_t *)this->shared_memory.get() + aux_buffer_position;
	this->injection_struct.nt_headers = (PIMAGE_NT_HEADERS)((std::uint8_t *)image_base + dos_header.e_lfanew);
	this->injection_struct.base_relocation = (PIMAGE_BASE_RELOCATION)((std::uint8_t *)image_base + nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
	this->injection_struct.import_directory = (PIMAGE_IMPORT_DESCRIPTOR)((std::uint8_t *)image_base + nt_headers.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	this->injection_struct.LoadLibraryA_f = LoadLibraryA;
	this->injection_struct.GetProcAddress_f = GetProcAddress;
	this->injection_struct.OutputDebugStringA_f = OutputDebugStringA;
#ifdef _M_X64
	this->injection_struct.exception_table = (std::uint8_t *)image_base + exception_information.VirtualAddress;
	this->injection_struct.exception_table_length = exception_information.Size / sizeof(RUNTIME_FUNCTION);
	this->injection_struct.RtlAddFunctionTable_f = RtlAddFunctionTable;
	this->injection_struct.RtlPcToFileHeader_f = (RtlPcToFileHeader_ft)((std::uint8_t *)cidr.RtlPcToFileHeader_hook_position);
	this->injection_struct.RtlPcToFileHeader_unhooked_f = RtlPcToFileHeader;
#endif
	
	this->injection_struct.ipc = this->open_shared_memories();
	
	if (!find_export(this->injection_struct.InitializeDll_f, image_base, dll_buffer, "InitializeDll2"))
		throw std::runtime_error("Couldn't find InitializeDll2() entry point in DLL.");

	auto error = this->shared_memory.copy_to_remote(&this->injection_struct, sizeof(this->injection_struct), 0);
	if (error != ERROR_SUCCESS)
		throw std::runtime_error("Failed to copy initialization struct to remote process: " + std::to_string(error));

	//Clear buffer area.
	const size_t zero_buffer_size = total_size - sizeof(this->injection_struct);
	std::unique_ptr<char[]> zero_buffer(new char[zero_buffer_size]);
	memset(zero_buffer.get(), 0, zero_buffer_size);
	this->shared_memory.copy_to_remote(zero_buffer.get(), zero_buffer_size, sizeof(this->injection_struct));
}

ProcessInjector::copy_image_to_remote_result ProcessInjector::copy_image_to_remote(const std::uint8_t *image_buffer, size_t image_size){
	auto dos_header = (PIMAGE_DOS_HEADER)image_buffer;
 
	if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
		throw std::runtime_error("Invalid executable image");
 
	auto nt_headers = (PIMAGE_NT_HEADERS)(image_buffer + dos_header->e_lfanew);
 
	if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
		throw std::runtime_error("Invalid PE header");
 
	if (!(nt_headers->FileHeader.Characteristics & IMAGE_FILE_DLL))
		throw std::runtime_error("Image is not a DLL");

	auto image = allocate_remote_memory(this->process, nt_headers->OptionalHeader.SizeOfImage);

	std::cout << "Image allocated at " << to_hex(image.get()) << std::endl;
	
	auto error = image.copy_to_remote(image_buffer, nt_headers->OptionalHeader.SizeOfHeaders, 0);
	if (error)
		throw std::runtime_error("Copying headers failed with error " + std::to_string(error));
	
	auto section_header = (PIMAGE_SECTION_HEADER)(nt_headers + 1);

	for (decltype(nt_headers->FileHeader.NumberOfSections) i = 0; i < nt_headers->FileHeader.NumberOfSections; i++){
		std::cout << "Copying from offset " << to_hex(section_header[i].PointerToRawData) << " " << to_hex(section_header[i].SizeOfRawData) << " bytes to offset " << to_hex(section_header[i].VirtualAddress) << std::endl;
		error = image.copy_to_remote(image_buffer + section_header[i].PointerToRawData, section_header[i].SizeOfRawData, section_header[i].VirtualAddress);
		if (error)
			throw std::runtime_error("Copying section " + std::to_string(i) + " failed with error " + std::to_string(error));
	}

	//image.remove_write_permission();

	copy_image_to_remote_result ret;
	ret.image_base = image.get();
	ret.image_size = image.get_size();
	image.release();
	ret.dos_header = *dos_header;
	ret.nt_header = *nt_headers;

	return ret;
}

void ProcessInjector::inject(){
	adjust_token_privileges();
	
	auto buffer = read_dll(this->dll_path.c_str());
	auto pbuffer = &buffer[0];

	auto image = this->copy_image_to_remote(pbuffer, buffer.size());
	
	auto &exception_information = image.nt_header.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
	auto injected = this->copy_injection_code((uintptr_t)image.image_base, image.image_size);
	this->copy_init_data(image.image_base, injected, image.dos_header, image.nt_header, pbuffer, exception_information);
	
	auto entry_point = (LPTHREAD_START_ROUTINE)((uintptr_t)injected.module_address + ((uintptr_t)load_dll - (uintptr_t)current_HMODULE()));

	std::cout << "Entry point: " << to_hex(entry_point) << " (" << to_hex(injected.module_address) << " + " << to_hex((uintptr_t)load_dll - (uintptr_t)current_HMODULE()) << ")" << std::endl;
	
	auto thread = CreateRemoteThread(this->process.get(), nullptr, 0, entry_point, this->shared_memory.get(), 0, nullptr);
 
	if (!thread){
		auto error = GetLastError();
		std::stringstream stream;
		stream << "CreateRemoteThread() failed with error " << error;
		throw std::runtime_error(stream.str());
	}
 
	if (WaitForSingleObject(thread, 2000) == WAIT_TIMEOUT){
		std::cout << "Remote thread timed out. Continuing anyway.\n";
		return;
	}

	DWORD exit_code;
	GetExitCodeThread(thread, &exit_code);

	std::cout << "Remote thread returned with code " << exit_code << std::endl;
}

void ProcessInjector::monitor(){
	WaitForSingleObject(this->process.get(), INFINITE);
	std::cout << "Process terminated. Joining thread.\n";
}
