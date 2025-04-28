#include <cstring>
#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>

#include "PluginAPI.h"
#include "ConsoleExt.h"
#include "obse64_version.h"
#include "utils.h"

#include <MinHook.h>
#if _WIN64
#pragma comment(lib, "libMinHook.x64.lib")
#else
#pragma comment(lib, "libMinHook.x86.lib")
#endif

#define MAX_ARGS 64

#define HELP_PATTERN				"48 89 5C 24 08 57 48 83 EC 20 48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8D 1D ? ? ? ? BF ? ? ? ?"
#define CONSOLEPRINT_PATTERN		"48 89 4c 24 08 48 89 54 24 10 4c 89 44 24 18 4c 89 4c 24 20 48 83 ec 28 80 3d 41 ? ? ? ?"
#define COMMANDNAMECHECK_PATTERN	"48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 33 ED 48 8D 1D ? ? ? ?"
#define FULLCHECK_PATTERN			"40 53 56 41 55 48 83 EC 40 48 8B DA 4C 8B E9 48 C7 C6 FF FF FF FF 48 FF C6 80 3C 32 00 75 F7"

uintptr_t base = 0;
uintptr_t _console_print = 0;
uintptr_t _init_commands = 0;
uintptr_t _command_full_check = 0;

OBSEInterface* obse = nullptr;
PluginHandle plugin_handle;

int silentCount = 0;

typedef char(__fastcall* CommandFullFunc)(void* a1, char* a2);
CommandFullFunc pCommandFullFunc = nullptr;
CommandFullFunc pCommandFullFuncTarget;

typedef void(*HelpCommand)();
HelpCommand pHelpCommand = nullptr;
HelpCommand pHelpCommandTarget;

typedef void(*ConsolePrint_t)(const char*, ...);
ConsolePrint_t pConsolePrint = nullptr;
ConsolePrint_t pConsolePrintTarget;

template <class... Args>
void ConsolePrint(const char* fmt, Args... args)
{
	if (_console_print == 0)
		return;

	using func_t = void(*)(const char*, ...);
	auto func = reinterpret_cast<func_t>(_console_print);
	return func(fmt, args...);
}

void detourConsolePrint(const char* fmt, ...) {
	char buffer[2048];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	char filtered[2048];
	char* out = filtered;
	for (const unsigned char* in = (unsigned char*)buffer; *in && (out - filtered) < (int)sizeof(filtered) - 1; ++in)
		if (isprint(*in) || *in == '\n' || *in == '\r' || *in == '\t')
			*out++ = *in;

	*out = 0;
	if (!memcmp("Script", filtered, 6) && silentCount > 0) {
		silentCount--;
		return;
	}

	pConsolePrintTarget("%s", filtered);
}

void detourHelpCommand() {
	pHelpCommandTarget();
	for (ConsoleExt::Group* group : ConsoleExt::groups) {
		ConsolePrint("----%s-------------------------", group->name);
		ConsoleExt::Command* cmd = group->start;
		while (cmd) {
			printf("%s: %p %p %p\n", cmd->name, cmd->name, cmd->short_name, cmd->help_string);
			if (cmd->short_name == nullptr && cmd->help_string == nullptr)
				ConsolePrint("%s", cmd->name);
			else if (cmd->short_name != nullptr && cmd->help_string == nullptr)
				ConsolePrint("%s (%s)", cmd->name, cmd->short_name);
			else if (cmd->short_name != nullptr && cmd->help_string != nullptr)
				ConsolePrint("%s (%s) -> %s", cmd->name, cmd->short_name, cmd->help_string);
			cmd = cmd->next;
		}
	}
}

std::string lower_string(const char* str) {
	std::string result = str;
	std::transform(result.begin(), result.end(), result.begin(),
		[](unsigned char c) { return std::tolower(c); });

	return result;
}



