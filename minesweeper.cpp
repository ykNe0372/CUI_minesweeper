#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <stdlib.h>
#include "PoolAllocator.h"

#ifdef _WIN32
#include <conio.h>
#include <Windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <cstdio>
#endif

using namespace std;

enum class CellState {
	CLOSED,
	OPENED,
	BOMB,
};

class Cell {
public:
	CellState state = CellState::CLOSED;
	bool isFlagged = false;
};

enum class InputKey {
	None,
	Up,
	Down,
	Left,
	Right,
	Space,
	Flag,
	Digit1,
	Digit2,
	Digit3,
	Digit0,
	CtrlC
};

enum class GameMode {
	VANILLA,
	TRIPLET,
	CROSS
};

enum class GameState {
	INITIALIZING,
	PLAYING,
	GAME_CLEAR,
	GAME_OVER,
	EXITING
};

class Console {
public:
	static void Init();
	static void Restore();
	static InputKey ReadKey();
	static void Clear();
	static void MoveCursorTop();
	static void SetColor(int fg, int bg = -1);
	static void ResetColor();
};

class Game {
public:
	Game(int size, int bombs, GameMode mode);
	~Game();
	void Run();

private:
	void InitializeGame();
	void CleanupGame();
	void GameLoop();
	void HandleInput();
	void Update();
	void Render();
	void PlaceBombs(int firstClickX, int firstClickY);
	void OpenCell(int x, int y);
	int CountAdjacentBombs(int x, int y);
	bool IsBomb(int x, int y) const;
	bool Triplet(int x, int y) const;
	void ShowResult();

	const int size;
	const int bombs;
	vector<vector<Cell*>> board;
	PoolAllocator<Cell, 100> cellPool;

	GameMode gameMode;

	GameState gameState;
	int cursorX, cursorY;
	int openedCells;
	bool isFirstOpen;
};

// xxxxx----------

#ifndef _WIN32
static termios originalTermios;
void EnableRawMode();
void DisableRawMode();
#endif

void Console::Init() {
#ifndef _WIN32
    EnableRawMode();
#endif
}

void Console::Restore() {
#ifndef _WIN32
    DisableRawMode();
#endif
}

void Console::Clear() {
#ifdef _WIN32
    system("cls");
#else
    cout << "\x1b[2J\x1b[H";
#endif
}

void Console::MoveCursorTop() {
#ifndef _WIN32
    cout << "\x1b[H";
#endif
}

void Console::SetColor(int fg, int bg) {
#ifndef _WIN32
    if (fg < 0 && bg < 0) return;
    cout << "\x1b[";
    bool first = true;
    if (fg >= 0) {
        cout << (30 + fg);
        first = false;
    }
    if (bg >= 0) {
        if (!first) cout << ";";
        cout << (40 + bg);
    }
    cout << "m";
#endif
}

void Console::ResetColor() {
#ifndef _WIN32
    cout << "\x1b[0m";
#endif
}

#ifndef _WIN32

void EnableRawMode() {
	tcgetattr(STDIN_FILENO, &originalTermios);
	termios raw = originalTermios;
	raw.c_lflag &= ~(ECHO | ICANON);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void DisableRawMode() {
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios);
}
#endif

// xxxxx----------

InputKey Console::ReadKey() {
#ifdef _WIN32
	int c = _getch();
	if (c == 3) return InputKey::CtrlC;
	if (c == 224) {		// 特殊キー
		int d = _getch();
		switch (d) {
			case 72: return InputKey::Up;
			case 80: return InputKey::Down;
			case 75: return InputKey::Left;
			case 77: return InputKey::Right;
		}
	}

	switch (c) {
		case 'f': case 'F': return InputKey::Flag;
		case ' ': return InputKey::Space;
		case '1': return InputKey::Digit1;
		case '2': return InputKey::Digit2;
		case '3': return InputKey::Digit3;
		case '0': return InputKey::Digit0;
	}

	return InputKey::None;
#else
	// ANSI エスケープシーケンス
	char buf[3];
	if (read(STDIN_FILENO, &buf[0], 1) != 1) return InputKey::None;
	if (buf[0] == '\x03') return InputKey::CtrlC;
	
	if (buf[0] == '\x1b') {		// ESC
		if (read(STDIN_FILENO, &buf[1], 1) != 1) return InputKey::None;
		if (read(STDIN_FILENO, &buf[2], 1) != 1) return InputKey::None;
		if (buf[1] == '[') {
			switch (buf[2]) {
				case 'A': return InputKey::Up;
				case 'B': return InputKey::Down;
				case 'C': return InputKey::Right;
				case 'D': return InputKey::Left;
			}
		}
		return InputKey::None;
	}

	switch (buf[0]) {
		case 'f': case 'F': return InputKey::Flag;
		case ' ': return InputKey::Space;
		case '1': return InputKey::Digit1;
		case '2': return InputKey::Digit2;
		case '3': return InputKey::Digit3;
		case '0': return InputKey::Digit0;
	}

	return InputKey::None;
#endif
}

