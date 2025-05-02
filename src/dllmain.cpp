#include <cstring>
#include <Windows.h>
#include <iostream>
#include <vector>
#include <algorithm>

#include <PluginAPI.h>
#include <obse64_version.h>
#include <utils.h>
#include <ConsoleExt.h>

#include <MinHook.h>
#if _WIN64
#pragma comment(lib, "libMinHook.x64.lib")
#else
#pragma comment(lib, "libMinHook.x86.lib")
#endif

#include "main.h"

OBSEInterface* obse = nullptr;
PluginHandle plugin_handle;

typedef void(__fastcall* ExecuteCommand_t)(__int64, __int64, const std::string&);
ExecuteCommand_t pExecuteCommand = nullptr;
ExecuteCommand_t pExecuteCommandTarget;

void ConsoleOutputFlag(uint8_t val) {
	if (_console_print_flag == 0) {
		if (_execute_cmd == 0)
			return;

		uintptr_t openIns = (uintptr_t)PatternScan((void*)base, PRINT_FLAG_PATTERN, _execute_cmd);
		uint32_t disp = *(uint32_t*)(openIns + 2);
		_console_print_flag = disp + openIns + 7;

	}

	*(char*)_console_print_flag = val;
}

template <class... Args>
void ConsolePrint(const char* fmt, Args... args)
{
	if (_console_print == 0)
		return;

	using func_t = void(*)(const char*, ...);
	auto func = reinterpret_cast<func_t>(_console_print);
	return func(fmt, args...);
}

void __fastcall detourCommandExecute(__int64 a1, __int64 a2, const std::string& cmd) {
	ConsoleOutputFlag(1);
	size_t argCount = 0;
	char* cmdName;
	char* cmdCopy;
	char** args = parse_arguments(cmd.c_str(), &argCount, &cmdName, &cmdCopy);
	
	if (!strcmp(cmdName, "help")) {
		pExecuteCommandTarget(a1, a2, cmd);

		ConsoleOutputFlag(1);
		for (ConsoleExt::Group* group : ConsoleExt::groups) {
			ConsolePrint("----%s-------------------------", group->name);
			ConsoleExt::Command* cmd = group->start;
			while (cmd) {
				if (cmd->short_name == nullptr && cmd->help_string == nullptr)
					ConsolePrint("%s", cmd->name);
				else if (cmd->short_name != nullptr && cmd->help_string == nullptr)
					ConsolePrint("%s (%s)", cmd->name, cmd->short_name);
				else if (cmd->short_name != nullptr && cmd->help_string != nullptr)
					ConsolePrint("%s (%s) -> %s", cmd->name, cmd->short_name, cmd->help_string);
				cmd = cmd->next;
			}
		}

		free(args);
		free(cmdCopy);
		ConsoleOutputFlag(0);
		return;
	}

	for (ConsoleExt::Group* group : ConsoleExt::groups) {
		ConsoleExt::Command* cmd = group->start;
		std::string lower_cmd = lower_string(cmdName);
		while (cmd) {
			std::string cmd_name = lower_string(cmd->name);
			std::string cmd_short = "";

			if (cmd->short_name != nullptr)
				cmd_short = lower_string(cmd->short_name);

			if (lower_cmd.compare(cmd_name) == 0 || lower_cmd.compare(cmd_short) == 0) {
				if (cmd->execute_function)
					cmd->execute_function(argCount, args);
				else
					ConsolePrint("%s doesn't have a function.", cmd->name);
				free(args);
				free(cmdCopy);
				ConsoleOutputFlag(0);
				return;
			}

			cmd = cmd->next;
		}
	}

	free(args);
	free(cmdCopy);

	//ConsoleOutputFlag(0);
	// huh this^ was causing the player.additem crash???

	pExecuteCommandTarget(a1, a2, cmd);
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
	if (!strcmp(packet->name, "cmd_execute")) {
		pExecuteCommand(0, 0, std::string((char*)packet->value));
		return;
	}
	if (!strcmp(packet->name, "print")) {
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

		printf("[?] scanning for consolePrint function..\n");
		_console_print = (uintptr_t)PatternScan((void*)base, CONSOLEPRINT_PATTERN);
		printf("[*] consolePrint: %p\n", (void*)_console_print);

		printf("[?] scanning for cmd execute function..\n");
		_execute_cmd = (uintptr_t)PatternScan((void*)base, EXECUTECMD_PATTERN);
		pExecuteCommand = (ExecuteCommand_t)_execute_cmd;
		printf("[*] hooking command execute function.\n");

		if (MH_CreateHook(pExecuteCommand, &detourCommandExecute, reinterpret_cast<LPVOID*>(&pExecuteCommandTarget)) != MH_OK)
			printf("[!] failed to hook\n");


		if (MH_EnableHook(pExecuteCommand) != MH_OK)
			printf("[!] failed to enable hook\n");

		else printf("[*] successfully hooked!\n");
		printf("[*] cmdExecute: %p\n", (void*)_execute_cmd);

		OBSEMessagingInterface* msgInterface = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);
		plugin_handle = obse->GetPluginHandle();

		msgInterface->RegisterListener(plugin_handle, "OBSE", CoreMessage);
		msgInterface->RegisterListener(plugin_handle, nullptr, HandleMessage);

		return true;
	}
}
