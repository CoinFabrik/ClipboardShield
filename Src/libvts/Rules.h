#pragma once

#include "utility.h"
#include "../common/utf8.h"
#include <string>
#include <fstream>
#include <sstream>

enum class FirewallPolicy{
	Allow,
	Deny,
};

template <typename T>
inline std::basic_ostream<T> &operator<<(std::basic_ostream<T> &stream, const GUID &guid){
	stream << guid.Data1 << '-' << guid.Data2 << '-' << guid.Data3;
	for (auto c : guid.Data4)
		stream << '-' << c;
	return stream;
}

inline const wchar_t *to_string(FirewallPolicy p){
	switch (p){
		case FirewallPolicy::Allow:
			return L"allow";
		case FirewallPolicy::Deny:
			return L"deny";
	}
	return L"";
}

class FirewallRule{
	std::wstring executable;
	FirewallPolicy default_policy;
	std::vector<std::pair<std::wstring, FirewallPolicy>> second_parties;
public:
	FirewallRule() = default;
	FirewallRule(const std::wstring &executable, FirewallPolicy default_policy = FirewallPolicy::Allow)
		: executable(executable)
		, default_policy(default_policy){}
	FirewallRule(const FirewallRule &) = default;
	FirewallRule &operator=(const FirewallRule &) = default;
	FirewallRule(FirewallRule &&other){
		*this = std::move(other);
	}
	const FirewallRule &operator=(FirewallRule &&other){
		this->executable = other.executable;
		this->default_policy = other.default_policy;
		this->second_parties = std::move(other.second_parties);
		return *this;
	}
	void add_second_party(const std::wstring &second_party, FirewallPolicy policy){
		this->second_parties.emplace_back(second_party, policy);
	}
	FirewallPolicy check_permission(const std::wstring &second_party, std::wstringstream &explanation) const{
		explanation << L", second_party = " << second_party;
		for (auto &sp : this->second_parties){
			if (paths_are_equivalent(second_party, sp.first) <= 0)
				continue;
			explanation << ", using specific rule";
			return sp.second;
		}
		explanation << ", second party not found, using default policy for first party";
		return this->default_policy;
	}
	FirewallPolicy check_permission(std::wstringstream &explanation) const{
		explanation << ", using default policy for first party";
		return this->default_policy;
	}
	const auto &get_executable() const{
		return this->executable;
	}
	void describe(std::wostream &stream) const{
		stream << L"first party " << this->executable << L": default " << to_string(this->default_policy) << "\r\n";
		for (auto &destination : this->second_parties)
			stream << L"    second party " << destination.first << L" -> " << to_string(destination.second) << "\r\n";
	}
};

class FirewallConfiguration{
	typedef std::vector<FirewallRule> map_t;
	map_t source_rules;
	map_t destination_rules;
	FirewallPolicy default_policy;

