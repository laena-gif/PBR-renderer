#include <PCH.h>
#include "Application.h"

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nShowCmd)
{
	wchar_t exePathFull[MAX_PATH];
	GetModuleFileName(nullptr, exePathFull, MAX_PATH);

	std::filesystem::path exePath{ exePathFull };
	std::filesystem::current_path(exePath.remove_filename());

	Application app;
	app.Init(2560, 1440);
	app.Run();
	return 0;
}