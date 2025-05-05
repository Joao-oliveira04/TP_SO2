#include "../wordgame.h"
#include <conio.h>
#include <fcntl.h>  
#include <io.h>     
#include <stdio.h>
#include <tchar.h>
#include <windows.h>

Player player;

void UserValidation(int argc, LPTSTR argv[]) {
	if (argc != 2) {
		_tprintf_s(_T("Syntax: %s <username>\n"), argv[0]);
		exit(1);
	}

	DWORD size = _tcslen(argv[1]);
	if (size > USERNAME_SIZE - 1) {
		_tprintf_s(_T("Username is %d characters. Max size is %d characters.\n"), size, USERNAME_SIZE - 1);
		exit(1);
	}

	memset(&player, 0, sizeof(Player));

	_tcsncpy_s(player.username, USERNAME_SIZE, argv[1], USERNAME_SIZE - 1);
	player.username[USERNAME_SIZE - 1] = '\0';
}

void Cleanup() {
	if (player.pipe != INVALID_HANDLE_VALUE)
		CloseHandle(player.pipe);
}

void HandleError(TCHAR* msg) {
	_tprintf_s(_T("[ERROR] %s (%d)\n"), msg, GetLastError());
	Cleanup();
	exit(1);
}

void ConnectToArbitro() {
	_tprintf_s(_T("Waiting Server...\n"));

	while (!WaitNamedPipe(PIPE_NAME, NMPWAIT_WAIT_FOREVER))
		Sleep(1000);

	player.pipe = CreateFile(
		PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (player.pipe == INVALID_HANDLE_VALUE)
		HandleError(_T("CreateFile"));

	DWORD mode = PIPE_READMODE_MESSAGE;
	if (!SetNamedPipeHandleState(player.pipe, &mode, NULL, NULL))
		HandleError(_T("SetNamedPipeHandleState"));

	_tprintf_s(_T("Connected, Welcome %s!\n"), player.username);
}

void WriteMessage(Message* msg) {
	if (!WriteFile(player.pipe, msg, sizeof(Message), NULL, NULL))
		HandleError(_T("WriteFile"));
}

void ReadMessage(Message* msg) {
	if (!ReadFile(player.pipe, msg, sizeof(Message), NULL, NULL))
		HandleError(_T("ReadFile"));
	_tprintf_s(_T("[%s]: '%s'\n"), msg->username, msg->text);
}

void HandleServerMessage(Message msg) {}

DWORD WINAPI ServerListenerThread() {
	Message received;

	while (1) {
		ReadMessage(&received);

		if (_tcsicmp(received.text, _T("exit")) == 0) {
			_tprintf_s(_T("Server Closed The Game...\n"));
			Cleanup();
			exit(0);
		}

		HandleServerMessage(received);
	}

	return 0;
}

int _tmain(int argc, TCHAR* argv[]) {
#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif

	UserValidation(argc, argv);
	ConnectToArbitro();

	// Server Listener Thread
	HANDLE hThread = CreateThread(NULL, 0, ServerListenerThread, NULL, 0, NULL);
	if (hThread == NULL)
		HandleError(_T("CreateThread"));
	else
		CloseHandle(hThread);

	Message sent, received;
	_tcscpy_s(sent.username, USERNAME_SIZE, player.username);

	while (1) {
		_fgetts(sent.text, WORD_SIZE, stdin);
		sent.text[_tcslen(sent.text) - 1] = '\0';

		WriteMessage(&sent);

		if (_tcsicmp(sent.text, _T("exit")) == 0) {
			_tprintf_s(_T("Exiting Game...\n"));
			Cleanup();
			exit(0);
		}
	}

	return 0;
}