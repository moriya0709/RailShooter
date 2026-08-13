#include <Windows.h>
#include "M_Framework.h"
#include "Game.h"

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

	M_Framework* game = new Game();
	
	game->Run();

	delete game;

	return 0;

}