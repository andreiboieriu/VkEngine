#include "editor_app.h"

int main(int argc, char* argv[])
{
	EditorApp app;

	app.init();
	app.run();
	app.cleanup();

	return 0;
}
