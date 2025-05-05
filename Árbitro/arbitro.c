#include "../wordgame.h"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>

Game game;

void Cleanup() {
	for (int i = 0; i < game.playerCount; i++)
		if (game.players[i].pipe != INVALID_HANDLE_VALUE) {
			FlushFileBuffers(game.players[i].pipe);
			DisconnectNamedPipe(game.players[i].pipe);
			CloseHandle(game.players[i].pipe);
		}
}

void HandleError(TCHAR* msg) {
	_tprintf_s(_T("[ERROR] %s (%d)\n"), msg, GetLastError());
	Cleanup();
	exit(1);
}

void PlayerConnected(HANDLE hPipe) {
	game.players[game.playerCount].pipe = hPipe;
	game.playerCount++;
}

void WriteMessage(HANDLE hPipe, Message* msg) {
	if (!WriteFile(hPipe, msg, sizeof(Message), NULL, NULL))
		HandleError(_T("WriteFile"));
}

void ReadMessage(HANDLE hPipe, Message* msg) {
	if (!ReadFile(hPipe, msg, sizeof(Message), NULL, NULL))
		HandleError(_T("ReadFile"));
	_tprintf_s(_T("[%s]: '%s'\n"), msg->username, msg->text);
}

void HandlePlayerMessage(Message received, Message* sent) {
	DWORD size = _tcslen(received.text);

	for (int i = 0; i < size; i++)
		sent->text[i] = _totupper(received.text[i]);
	sent->text[size] = '\0';
}

DWORD WINAPI ServerToPlayerThread() {
	Message sent;
	_tcscpy_s(sent.username, USERNAME_SIZE, _T("Server"));

	while (1) {
		_fgetts(sent.text, WORD_SIZE, stdin);
		sent.text[_tcslen(sent.text) - 1] = '\0';

		for (int i = 0; i < game.playerCount; i++)
			WriteMessage(game.players[i].pipe, &sent);

		if (_tcsicmp(sent.text, _T("exit")) == 0) {
			_tprintf_s(_T("Exiting Game...\n"));
			Cleanup();
			exit(0);
		}
	}

	return 0;
}

DWORD WINAPI PlayerListenerThread(Player* player) {
	Message received, sent;
	_tcscpy_s(sent.username, USERNAME_SIZE, _T("Server"));

	while (1) {
		ReadMessage(player->pipe, &received);

		if (_tcsicmp(received.text, _T("exit")) == 0)
			_tprintf_s(_T("%s Left\n"), received.username);

		HandlePlayerMessage(received, &sent);

		WriteMessage(player->pipe, &sent);
	}

	return 0;
}

int _tmain(int argc, TCHAR* argv[]) {
#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif

	_tprintf_s(_T("Server Opened!\n"));

	// Input Thread
	HANDLE hThread = CreateThread(NULL, 0, ServerToPlayerThread, NULL, 0, NULL);
	if (hThread == NULL)
		HandleError(_T("CreateThread"));
	else
		CloseHandle(hThread);

	DWORD threadId;

	while (1) {
		HANDLE hPipe = CreateNamedPipe(
			PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			MAX_PLAYERS, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, 5000, NULL);
		if (hPipe == INVALID_HANDLE_VALUE)
			HandleError(_T("CreateNamedPipe"));

		if (!(ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED)))
			CloseHandle(hPipe);

		PlayerConnected(hPipe);

		HANDLE hThread = CreateThread(NULL, 0, PlayerListenerThread, &game.players[game.playerCount - 1], 0, &threadId);
		if (hThread == NULL)
			HandleError(_T("CreateThread"));
		else
			CloseHandle(hThread);
	}

	return 0;
}