// xxxxx----------

Game::Game(int size, int bombs, GameMode mode)
	: size(size), bombs(bombs), gameMode(mode), board(size, vector<Cell*>(size, nullptr)), gameState(GameState::INITIALIZING), cursorX(0), cursorY(0), openedCells(0), isFirstOpen(true) {}

Game::~Game() {
	CleanupGame();
}

void Game::Run() {
	bool keepPlaying = true;
	while (keepPlaying) {
		cout << "ゲームモード選択:\n";
		cout << "1: Vanilla: 特殊ルール無し\n"; 									   // V
		cout << "2: Triplet: 地雷は、縦横斜めに3連続に並ばない\n"; 					   // 1T
		cout << "3: Cross:   手がかりの数字は、半径2の十字範囲にある地雷の数を表す\n"; // 1X
		cout << "遊びたいルールの数字を入力してください: ";
		InputKey mode_choice = InputKey::None;
		while (mode_choice != InputKey::Digit1 && mode_choice != InputKey::Digit2 && mode_choice != InputKey::Digit3) mode_choice = Console::ReadKey();
		
		char mode_char = '1';
		if (mode_choice == InputKey::Digit2) mode_char = '2';
		else if (mode_choice == InputKey::Digit3) mode_char = '3';
		cout << mode_char << "\n";

		switch (mode_choice) {
			case InputKey::Digit1: this->gameMode = GameMode::VANILLA; break;
			case InputKey::Digit2: this->gameMode = GameMode::TRIPLET; break;
			case InputKey::Digit3: this->gameMode = GameMode::CROSS; break;
			default: break;
		}

		InitializeGame();
		GameLoop();

		// Ctrl+Cで中断された場合は即座に終了
		if (gameState == GameState::EXITING) break;

		ShowResult();

		cout << "もう一度マインスイーパーを遊びますか? (1: Yes, 0: No): " << flush;  // バッファに残ったままにならないようにフラッシュ
		while (true) {
			InputKey choice = Console::ReadKey();
			if (choice == InputKey::Digit0) {
				cout << "0\n";
				keepPlaying = false;
				break;
			}
			else if (choice == InputKey::Digit1) {
				cout << "1\n";
				break;
			}
		}
	}
}

void Game::InitializeGame() {
	CleanupGame(); // 前のゲームのメモリを解放
	for (int x = 0; x < size; ++x) {
		for (int y = 0; y < size; ++y) board[x][y] = new (cellPool.Alloc()) Cell();
	}

	cursorX = 0;
	cursorY = 0;
	openedCells = 0;
	isFirstOpen = true;
	gameState = GameState::PLAYING;

	Console::Clear(); // 表示をクリア
}

void Game::CleanupGame() {
	for (int x = 0; x < size; ++x) {
		for (int y = 0; y < size; ++y) {
			if (board[x][y] != nullptr) {
				cellPool.Free(board[x][y]);
				board[x][y] = nullptr;
			}
		}
	}
}

void Game::GameLoop() {
	while (gameState == GameState::PLAYING) {
		Render();
		HandleInput();
		Update();
	}
}

void Game::HandleInput() {
	InputKey key = Console::ReadKey();

	if (key == InputKey::CtrlC) {
		gameState = GameState::EXITING;
		return;
	}

	switch (key) {
	case InputKey::Up: if (cursorX > 0) --cursorX; break;
	case InputKey::Down: if (cursorX < size - 1) ++cursorX; break;
	case InputKey::Left: if (cursorY > 0) --cursorY; break;
	case InputKey::Right: if (cursorY < size - 1) ++cursorY; break;
	case InputKey::Space: {
		Cell* currentCell = board[cursorX][cursorY];

		// 旗が立っているマスは開けない
		if (currentCell->isFlagged) return;

		// 初回のマスとその周囲8マスは確定で安全マス
		if (isFirstOpen) {
			PlaceBombs(cursorX, cursorY);
			isFirstOpen = false;
		}

		if (currentCell->state == CellState::BOMB) gameState = GameState::GAME_OVER;
		else OpenCell(cursorX, cursorY);
		break;
	}
	case InputKey::Flag: {
		Cell* currentCell = board[cursorX][cursorY];
		if (currentCell->state != CellState::OPENED) currentCell->isFlagged = !currentCell->isFlagged;
		break;
	}
	default:
		break;
	}
}


