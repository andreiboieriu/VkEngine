#include "game_app.h"

int main(int argc, char* argv[])
{
	GameApp app;

	app.init();
	app.run();
	app.cleanup();

	return 0;
}
