#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <conio.h>
#include <stdlib.h>
#include <Windows.h>
#include "PoolAllocator.h"

using namespace std;

// 型定義
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

// Gameクラス定義
class Game
{
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

// ===========================

Game::Game(int size, int bombs, GameMode mode)
	: size(size), bombs(bombs), gameMode(mode), board(size, vector<Cell*>(size, nullptr)),
	gameState(GameState::INITIALIZING), cursorX(0), cursorY(0), openedCells(0), isFirstOpen(true)
{
}

Game::~Game()
{
	CleanupGame();
}

void Game::Run()
{
	bool keepPlaying = true;
	while (keepPlaying)
	{
		cout << "ゲームモード選択:\n";
		cout << "1: Vanilla: 特殊ルール無し\n"; // V
		cout << "2: Triplet: 地雷は、縦横斜めに3連続に並ばない\n"; // 1T
		cout << "3: Cross:   手がかりの数字は、半径2の十字範囲にある地雷の数を表す\n"; // 1X
		cout << "遊びたいルールの数字を入力してください: ";
		int mode_choice = 0;
		while (mode_choice != '1' && mode_choice != '2' && mode_choice != '3') mode_choice = _getch();
		cout << (char)mode_choice << "\n";

		switch (mode_choice) {
			case '1': this->gameMode = GameMode::VANILLA; break;
			case '2': this->gameMode = GameMode::TRIPLET; break;
			case '3': this->gameMode = GameMode::CROSS; break;
		}

		InitializeGame();
		GameLoop();

		// Ctrl+Cで中断された場合は即座に終了
		if (gameState == GameState::EXITING) break;

		ShowResult();

		cout << "もう一度マインスイーパーを遊びますか? (1: Yes, 0: No): ";
		while (true) {
			int choice = _getch();
			if (choice == '0') {
				cout << "0\n";
				keepPlaying = false;
				break;
			}
			else if (choice == '1') {
				cout << "1\n";
				break;
			}
		}
	}
}

void Game::InitializeGame()
{
	CleanupGame(); // 前のゲームのメモリを解放
	for (int x = 0; x < size; ++x) {
		for (int y = 0; y < size; ++y) {
			board[x][y] = new (cellPool.Alloc()) Cell();
		}
	}
	cursorX = 0;
	cursorY = 0;
	openedCells = 0;
	isFirstOpen = true;
	gameState = GameState::PLAYING;

	system("cls"); // 表示をクリア
}

void Game::CleanupGame()
{
	for (int x = 0; x < size; ++x) {
		for (int y = 0; y < size; ++y) {
			if (board[x][y] != nullptr) {
				cellPool.Free(board[x][y]);
				board[x][y] = nullptr;
			}
		}
	}
}

void Game::GameLoop()
{
	while (gameState == GameState::PLAYING) {
		Render();
		HandleInput();
		Update();
	}
}

void Game::HandleInput()
{
	int key = _getch();

	if (key == 3) { // Ctrl+C
		gameState = GameState::EXITING;
		return;
	}

	if (key == 224) { // 矢印などの特殊キー
		key = _getch();
		switch (key) {
		case 72: if (cursorX > 0) --cursorX; break;        // 上
		case 80: if (cursorX < size - 1) ++cursorX; break; // 下
		case 75: if (cursorY > 0) --cursorY; break;        // 左
		case 77: if (cursorY < size - 1) ++cursorY; break; // 右
		}
	}
	else if (key == ' ') { // Spaceキー
		Cell* currentCell = board[cursorX][cursorY];

		// 旗が立っているマスは開けない
		if (currentCell->isFlagged) {
			return;
		}

		// 初回のマスとその周囲8マスは確定で安全マス
		if (isFirstOpen) {
			PlaceBombs(cursorX, cursorY);
			isFirstOpen = false;
		}

		if (currentCell->state == CellState::BOMB) gameState = GameState::GAME_OVER;
		else OpenCell(cursorX, cursorY);
	}
	else if (key == 'f' || key == 'F') { // Fキーで旗を立てる/外す
		Cell* currentCell = board[cursorX][cursorY];
		if (currentCell->state != CellState::OPENED) {
			currentCell->isFlagged = !currentCell->isFlagged;
		}
	}
}

void Game::Update()
{
	// 開けられるところを全て開ければクリア
	if (gameState == GameState::PLAYING && openedCells == size * size - bombs) gameState = GameState::GAME_CLEAR;
}

void Game::Render()
{
	// コンソールのハンドルを取得し、現在の文字属性を保存
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
	WORD saved_attributes;
	GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
	saved_attributes = consoleInfo.wAttributes;

	// ちかちか (フリッカリング？とか言うらしい) を抑制
	COORD cursorCoord = {0, 0};
	SetConsoleCursorPosition(hConsole, cursorCoord);

	for (int i = 0; i < size; ++i) cout << "==";
	cout << "\n";

	for (int x = 0; x < size; ++x) {
		for (int y = 0; y < size; ++y) {
			Cell* cell = board[x][y];
			bool isCursor = (x == cursorX && y == cursorY);
			bool isFinished = (gameState == GameState::GAME_OVER || gameState == GameState::GAME_CLEAR);

			// 表示する文字を決定
			char charToPrint;
			int number = 0; // ヒントの数字を保持 (色付けのため)
			if (cell->state == CellState::OPENED) {
				int count = CountAdjacentBombs(x, y);
				if (count > 0) {
					charToPrint = count + '0';
					number = count;
				} else {
					charToPrint = ' ';
				}
			}
			else {
				if (cell->isFlagged) charToPrint = '*';
				else if (cell->state == CellState::BOMB && isFinished) charToPrint = 'X';
				else charToPrint = '-';
			}

			// 文字色と背景色を設定
			WORD attributes = saved_attributes;
			if (isCursor) {
				// カーソル位置は背景を白くする
				attributes = (attributes & 0x0F) | BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED;
			}

			if (number > 0) {
				WORD color = 0;
				switch (number) {
					case 1: color = FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;  // 1: 明るい青
					case 2: color = FOREGROUND_GREEN; break;                        // 2: 緑
					case 3: color = FOREGROUND_RED | FOREGROUND_GREEN; break;       // 3: オレンジ (暗い黄)
					case 4: color = FOREGROUND_BLUE; break;                         // 4: 紺色
					case 5: color = FOREGROUND_RED; break;                          // 5: 茶色 (暗い赤)
					case 6: color = FOREGROUND_GREEN | FOREGROUND_BLUE; break;      // 6: シアン (暗い)
					case 7: color = FOREGROUND_RED | FOREGROUND_BLUE; break;        // 7: 黒の代わりにマゼンタ
					case 8: color = FOREGROUND_INTENSITY; break;                    // 8: 灰色
				}
				attributes = (attributes & 0xF0) | color; // 背景色を維持しつつ文字色を設定
			}
			else if (cell->isFlagged) {
				WORD color = FOREGROUND_RED | FOREGROUND_INTENSITY; // 旗: 明るい赤
				attributes = (attributes & 0xF0) | color;
			}

			SetConsoleTextAttribute(hConsole, attributes);
			cout << charToPrint << " ";
			SetConsoleTextAttribute(hConsole, saved_attributes); // 次の描画のために属性を元に戻す
		}
		cout << "\n";
	}

	for (int i = 0; i < size; ++i) cout << "==";
	cout << "\n矢印キー: カーソル移動, Space: マスを開ける, f/F: 旗を立てる, Ctrl+C: 強制終了\n";

	int totalSafeCells = size * size - bombs;
	int remainingSafeCells = totalSafeCells - openedCells;
	// 数字が2桁->1桁になるときに表示が崩れるのを防ぐため、setwで表示幅を固定します。
	// 例: " 9/75" のように、1桁の数字の前にスペースが自動で挿入されます。
	cout << "残り安全マス: " << setw(2) << remainingSafeCells << "/" << totalSafeCells << " \n";
}

void Game::ShowResult()
{
	Render(); // 最終盤面を描画
	if (gameState == GameState::GAME_CLEAR) {
		cout << "CLEAR!\n";
	} else if (gameState == GameState::GAME_OVER) {
		cout << "GAME OVER\n";
	}
}

void Game::PlaceBombs(int firstOpenX, int firstOpenY)
{
	random_device rd;
	mt19937 gen(rd());
	uniform_int_distribution<> dist(0, size - 1);

	for (int i = 0; i < bombs; ++i) {
		int x = dist(gen);
		int y = dist(gen);

		bool isForbidden = (abs(x - firstOpenX) <= 1 && abs(y - firstOpenY) <= 1); // 最初に開けたマスの周囲8マスか
		bool isExistingBomb = board[x][y]->state == CellState::BOMB;			   // 既に設置済みのマス

		bool violatesRule = false;
		if (gameMode == GameMode::TRIPLET) {
			violatesRule = Triplet(x, y);
		}

		if (isForbidden || isExistingBomb || violatesRule) i--; // 置けなかったら再試行
		else board[x][y]->state = CellState::BOMB;
	}
}

void Game::OpenCell(int x, int y)
{
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

int Game::CountAdjacentBombs(int x, int y)
{
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

bool Game::IsBomb(int x, int y) const
{
	if (x < 0 || x >= size || y < 0 || y >= size) {
		return false;
	}
	return board[x][y]->state == CellState::BOMB;
}

bool Game::Triplet(int x, int y) const
{
	// 4方向（横、縦、右下がり、右上がり）をチェック
	const int directions[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};

	for (const auto& dir : directions) {
		int dx = dir[0];
		int dy = dir[1];

		// パターン1: [ここ] - [地雷] - [地雷]
		if (IsBomb(x + dx, y + dy) && IsBomb(x + 2 * dx, y + 2 * dy)) return true;
		// パターン2: [地雷] - [ここ] - [地雷]
		if (IsBomb(x - dx, y - dy) && IsBomb(x + dx, y + dy)) return true;
		// パターン3: [地雷] - [地雷] - [ここ]
		if (IsBomb(x - 2 * dx, y - 2 * dy) && IsBomb(x - dx, y - dy)) return true;
	}
	return false;
}

// =====================================

int main()
{
	// モード選択はRun()メソッド内
	Game minesweeper(10, 25, GameMode::VANILLA); // 初期モードはダミーとして渡す
	minesweeper.Run();
	return 0;
}