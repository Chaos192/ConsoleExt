This guide will just show you how to setup and add commands and groups.
To initialize ConsoleExt you'll need to feed the `ConsoleExt::Init` function the plugin's handle and the `OBSEMessagingInterface`.
Then you'll need to wait for a message ith the type of `ConsoleExt::EventType::Load` to safely create commands.
After getting that you'll need to create the commmands and groups then use the `ConsoleExt::CreateGroup` and `ConsoleExt::CreateCommand` function for the ConsoleExt plugin to create them the copy you have will be a fake and you'll need to use the `ConsoleExt::UpdateCommand` function when you want to edit them.

# Initializing
To setup ConsoleExt you'll first need to run the `ConsoleExt::Init` fuction with a messageInteraface and the plugin handle.
```cpp
OBSEMessagingInterface* msgInterface = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);;
PluginHandle handle = obse->GetPluginHandle();

ConsoleExt::Init(handle, msgInterface);
```

# Creating a command
In order to safely create a command you'll need to wait for the message with the type of `ConsoleExt::EventType::Load`.
```cpp
if (msg->type == ConsoleExt::EventType::Load)
	CreateCommands();
```
Then to create the commands you'll need to create a `ConsoleExt::Command` template that'll be sent to the plugin and copied.
```cpp
// .. this is outside of the function CreateCommands
void ExampleFunction(std::vector<char*> args) {
	for (char* arg : args)
		ConsoleExt::Print("%s", arg);

	ConsoleExt::Print("Example function ran!");
}

// ... this is inside of the function CreateCommands for reference above.
ConsoleExt::Command cmd;

cmd.name = "example";
cmd.short_name = "ex";
cmd.help_string = "This is an example help string.";
cmd.execute_function = ExampleFunction;

// Possible errors to look out for.
ConsoleExt::Error err = ConsoleExt::CreateCommand(&cmd);
if (err != ConsoleExt::Error::None) {
    printf("[!] failed to create command\n");
    if (err == ConsoleExt::Error::NoClient)
		printf("[!] no client\n");
    else if (err == ConsoleExt::Error::NoSender)
		printf("[!] no sender\n");
    return;
}
```

# Updating a command
If you change anything from the cmd you made you'll notice that nothing changes when you try to run the command.
That's because it's just the template that the plugin copied from you to perserve values.

To change them you'll need to run the `ConsoleExt::UpdateCommand` function to do so.
```cpp
cmd.name = "update_example";
ConsoleExt::UpdateCommand(&cmd);
```

# Creating groups
If you want to create your own group so in the help message it splits off from other commands you'll need to do this.
```cpp
// .. this is outside of the function CreateCommands
void ExampleFunction(std::vector<char*> args) {
	for (char* arg : args)
		ConsoleExt::Print("%s", arg);

	ConsoleExt::Print("Example function ran!");
}
// ... this is inside of the function CreateCommands for reference above.
ConsoleExt::Group group;
group.name = "Example Group";

ConsoleExt::Command cmd;

cmd.name = "example";
cmd.short_name = "ex";
cmd.help_string = "This is an example help string.";
cmd.execute_function = ExampleFunction;
cmd.group = &group;

ConsoleExt::CreateGroup(&group);
ConsoleExt::CreateCommand(&cmd);
```

## Full example
```cpp
void ExampleFunction(std::vector<char*> args) {
	for (char* arg : args)
		ConsoleExt::Print("%s", arg);

	ConsoleExt::Print("Example function ran!");
}

void CreateCommands() {
	// Create first command with no group
	ConsoleExt::Command cmd;

	cmd.name = "example";
	cmd.short_name = "ex";
	cmd.help_string = "This is an example help string.";
	cmd.execute_function = ExampleFunction;

    // Possible errors to look out for.
	ConsoleExt::Error err = ConsoleExt::CreateCommand(&cmd);
    
	if (err != ConsoleExt::Error::None) {
		printf("[!] failed to create command\n");
		if (err == ConsoleExt::Error::NoClient)
			printf("[!] no client\n");
		else if (err == ConsoleExt::Error::NoSender)
			printf("[!] no sender\n");
		return;
	}

    // Update the name
    cmd.name = "update_example";
    ConsoleExt::UpdateCommand(&cmd);

    // Create command and group
	ConsoleExt::Group group;
	group.name = "Example Group";

	ConsoleExt::Command cmd1;
	cmd1.name = "example1";
	cmd1.short_name = "ex1";
	cmd1.group = &group;

	ConsoleExt::CreateGroup(&group);
	ConsoleExt::CreateCommand(&cmd1);
}

void MessageHandler(OBSEMessagingInterface::Message* msg) {
    // Handle the EventType::Load message.
	if (msg->type == ConsoleExt::EventType::Load)
		CreateCommands();
}

extern "C" {
    __declspec(dllexport) bool OBSEPlugin_Load(const OBSEInterface* obse) {
        OBSEMessagingInterface* msgInterface = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);;
        PluginHandle handle = obse->GetPluginHandle();

        ConsoleExt::Init(handle, msgInterface);
        msgInterface->RegisterListener(handle, "OBSE", MessageHandler);

        return true;
    }
}
```