void Game::Update() {
	// 開けられるところを全て開ければクリア
	if (gameState == GameState::PLAYING && openedCells == size * size - bombs) gameState = GameState::GAME_CLEAR;
}

void Game::Render() {
#ifdef _WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);  // 標準出力コンソールのハンドルを取得
	CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	WORD saved_attributes;
	GetConsoleScreenBufferInfo(hConsole, &consoleInfo); // コンソール状態を取得
	saved_attributes = consoleInfo.wAttributes;			// 現在の文字色・背景色などの属性を保存

	// ちかちか (フリッカリング) を抑制
	COORD cursorCoord = {0, 0};
	SetConsoleCursorPosition(hConsole, cursorCoord);

	for (int i = 0; i < size; ++i) cout << "==";
	cout << "\n";

	for (int x = 0; x < size; ++x) {
		for (int y = 0; y < size; ++y) {
			Cell* cell = board[x][y];
			bool isCursor = (x == cursorX && y == cursorY);
			bool isFinished = (gameState == GameState::GAME_OVER || gameState == GameState::GAME_CLEAR);

			char charToPrint;
			int number = 0;
			if (cell->state == CellState::OPENED) {
				int count = CountAdjacentBombs(x, y);
				if (count > 0) {
					charToPrint = count + '0';
					number = count;
				}
				else charToPrint = ' ';
			}
			else {
				if (cell->isFlagged) charToPrint = '*';
				else if (cell->state == CellState::BOMB && isFinished) charToPrint = 'X';
				else charToPrint = '-';
			}

			WORD attributes = saved_attributes;
			if (isCursor) attributes = (attributes & 0x0F) | BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED;

			if (number > 0) {
				WORD color = 0;
				switch (number) {
					case 1: color = FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;                      			   // 青
					case 2: color = FOREGROUND_GREEN; break;                                         				   // 緑
					case 3: color = FOREGROUND_RED | FOREGROUND_GREEN; break;                       				   // 黄
					case 4: color = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;   				   // 紫
					case 5: color = FOREGROUND_RED | FOREGROUND_INTENSITY; break;                    				   // 赤
					case 6: color = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break; 				   // 水色
					case 7: color = FOREGROUND_INTENSITY; break;                                    				   // 灰色
					case 8: color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break; // 白
				}
				attributes = (attributes & 0xF0) | color;
			}
			else if (cell->isFlagged) {
				WORD color = FOREGROUND_RED | FOREGROUND_INTENSITY;
				attributes = (attributes & 0xF0) | color;
			}

			SetConsoleTextAttribute(hConsole, attributes);
			cout << charToPrint << " ";
			SetConsoleTextAttribute(hConsole, saved_attributes);
		}
		cout << "\n";
	}
#else
	Console::MoveCursorTop();

	for (int i = 0; i < size; ++i) cout << "==";
	cout << "\n";

	for (int x = 0; x < size; ++x) {
		for (int y = 0; y < size; ++y) {
			Cell* cell = board[x][y];
			bool isCursor = (x == cursorX && y == cursorY);
			bool isFinished = (gameState == GameState::GAME_OVER || gameState == GameState::GAME_CLEAR);

			char charToPrint;
			int number = 0;
			if (cell->state == CellState::OPENED) {
				int count = CountAdjacentBombs(x, y);
				if (count > 0) {
					charToPrint = count + '0';
					number = count;
				} else {
					charToPrint = ' ';
				}
			} else {
				if (cell->isFlagged) charToPrint = '*';
				else if (cell->state == CellState::BOMB && isFinished) charToPrint = 'X';
				else charToPrint = '-';
			}

			if (isCursor) cout << "\x1b[47m\x1b[30m";    // 背景を白([47m)、文字を黒([30m)
			else if (number > 0) {
				switch (number) {
					case 1: cout << "\x1b[34m"; break;	// 青
					case 2: cout << "\x1b[32m"; break;	// 緑
					case 3: cout << "\x1b[33m"; break;	// 黄色
					case 4: cout << "\x1b[35m"; break;	// 紫
					case 5: cout << "\x1b[31m"; break;	// 赤
					case 6: cout << "\x1b[36m"; break;	// 水色
					case 7: cout << "\x1b[90m"; break;	// 灰色
					case 8: cout << "\x1b[37m"; break;	// 白
				}
			}
			else if (cell->isFlagged) cout << "\x1b[31m";

			cout << charToPrint << " ";
			cout << "\x1b[0m";	// デフォルト
		}
		cout << "\n";
	}
#endif

	for (int i = 0; i < size; ++i) cout << "==";
	cout << "\n矢印キー: カーソル移動 Space: マスを開ける\nf/F: 旗を立てる・外す Ctrl+C: 強制終了\n\n";

	int totalSafeCells = size * size - bombs;
	int remainingSafeCells = totalSafeCells - openedCells;
	cout << "残り安全マス: " << setw(2) << remainingSafeCells << "/" << totalSafeCells << " \n";
}