char** split_string(char* str, size_t* outCount) {
	if (!str || !outCount)
		return nullptr;

	const size_t maxTokens = MAX_ARGS;
	char** result = (char**)malloc(sizeof(char*) * maxTokens);
	size_t count = 0;

	char* p = str;
	bool skippedFirst = false;

	while (*p && count < maxTokens) {
		while (isspace(static_cast<unsigned char>(*p)))
			++p;

		if (*p == '\0')
			break;

		char* tokenStart = p;

		if (*p == '"') {
			++tokenStart;
			++p;
			while (*p && *p != '"')
				++p;
			if (*p == '"')
				*p++ = '\0';
		}
		else {
			while (*p && !isspace(static_cast<unsigned char>(*p)))
				++p;

			if (*p)
				*p++ = '\0';
		}

		if (!skippedFirst) {
			skippedFirst = true;
		}
		else {
			result[count++] = tokenStart;
		}
	}

	*outCount = count;
	return result;
}


char __fastcall detourCommandFull(char** a1, char* cmdName) {
	char* tmp_fullCmd = *a1;
	
	size_t fullCmd_size = strlen(tmp_fullCmd) + 1;
	char* fullCmd = (char*)malloc(fullCmd_size);
	memcpy(fullCmd, tmp_fullCmd, fullCmd_size);

	size_t argCount = 0;
	char** args = split_string(fullCmd, &argCount);


	for (ConsoleExt::Group* group : ConsoleExt::groups) {
		ConsoleExt::Command* cmd = group->start;
		std::string lower_cmd = lower_string(cmdName);
		while (cmd) {
			std::string cmd_name = lower_string(cmd->name);
			std::string cmd_short = "";

			if (cmd->short_name != nullptr)
				cmd_short = lower_string(cmd->short_name);

			if (lower_cmd.compare(cmd_name) == 0 || lower_cmd.compare(cmd_short) == 0) {
				silentCount = 1;
				if (cmd->execute_function) {
					cmd->execute_function(argCount, args);
				}
				else
					ConsolePrint("%s doesn't have a function.", cmd->name);
				return 0;
			}

			cmd = cmd->next;
		}
	}
	free(args);
	free(fullCmd);

	return pCommandFullFuncTarget(a1, cmdName);
}

