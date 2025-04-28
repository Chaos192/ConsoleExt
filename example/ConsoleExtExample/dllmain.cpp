#include "PluginApi.h"
#include "obse64_version.h"
#include "ConsoleExt.h"

void ExampleFunction(std::vector<char*> args) {
	for (char* arg : args)
		ConsoleExt::Print("%s", arg);

	ConsoleExt::Print("Example function ran!");
}

void CreateCommands() {
	printf("[*] Loaded example!\n");
	ConsoleExt::Command cmd;

	cmd.name = "example";
	cmd.short_name = "ex";
	cmd.help_string = "This is an example help string.";
	cmd.execute_function = ExampleFunction;

	ConsoleExt::Error err = ConsoleExt::CreateCommand(&cmd);
	if (err != ConsoleExt::Error::None) {
		printf("[!] failed to create command\n");
		if (err == ConsoleExt::Error::NoClient)
			printf("[!] no client\n");
		else if (err == ConsoleExt::Error::NoSender)
			printf("[!] no sender\n");
	}

	ConsoleExt::Group group;
	group.name = "Example Group";

	ConsoleExt::Command cmd1;
	cmd1.name = "example1";
	cmd1.short_name = "ex1";
	cmd1.group = &group;

	ConsoleExt::CreateGroup(&group);
	ConsoleExt::CreateCommand(&cmd1);

	cmd1.name = "update_example1";
	ConsoleExt::UpdateCommand(&cmd1);

	ConsoleExt::Group group2;
	group2.name = "Example Group 2";

	ConsoleExt::Command cmd2;
	cmd2.name = "example2";
	cmd2.short_name = "ex2";
	cmd2.group = &group2;

	ConsoleExt::CreateGroup(&group2);
	ConsoleExt::CreateCommand(&cmd2);
}

void MessageHandler(OBSEMessagingInterface::Message* msg) {
	if (msg->type == ConsoleExt::EventType::Load)
		CreateCommands();
}

extern "C" {
	__declspec(dllexport) OBSEPluginVersionData OBSEPlugin_Version =
	{
		OBSEPluginVersionData::kVersion,
		1,
		"OBSE Test",
		"chonker",
		0,
		0,
		{ RUNTIME_VERSION_0_411_140, 0 },
		0,
		0, 0, 0
	};

	__declspec(dllexport) bool OBSEPlugin_Load(const OBSEInterface* obse) {
		OBSEMessagingInterface* msgInterface = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);;
		PluginHandle handle = obse->GetPluginHandle();

		ConsoleExt::Init(handle, msgInterface);
		msgInterface->RegisterListener(handle, "OBSE", MessageHandler);

		return true;
	}
}