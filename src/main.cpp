#include "game_app.h"

int main(int argc, char* argv[])
{
	GameApp engine;

	engine.init();
	engine.run();
	engine.cleanup();

	return 0;
}