void HandleMessage(OBSEMessagingInterface::Message* msg) {
	if (msg->type != ConsoleExt::EventType::Event)
		return;

	ConsoleExt::Packet* packet = (ConsoleExt::Packet*)msg->data;
	if (!packet) {
		printf("[!] failed to get packet\n");
		return;
	}

	if (memcmp(packet->magic, "\xCA\x75\x30", 3) != 0) {
		printf("[!] packet doesn't have proper magic\n");
		return;
	}
	if (!strcmp(packet->name, "print")) {
		printf("error print?\n");
		ConsolePrint("%s", packet->value);
		return;
	}
	if (!strcmp(packet->name, "cmd_create")) {
		ConsoleExt::Command* tmp_cmd = (ConsoleExt::Command*)packet->value;
		ConsoleExt::Command* cmd = (ConsoleExt::Command*)malloc(sizeof(ConsoleExt::Command));

		memcpy(cmd, tmp_cmd, sizeof(ConsoleExt::Command));

		if (cmd->group == nullptr) {
			if (ConsoleExt::groups.size() == 0) {
				ConsoleExt::Group* group = (ConsoleExt::Group*)malloc(sizeof(ConsoleExt::Group));
				group->name = "Mod Commands";

				group->start = cmd;
				group->end = cmd;
				group->id = 0;

				cmd->id = -1;

				ConsoleExt::groups.push_back(group);
				cmd->group = group;

			}
			else cmd->group = ConsoleExt::groups[0];
		}
		else {
			// locate group
			auto& groups = ConsoleExt::groups;
			int targetId = cmd->group->id;

			auto it = std::find_if(groups.begin(), groups.end(),
				[targetId](ConsoleExt::Group* g) {
					return g->id == targetId;
				}
			);
			if (groups.end() != it)
				cmd->group = it[0];
			else printf("[!] couldn't find group!\n");
		}

		tmp_cmd->id = cmd->group->end ? cmd->group->end->id + 1 : 0;
		tmp_cmd->group = cmd->group;
		cmd->id = tmp_cmd->id;

		if (cmd->group) {
			if (!cmd->group->end)
				cmd->group->end = cmd;
			else if (cmd->group->end != cmd) {
				cmd->group->end->next = cmd;
				cmd->prev = cmd->group->end;
			}

			if (!cmd->group->start)
				cmd->group->start = cmd;

			tmp_cmd->next = cmd->next;
			tmp_cmd->prev = cmd->prev;
		}
		else printf("[!] failed to add command to group.\n");
		return;
	}
	else if (!strcmp(packet->name, "cmd_remove")) {
		ConsoleExt::Command* cmd = (ConsoleExt::Command*)packet->value;

		ConsoleExt::Command* rel_cmd = cmd->group->start;
		while (rel_cmd) {
			if (rel_cmd->id == cmd->id) {
				rel_cmd->prev->next = rel_cmd->next;
				rel_cmd->next->prev = rel_cmd->prev;

				cmd->next = nullptr;
				cmd->prev = nullptr;

				free(rel_cmd);
				return;
			}

			rel_cmd = rel_cmd->next;
		}
		return;
	}
	else if (!strcmp(packet->name, "cmd_update")) {
		ConsoleExt::Command* cmd = (ConsoleExt::Command*)packet->value;

		ConsoleExt::Command* rel_cmd = cmd->group->start;
		while (rel_cmd) {
			if (rel_cmd->id == cmd->id) {
				memcpy(rel_cmd, cmd, packet->size);
				return;
			}
			rel_cmd = rel_cmd->next;
		}

		return;
	}
	else if (!strcmp(packet->name, "group_create")) {
		ConsoleExt::Group* tmp_group = (ConsoleExt::Group*)packet->value;
		ConsoleExt::Group* group = (ConsoleExt::Group*)malloc(sizeof(ConsoleExt::Group));

		tmp_group->id = ConsoleExt::groups.size();

		memcpy(group, tmp_group, sizeof(ConsoleExt::Group));

		ConsoleExt::groups.push_back(group);

		return;
	}
	else if (!strcmp(packet->name, "group_remove")) {
		ConsoleExt::Group* group = (ConsoleExt::Group*)packet->value;

		auto& cmd = group->start;
		while (cmd) {
			auto& next = cmd->next;
			free(cmd);
			cmd = next;
		}

		group->start = nullptr;
		group->end = nullptr;

		int targetId = group->id;

		auto& groups = ConsoleExt::groups;
		auto it = std::find_if(groups.begin(), groups.end(),
			[targetId](ConsoleExt::Group* g) {
				return g->id == targetId;
			}
		);

		if (it != groups.end()) {
			delete* it;
			groups.erase(it);
		}

		return;
	}
	else if (!strcmp(packet->name, "group_update")) {
		ConsoleExt::Group* group = (ConsoleExt::Group*)packet->value;

		auto& groups = ConsoleExt::groups;
		int targetId = group->id;

		auto it = std::find_if(groups.begin(), groups.end(),
			[targetId](ConsoleExt::Group* g) {
				return g->id == targetId;
			}
		);

		ConsoleExt::Group* rel_group = it[0];
		rel_group->name = group->name;

		return;
	}
}

void VersionOutput(int argc, char** argv) {
	ConsolePrint("%s", CONSOLEEXT_VERSION);
	for (int i = 0; i < argc; i++)
		printf("%s\n", argv[i]);
	//for (char* arg : args)
		//printf("%s\n", arg);
}

void LoadPlugin() {
	OBSEMessagingInterface* msgInterface = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);

	ConsoleExt::Init(handle, msgInterface);
	ConsoleExt::Command cmd;

	cmd.name = "ConsoleExtVersion";
	cmd.short_name = "cxv";
	cmd.help_string = "Check what current version ConsoleExt is.";
	cmd.execute_function = VersionOutput;

	ConsoleExt::CreateCommand(&cmd);

	msgInterface->Dispatch(handle, ConsoleExt::EventType::Load, nullptr, 0, nullptr);
}

