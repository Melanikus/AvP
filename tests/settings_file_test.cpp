// The settings file must not be able to point the game at anything. Written 2026-11-05 by Melanikus for the AvP port.
//
// AliensVsPredator.cfg is a text file people share, and anyone can edit it. The game used to take the folder its shaders come from out of that file. Point it at another computer on a
// network, and the game would go and fetch shaders from there and run them on the graphics card. So that setting has gone. This test reads the game's source, because a setting like
// that is easy to add back without anyone noticing: any new setting holding text has to be added to the list below, by someone who has thought about what it does with that text.

#include <stdio.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// The top of the repository. /FC in this test's settings makes __FILE__ the whole path of this file.
static std::filesystem::path Top()
{
	return std::filesystem::path(__FILE__).parent_path().parent_path();
}

static std::string ReadFile(std::filesystem::path const &path)
{
	std::ifstream file(path, std::ios::binary);
	std::stringstream all;
	all << file.rdbuf();
	return all.str();
}

int main()
{
	std::string const render = ReadFile(Top() / "win32" / "d3_func.cpp");
	std::string const utilities = ReadFile(Top() / "3dc" / "utilities.cpp");

	Check(render.find("shaderPath") != std::string::npos, "this test is reading the file that uses the shaders' folder");
	Check(render.find("\"ShaderPath\"") == std::string::npos, "the shaders' folder doesn't come from the settings file");
	Check(render.find("static const std::string shaderPath = \"shaders/\";") != std::string::npos, "it's always the one beside the game");
	Check(utilities.find("\"CommandLine\"") == std::string::npos, "and a command line doesn't come from there either");

	// Every file that reads text from the settings. A number or a yes or no can't name a place, but text can.
	std::set<std::string> const allowed = { "ConfigFile.cpp", "ConfigFile.h" };
	int filesLookedAt = 0;

	for (char const *folder : { "3dc", "win32" })
	{
		for (auto const &entry : std::filesystem::recursive_directory_iterator(Top() / folder))
		{
			std::string const extension = entry.path().extension().string();

			if (!entry.is_regular_file() || (extension != ".cpp" && extension != ".c" && extension != ".h" && extension != ".hpp"))
			{
				continue;
			}

			filesLookedAt++;

			if (ReadFile(entry.path()).find("Config_GetString(") != std::string::npos)
			{
				std::string const name = entry.path().filename().string();

				Check(allowed.count(name) == 1, name + " reads text from the settings: it has to be on this test's list, once someone has checked what it does with that text");
			}
		}
	}

	Check(filesLookedAt > 100, "the search really went through the game's source");

	if (checksFailed)
	{
		printf("FAILED: %d checks\n", checksFailed);
		return 1;
	}

	printf("all settings file checks passed\n");
	return 0;
}