	static std::wstring process_newlines(const std::wstring &s) {
		std::wstring ret;
		bool state = false;
		for (auto c : s){
			if (!state){
				if (c == '\r'){
					state = true;
					continue;
				}
			}else{
				if (c == '\n'){
					ret += '\n';
					state = false;
					continue;
				}
				if (c == '\r'){
					ret += '\n';
					continue;
				}
				ret += '\n';
			}
			ret += c;
		}
		if (state)
			ret += '\n';
		return ret;
	}
	static std::pair<std::wstring, std::wstring> split_line(const std::wstring &line){
		auto space = line.find(' ');
		if (space == line.npos)
			return {line, {}};
		return {line.substr(0, space), line.substr(space + 1)};
	}
	FirewallPolicy check_permission(std::wstringstream &stream, const map_t &rules, const std::wstring &first_party, const std::wstring &second_party) const{
		stream << first_party;
		for (auto &rule : rules){
			if (paths_are_equivalent(first_party, rule.get_executable()) <= 0)
				continue;
			return rule.check_permission(second_party, stream);
		}
		stream << L", using default policy";
		return this->default_policy;
	}
	FirewallPolicy check_permission(std::wstringstream &stream, const map_t &rules, const std::wstring &first_party) const{
		stream << first_party;
		for (auto &rule : rules){
			if (paths_are_equivalent(first_party, rule.get_executable()) <= 0)
				continue;
			return rule.check_permission(stream);
		}
		stream << L", using default policy";
		return this->default_policy;
	}
	static FirewallRule *add_rule(std::vector<FirewallRule> &src_rules, std::vector<FirewallRule> &dst_rules, const std::wstring &argument, bool source, bool allow){
		auto &v = source ? src_rules : dst_rules;
		v.emplace_back(argument, allow ? FirewallPolicy::Allow : FirewallPolicy::Deny);
		return &v.back();
	}
	void add_source_rule(FirewallRule &&rule){
		this->source_rules.emplace_back(std::move(rule));
	}
	void add_destination_rule(FirewallRule &&rule){
		this->destination_rules.emplace_back(std::move(rule));
	}
public:
	FirewallConfiguration(FirewallPolicy default_policy = FirewallPolicy::Allow): default_policy(default_policy){}
	FirewallConfiguration(const FirewallConfiguration &) = default;
	FirewallConfiguration &operator=(const FirewallConfiguration &) = default;
	FirewallConfiguration(FirewallConfiguration &&other){
		*this = std::move(other);
	}
	const FirewallConfiguration &operator=(FirewallConfiguration &&other){
		this->source_rules = std::move(other.source_rules);
		this->destination_rules = std::move(other.destination_rules);
		this->default_policy = other.default_policy;
		return *this;
	}
	FirewallPolicy check_permission_source(const std::wstring &source, const std::wstring &destination, std::wstring &explanation) const{
		std::wstringstream stream;
		stream << "source = ";
		auto ret = this->check_permission(stream, this->source_rules, source, destination);
		explanation = stream.str();
		return ret;
	}
	FirewallPolicy check_permission_destination(const std::wstring &source, const std::wstring &destination, std::wstring &explanation) const{
		std::wstringstream stream;
		stream << "destination = ";
		auto ret = this->check_permission(stream, this->destination_rules, destination, source);
		explanation = stream.str();
		return ret;
	}
	FirewallPolicy check_permission_destination(const std::wstring &destination, std::wstring &explanation) const{
		std::wstringstream stream;
		stream << "destination = ";
		auto ret = this->check_permission(stream, this->destination_rules, destination);
		explanation = stream.str();
		return ret;
	}
	static FirewallConfiguration parse_config_file(const wchar_t *path){
		auto data = process_newlines(load_utf8_file(path));
		std::wstringstream stream(data);

		FirewallPolicy default_policy = FirewallPolicy::Allow;
		std::vector<FirewallRule> dst_rules;
		std::vector<FirewallRule> src_rules;
		FirewallRule *last_rule = nullptr;

		while (true){
			std::wstring line;
			std::getline(stream, line);
			if (!stream)
				break;
			if (!line.size())
				continue;

			bool indented = !!isspace(line.front());
			if (indented){
				auto not_space = line.find_first_not_of(L" \t");
				if (not_space == line.npos)
					continue;
				line = line.substr(not_space);
			}

			auto split = split_line(line);
			std::transform(split.first.begin(), split.first.end(), split.first.begin(), tolower);
			if (split.first == L"policy"){
				if (split.second == L"allow"){
					default_policy = FirewallPolicy::Allow;
				}else if (split.second == L"deny"){
					default_policy = FirewallPolicy::Deny;
				}else
					throw std::runtime_error("Invalid configuration file");
			}else if (split.first == L"allow"){
				if (!indented)
					throw std::runtime_error("Invalid configuration file");
				last_rule->add_second_party(split.second, FirewallPolicy::Allow);
			}else if (split.first == L"deny"){if (!indented)
				if (!indented)
					throw std::runtime_error("Invalid configuration file");
				last_rule->add_second_party(split.second, FirewallPolicy::Deny);
			}else if (split.first == L"source-allow"){
				if (indented)
					throw std::runtime_error("Invalid configuration file");
				last_rule = add_rule(src_rules, dst_rules, split.second, true, true);
			}else if (split.first == L"source-deny"){
				if (indented)
					throw std::runtime_error("Invalid configuration file");
				last_rule = add_rule(src_rules, dst_rules, split.second, true, false);
			}else if (split.first == L"destination-allow"){
				if (indented)
					throw std::runtime_error("Invalid configuration file");
				last_rule = add_rule(src_rules, dst_rules, split.second, false, true);
			}else if (split.first == L"destination-deny"){
				if (indented)
					throw std::runtime_error("Invalid configuration file");
				last_rule = add_rule(src_rules, dst_rules, split.second, false, false);
			}else
				throw std::runtime_error("Invalid configuration file");
		}

		FirewallConfiguration ret(default_policy);
		for (auto &rule : src_rules)
			ret.add_source_rule(std::move(rule));
		for (auto &rule : dst_rules)
			ret.add_destination_rule(std::move(rule));
		return ret;
	}
	std::wstring describe_rules() const{
		std::wstringstream ret;
		ret << L"Default policy: " << to_string(this->default_policy) << "\r\n"
			"Source rules:\r\n";
		for (auto &rule : this->source_rules)
			rule.describe(ret);
		ret << "Destination rules:\r\n";
		for (auto &rule : this->destination_rules)
			rule.describe(ret);
		return ret.str();
	}
};