void CoreMessage(OBSEMessagingInterface::Message* msg) {
	if (msg->type == OBSEMessagingInterface::kMessage_PostPostLoad)
		LoadPlugin();
	else HandleMessage(msg);
}

extern "C" {
	__declspec(dllexport) OBSEPluginVersionData OBSEPlugin_Version =
	{
		OBSEPluginVersionData::kVersion,

		1,
		"ConsoleExt",
		"chonker",

		OBSEPluginVersionData::kAddressIndependence_Signatures,
		OBSEPluginVersionData::kStructureIndependence_NoStructs,

		{ RUNTIME_VERSION_0_411_140, 0 },

		0,
		0, 0, 0
	};

	__declspec(dllexport) bool OBSEPlugin_Load(const OBSEInterface* _obse) {
		obse = (OBSEInterface*)_obse;
		FILE* pFile = nullptr;
		AllocConsole();
		freopen_s(&pFile, "CONOUT$", "w", stdout);

		base = (uintptr_t)GetModuleHandle(NULL);
		printf("[*] base: %p\n", (void*)base);

		if (MH_Initialize() != MH_OK) {
			printf("[!] Minhook failed to initialize.\n");
			return false;
		}

		printf("[?] scanning for help function..\n");
		_init_commands = (uintptr_t)PatternScan((void*)base, HELP_PATTERN);
		pHelpCommand = (HelpCommand)_init_commands;
		printf("[*] hooking help function.\n");

		if (MH_CreateHook(pHelpCommand, &detourHelpCommand, reinterpret_cast<LPVOID*>(&pHelpCommandTarget)) != MH_OK)
			printf("[!] failed to hook help func\n");

		if (MH_EnableHook(pHelpCommand) != MH_OK)
			printf("[!] failed to enable hook\n");
		else printf("[*] successfully hooked!\n");

		printf("[*] helpFunc: %p\n", (void*)_init_commands);

		printf("[?] scanning for consolePrint function..\n");
		_console_print = (uintptr_t)PatternScan((void*)base, CONSOLEPRINT_PATTERN);
		pConsolePrint = (ConsolePrint_t)_console_print;
		printf("[*] hooking consoleprint function.\n");

		if (MH_CreateHook(pConsolePrint, &detourConsolePrint, reinterpret_cast<LPVOID*>(&pConsolePrintTarget)) != MH_OK)
			printf("[!] failed to hook consoleprint func\n");


		if (MH_EnableHook(pConsolePrint) != MH_OK)
			printf("[!] failed to enable hook\n");

		else printf("[*] successfully hooked!\n");
		printf("[*] consolePrint: %p\n", (void*)_console_print);

		printf("[?] scanning for cmd full check function..\n");
		_command_full_check = (uintptr_t)PatternScan((void*)base, FULLCHECK_PATTERN);
		pCommandFullFunc = (CommandFullFunc)_command_full_check;
		printf("[*] hooking command full check function.\n");

		if (MH_CreateHook(pCommandFullFunc, &detourCommandFull, reinterpret_cast<LPVOID*>(&pCommandFullFuncTarget)) != MH_OK)
			printf("[!] failed to hook command full check func\n");

		if (MH_EnableHook(pCommandFullFunc) != MH_OK)
			printf("[!] failed to enable hook\n");
		else printf("[*] successfully hooked!\n");

		printf("[*] cmdFullCheck: %p\n", (void*)_command_full_check);

		OBSEMessagingInterface* msgInterface = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);
		plugin_handle = obse->GetPluginHandle();

		msgInterface->RegisterListener(plugin_handle, "OBSE", CoreMessage);
		msgInterface->RegisterListener(plugin_handle, nullptr, HandleMessage);

		return true;
	}
}
