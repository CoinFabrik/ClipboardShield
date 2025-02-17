#pragma once

#include <cstdint>
#include <vector>
#include <type_traits>
#include <Windows.h>

#define GLOBAL_EVENT_PATH L"Global\\nktcbfw_event"
#define SHARED_MEMORY_PATH L"Global\\nktcbfw_shared_memory"
#define RETURN_EVENT_PATH_CLIENT L"Local\\nktcbfw_return_event_"
#define RETURN_SHARED_MEMORY_PATH_CLIENT L"Local\\nktcbfw_return_shared_memory_"
#define RETURN_EVENT_PATH_SERVER1 L"Session\\"
#define RETURN_EVENT_PATH_SERVER2 L"\\nktcbfw_return_event_"
#define RETURN_SHARED_MEMORY_PATH_SERVER1 L"Session\\"
#define RETURN_SHARED_MEMORY_PATH_SERVER2 L"\\nktcbfw_return_shared_memory_"

#define USE_MD5_DIGEST

enum class RequestType : LONG{
	Free = 0,
	//Other messages.
	Timeout,
	//Messages from client to server.
	Connection,
	CopyBegin,
	CopyEnd,
	Paste,
	//Responses
	ConnectionConfirmation,
	Continue,
	Cancel,
};

struct Payload{
	RequestType type;
	std::uint32_t process;
	std::uint64_t process_unique_id;
	std::uint32_t thread;
	std::uint32_t session;
	std::uint32_t next_message;
	static const size_t extra_data_size = 64 - sizeof(std::uint32_t) * 5 - sizeof(std::uint64_t);
	std::uint8_t extra_data[extra_data_size];
};

static_assert(std::is_pod<Payload>::value, "Oops!");
static_assert(sizeof(Payload) == 64, "Oops!");

struct Message{
	Payload payload;
#ifdef USE_MD5_DIGEST
	Md5Digest digest;
#endif
};

static_assert(std::is_pod<Message>::value, "Oops!");

const DWORD shared_memory_size = 1 << 20; // 1 MiB
const DWORD return_shared_memory_size = 1 << 12; // 4 KiB

template <size_t MaxSize>
struct basic_shared_memory_block{
	LONG first_message;
	static const LONG max_messages = (MaxSize - sizeof(LONG)) / sizeof(Message);
	Message messages[max_messages];
};

static_assert(std::is_pod<basic_shared_memory_block<shared_memory_size>>::value, "Oops!");
static_assert(std::is_pod<basic_shared_memory_block<return_shared_memory_size>>::value, "Oops!");

inline LONG interlocked_get(LONG &src){
	return *(volatile LONG *)&src;
}

inline void interlocked_set(LONG &dst, LONG value){
	*(volatile LONG *)&dst = value;
}

inline LONG exchange(LONG &dst, LONG value){
	return InterlockedExchange((volatile LONG *)&dst, value);
}

inline bool compare_exchange(LONG &dst, LONG value, LONG comparand){
	return InterlockedCompareExchange((volatile LONG *)&dst, value, comparand) == comparand;
}

template <typename T>
struct is_LONG_enum{
	static const bool value = std::is_same<typename std::underlying_type<T>::type, LONG>::value;
};

template <>
struct is_LONG_enum<LONG>{
	static const bool value = false;
};

template <typename T, typename Ret>
using enum_long_check = typename std::enable_if<is_LONG_enum<T>::value, Ret>::type;

template <typename T>
enum_long_check<T, T> interlocked_get(T &src){
	return interlocked_get(*(LONG *)&src);
}

template <typename T>
enum_long_check<T, void> interlocked_set(T &dst, T value){
	return interlocked_set(*(LONG *)&dst, (LONG)value);
}

template <typename T>
enum_long_check<T, LONG> exchange(T &dst, T value){
	return exchange(*(LONG *)&dst, (LONG)value);
}

template <typename T, typename T2>
enum_long_check<T, bool> compare_exchange(T &dst, T2 value, T comparand){
	return compare_exchange(*(LONG *)&dst, (LONG)value, (LONG)comparand);
}

template <size_t N>
bool push_new_message(basic_shared_memory_block<N> &dst, const Payload &payload, const autohandle_t &event, Md5 &hash){
	auto type = payload.type;
	LONG index = -1;
	do{
		LONG i = 0;
		for (auto &m : dst.messages){
			if (compare_exchange(m.payload.type, type, RequestType::Free)){
				index = i;
				break;
			}
			i++;
		}
	}while (index < 0);
	Message &destination = dst.messages[index];
	destination.payload = payload;
	MemoryBarrier();
	LONG head;
	do{
		head = dst.first_message;
		destination.payload.next_message = head;
	}while (!compare_exchange(dst.first_message, index, head));
#ifdef USE_MD5_DIGEST
	hash.reset();
	hash.update(&destination.payload, sizeof(destination.payload));
	destination.digest = hash.get_digest();
#endif
	return !!SetEvent(event.get());
}

inline Payload read_message(Md5 &hash, volatile Message &message){
#ifdef USE_MD5_DIGEST
	Payload ret;
	do{
		hash.reset();
		memcpy(&ret, (const void *)&message.payload, sizeof(ret));
		hash.update(&ret, sizeof(ret));
	}while (memcmp(hash.get_digest().data, (const void *)message.digest.data, Md5Digest::length) != 0);
#endif
	message.payload.type = RequestType::Free;
	return ret;
}

template <size_t N, typename T>
void pull_messages(basic_shared_memory_block<N> &src, std::vector<Payload> &payloads, Md5 &hash, const autohandle_t &event, DWORD timeout, const T &f){
	auto wait_result = WaitForSingleObject(event.get(), timeout);
	if (wait_result != WAIT_OBJECT_0)
		return;
	const auto invalid = basic_shared_memory_block<N>::max_messages;
	auto head = exchange(src.first_message, invalid);
	payloads.clear();
	for (auto index = head; index != invalid;){
		auto &current = src.messages[index];
		index = current.payload.next_message;
		payloads.push_back(read_message(hash, current));
	}
	for (auto &p : payloads)
		f(p);
}