void Game::ShowResult() {
	Render(); // 最終盤面を描画

	if (gameState == GameState::GAME_CLEAR) cout << "CLEAR!\n";
	else if (gameState == GameState::GAME_OVER) cout << "GAME OVER\n";
}

void Game::PlaceBombs(int firstOpenX, int firstOpenY) {
	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<> dist(0, size - 1);	// 等確率に選ぶ

	for (int i = 0; i < bombs; ++i) {
		int x = dist(gen);
		int y = dist(gen);

		bool isForbidden = (abs(x - firstOpenX) <= 1 && abs(y - firstOpenY) <= 1); // 最初に開けたマスの周囲8マスか
		bool isExistingBomb = board[x][y]->state == CellState::BOMB;			   // 既に設置済みのマス

		bool violatesRule = false;
		if (gameMode == GameMode::TRIPLET) violatesRule = Triplet(x, y);

		if (isForbidden || isExistingBomb || violatesRule) --i; // 置けなかったら再試行
		else board[x][y]->state = CellState::BOMB;
	}
}

void Game::OpenCell(int x, int y) {
	if (x < 0 || x >= size || y < 0 || y >= size || board[x][y]->state != CellState::CLOSED) return;
	board[x][y]->state = CellState::OPENED;
	++openedCells;

	// 0だったら連鎖的に開ける処理
	if (CountAdjacentBombs(x, y) == 0) {
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dy = -1; dy <= 1; ++dy) {
				if (dx == 0 && dy == 0) continue;
				OpenCell(x + dx, y + dy);
			}
		}
	}
}

int Game::CountAdjacentBombs(int x, int y) {
	int count = 0;

	if (gameMode == GameMode::CROSS) {
		// 半径2の十字型8マス
		const int cross_coords[8][2] = {
			{x, y - 2}, {x, y + 2}, {x - 2, y}, {x + 2, y},
			{x, y - 1}, {x, y + 1}, {x - 1, y}, {x + 1, y}
		};
		for (const auto& coord : cross_coords) {
			if (IsBomb(coord[0], coord[1])) ++count;
		}
	}
	else {
		for (int dx = -1; dx <= 1; ++dx) {
			for (int dy = -1; dy <= 1; ++dy) {
				if (dx == 0 && dy == 0) continue;
				int nx = x + dx, ny = y + dy;
				if (nx >= 0 && nx < size && ny >= 0 && ny < size && board[nx][ny]->state == CellState::BOMB) ++count;
			}
		}
	}
	return count;
}

bool Game::IsBomb(int x, int y) const {
	if (x < 0 || x >= size || y < 0 || y >= size) return false;

	return board[x][y]->state == CellState::BOMB;
}

bool Game::Triplet(int x, int y) const {
	// 4方向（横、縦、右下がり、右上がり）をチェック
	const int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

	for (const auto& dir : directions) {
		int dx = dir[0];
		int dy = dir[1];

		if (IsBomb(x + dx, y + dy) && IsBomb(x + 2 * dx, y + 2 * dy)) return true;  // パターン1: [ここ] - [地雷] - [地雷]
		if (IsBomb(x - dx, y - dy) && IsBomb(x + dx, y + dy)) return true;          // パターン2: [地雷] - [ここ] - [地雷]
		if (IsBomb(x - 2 * dx, y - 2 * dy) && IsBomb(x - dx, y - dy)) return true;  // パターン3: [地雷] - [地雷] - [ここ]
	}
	return false;
}

// =====================================

int main() {
	Console::Init();
	Game minesweeper(10, 25, GameMode::VANILLA); // 初期モードはダミーとして渡す
	minesweeper.Run();
	Console::Restore();
	return 0;
}