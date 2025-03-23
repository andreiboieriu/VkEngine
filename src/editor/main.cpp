#include "editor_app.h"

int main(int argc, char* argv[])
{
    std::vector<std::string> cliArgs(argv + 1, argv + argc);

	EditorApp app;

	app.init(cliArgs);
	app.run();
	app.cleanup();

	return 0;
}
