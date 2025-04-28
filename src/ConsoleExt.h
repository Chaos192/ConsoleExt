#pragma once

#include <stdbool.h>
#include <iostream>
#include <Windows.h>
#include <vector>

#include "PluginAPI.h"

#define CONSOLEEXT_VERSION "v1.1.0b"

inline namespace ConsoleExt {
	typedef struct CommandParam {

	};

	typedef struct Command {
		const char* name;
		const char* short_name = nullptr;
		const char* help_string = nullptr;
		void(*execute_function)(int, char**) = nullptr;

		int id;
		struct Group* group = nullptr;
		struct Command* next = nullptr;
		struct Command* prev = nullptr;
	};

	typedef struct Group {
		const char* name;

		int id;
		struct Command* start = nullptr;
		struct Command* end = nullptr;
	};

	typedef enum Error {
		None = 0,
		NoClient = 1,
		NoSender = 2
	};

	typedef struct Packet {
		uint8_t magic[3] = { 0xCA, 0x75, 0x30 };
		const char* name;
		size_t size;
		void* value;
	} Packet;

	typedef struct Response {
		void* value;
		size_t size;
		Error error = Error::None;
	} Response;

	std::vector<Group*> groups = {};
	OBSEMessagingInterface* client = nullptr;
	PluginHandle handle = 0;

	enum EventType {
		Event = 0xC0,
		Load = 0xCF
	};

	void Init(PluginHandle plugin_handle, OBSEMessagingInterface* msg) {
		client = msg;
		handle = plugin_handle;
	}

	Response SendPacket(Packet* packet) {
		Response result;
		result.error = Error::None;

		if (!client) {
			result.error = Error::NoClient;
			return result;
		}

		bool check = client->Dispatch(handle, Event, packet, sizeof(Packet), "ConsoleExt");

		if (!check) {
			result.error = Error::NoSender;
			return result;
		}

		return result;
	}

	Error SendCommandPacket(const char* name, Command* cmd) {
		Packet packet;

		packet.name = name;
		packet.size = sizeof(Command*);
		packet.value = cmd;

		return SendPacket(&packet).error;
	}

	Error SendGroupPacket(const char* name, Group* group) {
		Packet packet;

		packet.name = name;
		packet.size = sizeof(Group);
		packet.value = group;

		return SendPacket(&packet).error;
	}

	void Print(const char* fmt, ...) {
		char buffer[2048];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);

		Packet packet;

		packet.name = "print";
		packet.size = sizeof(buffer);
		packet.value = buffer;

		SendPacket(&packet);
	}

	Error UpdateGroup(Group* group) {
		return SendGroupPacket("group_update", group);
	}

	Error RemoveGroup(Group* group) {
		return SendGroupPacket("group_remove", group);
	}

	Error CreateGroup(Group* group) {
		return SendGroupPacket("group_create", group);
	}

	Error UpdateCommand(Command* cmd) {
		return SendCommandPacket("cmd_update", cmd);
	}

	Error RemoveCommand(Command* cmd) {
		return SendCommandPacket("cmd_remove", cmd);
	}

	Error CreateCommand(Command* cmd) {
		return SendCommandPacket("cmd_create", cmd);
	}
}