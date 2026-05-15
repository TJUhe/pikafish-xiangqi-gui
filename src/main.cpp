#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kRows = 10;
constexpr int kCols = 9;
constexpr int kWindowWidth = 860;
constexpr int kWindowHeight = 940;
constexpr int kTopBand = 96;
constexpr int kSideMargin = 48;
constexpr int kPieceRadius = 24;
constexpr int kRestartButtonId = 1001;
constexpr int kEngineComboId = 1002;
constexpr int kSearchDepth = 4;
constexpr int kWinScore = 1'000'000;
constexpr UINT kAiMoveMsg = WM_APP + 1;
constexpr UINT kAiMoveReadyMsg = WM_APP + 2;
constexpr DWORD kEngineThinkTimeMs = 500;

enum class Side {
    Red,
    Black
};

enum class PieceType {
    General,
    Advisor,
    Elephant,
    Horse,
    Chariot,
    Cannon,
    Soldier
};

struct Piece {
    PieceType type;
    Side side;
};

struct Position {
    int row = 0;
    int col = 0;
};

struct Move {
    Position from;
    Position to;
};

struct Layout {
    int left = 0;
    int top = 0;
    int cell = 0;
    int width = 0;
    int height = 0;
};

using Cell = std::optional<Piece>;
using Board = std::array<std::array<Cell, kCols>, kRows>;

Board g_board{};
HWND g_restartButton = nullptr;
HWND g_engineCombo = nullptr;
Side g_turn = Side::Red;
bool g_gameOver = false;
bool g_aiPending = false;
bool g_aiThinking = false;
std::optional<Position> g_selected;
std::optional<Position> g_lastMoveFrom;
std::optional<Position> g_lastMoveTo;
std::wstring g_status = L"你执红方，点击棋子开始。";
std::vector<Move> g_gameMoves;
bool g_engineAvailable = false;

enum class EngineKind {
    Pikafish = 0,
    FairyStockfish = 1,
    FairyStockfishNnue = 2
};

struct EngineConfig {
    EngineKind kind;
    const wchar_t* displayName;
    const wchar_t* directoryName;
    const wchar_t* keyword;
    const wchar_t* evalFile;
};

constexpr std::array<EngineConfig, 3> kEngines{{
    {EngineKind::Pikafish, L"Pikafish", L"pikafish", L"pikafish", L"pikafish.nnue"},
    {EngineKind::FairyStockfish, L"Fairy-Stockfish", L"fairy-stockfish", L"fairy", L""},
    {EngineKind::FairyStockfishNnue, L"Fairy-Stockfish-NNUE", L"fairy-stockfish-nnue", L"fairy", L""},
}};

EngineKind g_selectedEngine = EngineKind::Pikafish;

bool CrossedRiver(Position p, Side side);

const EngineConfig& ConfigFor(EngineKind kind) {
    return kEngines[static_cast<std::size_t>(kind)];
}

std::wstring EngineDisplayName(EngineKind kind) {
    return ConfigFor(kind).displayName;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring GetExecutableDirectory() {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    std::wstring path(buffer.data(), length);
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

std::wstring GetParentDirectory(const std::wstring& path) {
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

bool SamePosition(Position a, Position b) {
    return a.row == b.row && a.col == b.col;
}

bool InBounds(Position p) {
    return p.row >= 0 && p.row < kRows && p.col >= 0 && p.col < kCols;
}

Side Opposite(Side side) {
    return side == Side::Red ? Side::Black : Side::Red;
}

int PieceValue(PieceType type) {
    switch (type) {
    case PieceType::General:
        return 10000;
    case PieceType::Chariot:
        return 900;
    case PieceType::Cannon:
        return 450;
    case PieceType::Horse:
        return 400;
    case PieceType::Elephant:
        return 180;
    case PieceType::Advisor:
        return 180;
    case PieceType::Soldier:
        return 90;
    }
    return 0;
}

int AdvancementBonus(Position p, const Piece& piece) {
    if (piece.type != PieceType::Soldier) {
        return 0;
    }

    const int advancedRows = piece.side == Side::Red ? 9 - p.row : p.row;
    return advancedRows * 12 + (CrossedRiver(p, piece.side) ? 50 : 0);
}

const wchar_t* PieceText(const Piece& piece) {
    if (piece.side == Side::Red) {
        switch (piece.type) {
        case PieceType::General:
            return L"帅";
        case PieceType::Advisor:
            return L"仕";
        case PieceType::Elephant:
            return L"相";
        case PieceType::Horse:
            return L"马";
        case PieceType::Chariot:
            return L"车";
        case PieceType::Cannon:
            return L"炮";
        case PieceType::Soldier:
            return L"兵";
        }
    }

    switch (piece.type) {
    case PieceType::General:
        return L"将";
    case PieceType::Advisor:
        return L"士";
    case PieceType::Elephant:
        return L"象";
    case PieceType::Horse:
        return L"马";
    case PieceType::Chariot:
        return L"车";
    case PieceType::Cannon:
        return L"炮";
    case PieceType::Soldier:
        return L"卒";
    }
    return L"?";
}

Layout CalcLayout(const RECT& rc) {
    const int clientW = rc.right - rc.left;
    const int clientH = rc.bottom - rc.top;
    const int usableW = std::max(1, clientW - kSideMargin * 2);
    const int usableH = std::max(1, clientH - kTopBand - 42);
    const int cell = std::max(40, std::min(usableW / (kCols - 1), usableH / (kRows - 1)));
    const int width = cell * (kCols - 1);
    const int height = cell * (kRows - 1);
    return {(clientW - width) / 2, kTopBand + (usableH - height) / 2, cell, width, height};
}

bool InsidePalace(Position p, Side side) {
    if (p.col < 3 || p.col > 5) {
        return false;
    }
    return side == Side::Red ? (p.row >= 7 && p.row <= 9) : (p.row >= 0 && p.row <= 2);
}

bool CrossedRiver(Position p, Side side) {
    return side == Side::Red ? p.row <= 4 : p.row >= 5;
}

int CountBetween(const Board& board, Position from, Position to) {
    if (from.row != to.row && from.col != to.col) {
        return -1;
    }

    int count = 0;
    const int rowStep = (to.row > from.row) - (to.row < from.row);
    const int colStep = (to.col > from.col) - (to.col < from.col);
    Position current{from.row + rowStep, from.col + colStep};
    while (!SamePosition(current, to)) {
        if (board[current.row][current.col]) {
            ++count;
        }
        current.row += rowStep;
        current.col += colStep;
    }
    return count;
}

bool ClearStraightPath(const Board& board, Position from, Position to) {
    return CountBetween(board, from, to) == 0;
}

bool ValidGeneralMove(const Board& board, Position from, Position to, Side side) {
    const Cell& target = board[to.row][to.col];
    if (target && target->type == PieceType::General && target->side != side &&
        from.col == to.col) {
        return ClearStraightPath(board, from, to);
    }

    const int rowDelta = std::abs(to.row - from.row);
    const int colDelta = std::abs(to.col - from.col);
    return InsidePalace(to, side) && rowDelta + colDelta == 1;
}

bool ValidAdvisorMove(Position from, Position to, Side side) {
    return InsidePalace(to, side) && std::abs(to.row - from.row) == 1 &&
           std::abs(to.col - from.col) == 1;
}

bool ValidElephantMove(const Board& board, Position from, Position to, Side side) {
    if (std::abs(to.row - from.row) != 2 || std::abs(to.col - from.col) != 2) {
        return false;
    }
    if ((side == Side::Red && to.row < 5) || (side == Side::Black && to.row > 4)) {
        return false;
    }
    const Position eye{(from.row + to.row) / 2, (from.col + to.col) / 2};
    return !board[eye.row][eye.col];
}

bool ValidHorseMove(const Board& board, Position from, Position to) {
    const int rowDelta = std::abs(to.row - from.row);
    const int colDelta = std::abs(to.col - from.col);
    if (!((rowDelta == 2 && colDelta == 1) || (rowDelta == 1 && colDelta == 2))) {
        return false;
    }

    Position leg = from;
    if (rowDelta == 2) {
        leg.row += (to.row > from.row) ? 1 : -1;
    } else {
        leg.col += (to.col > from.col) ? 1 : -1;
    }
    return !board[leg.row][leg.col];
}

bool ValidChariotMove(const Board& board, Position from, Position to) {
    return (from.row == to.row || from.col == to.col) && ClearStraightPath(board, from, to);
}

bool ValidCannonMove(const Board& board, Position from, Position to) {
    if (from.row != to.row && from.col != to.col) {
        return false;
    }

    const int screens = CountBetween(board, from, to);
    const bool capture = board[to.row][to.col].has_value();
    return capture ? screens == 1 : screens == 0;
}

bool ValidSoldierMove(Position from, Position to, Side side) {
    const int forward = side == Side::Red ? -1 : 1;
    const int rowDelta = to.row - from.row;
    const int colDelta = std::abs(to.col - from.col);
    return (rowDelta == forward && colDelta == 0) ||
           (CrossedRiver(from, side) && rowDelta == 0 && colDelta == 1);
}

bool ValidPieceMove(const Board& board, const Move& move, const Piece& piece) {
    if (!InBounds(move.from) || !InBounds(move.to) || SamePosition(move.from, move.to)) {
        return false;
    }
    const Cell& target = board[move.to.row][move.to.col];
    if (target && target->side == piece.side) {
        return false;
    }

    switch (piece.type) {
    case PieceType::General:
        return ValidGeneralMove(board, move.from, move.to, piece.side);
    case PieceType::Advisor:
        return ValidAdvisorMove(move.from, move.to, piece.side);
    case PieceType::Elephant:
        return ValidElephantMove(board, move.from, move.to, piece.side);
    case PieceType::Horse:
        return ValidHorseMove(board, move.from, move.to);
    case PieceType::Chariot:
        return ValidChariotMove(board, move.from, move.to);
    case PieceType::Cannon:
        return ValidCannonMove(board, move.from, move.to);
    case PieceType::Soldier:
        return ValidSoldierMove(move.from, move.to, piece.side);
    }
    return false;
}

void Put(Board& board, int row, int col, PieceType type, Side side) {
    board[row][col] = Piece{type, side};
}

void ResetGame(HWND hwnd) {
    g_board = {};

    Put(g_board, 0, 0, PieceType::Chariot, Side::Black);
    Put(g_board, 0, 1, PieceType::Horse, Side::Black);
    Put(g_board, 0, 2, PieceType::Elephant, Side::Black);
    Put(g_board, 0, 3, PieceType::Advisor, Side::Black);
    Put(g_board, 0, 4, PieceType::General, Side::Black);
    Put(g_board, 0, 5, PieceType::Advisor, Side::Black);
    Put(g_board, 0, 6, PieceType::Elephant, Side::Black);
    Put(g_board, 0, 7, PieceType::Horse, Side::Black);
    Put(g_board, 0, 8, PieceType::Chariot, Side::Black);
    Put(g_board, 2, 1, PieceType::Cannon, Side::Black);
    Put(g_board, 2, 7, PieceType::Cannon, Side::Black);
    for (int col : {0, 2, 4, 6, 8}) {
        Put(g_board, 3, col, PieceType::Soldier, Side::Black);
    }

    Put(g_board, 9, 0, PieceType::Chariot, Side::Red);
    Put(g_board, 9, 1, PieceType::Horse, Side::Red);
    Put(g_board, 9, 2, PieceType::Elephant, Side::Red);
    Put(g_board, 9, 3, PieceType::Advisor, Side::Red);
    Put(g_board, 9, 4, PieceType::General, Side::Red);
    Put(g_board, 9, 5, PieceType::Advisor, Side::Red);
    Put(g_board, 9, 6, PieceType::Elephant, Side::Red);
    Put(g_board, 9, 7, PieceType::Horse, Side::Red);
    Put(g_board, 9, 8, PieceType::Chariot, Side::Red);
    Put(g_board, 7, 1, PieceType::Cannon, Side::Red);
    Put(g_board, 7, 7, PieceType::Cannon, Side::Red);
    for (int col : {0, 2, 4, 6, 8}) {
        Put(g_board, 6, col, PieceType::Soldier, Side::Red);
    }

    g_turn = Side::Red;
    g_gameOver = false;
    g_aiPending = false;
    g_aiThinking = false;
    g_selected.reset();
    g_lastMoveFrom.reset();
    g_lastMoveTo.reset();
    g_gameMoves.clear();
    g_status = L"你执红方。当前引擎：" + EngineDisplayName(g_selectedEngine);
    SetWindowTextW(hwnd, L"中国象棋 - 轮到你");
    InvalidateRect(hwnd, nullptr, TRUE);
}

std::vector<Move> GenerateLegalMoves(const Board& board, Side side) {
    std::vector<Move> moves;
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            const Cell& source = board[row][col];
            if (!source || source->side != side) {
                continue;
            }

            for (int toRow = 0; toRow < kRows; ++toRow) {
                for (int toCol = 0; toCol < kCols; ++toCol) {
                    Move move{{row, col}, {toRow, toCol}};
                    if (ValidPieceMove(board, move, *source)) {
                        moves.push_back(move);
                    }
                }
            }
        }
    }
    return moves;
}

bool HasGeneral(const Board& board, Side side) {
    for (const auto& row : board) {
        for (const Cell& cell : row) {
            if (cell && cell->side == side && cell->type == PieceType::General) {
                return true;
            }
        }
    }
    return false;
}

std::optional<Position> FindGeneral(const Board& board, Side side) {
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            const Cell& cell = board[row][col];
            if (cell && cell->side == side && cell->type == PieceType::General) {
                return Position{row, col};
            }
        }
    }
    return std::nullopt;
}

bool IsSquareAttacked(const Board& board, Position square, Side attacker) {
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            const Cell& source = board[row][col];
            if (!source || source->side != attacker) {
                continue;
            }
            const Move attack{{row, col}, square};
            if (ValidPieceMove(board, attack, *source)) {
                return true;
            }
        }
    }
    return false;
}

bool IsInCheck(const Board& board, Side side) {
    const std::optional<Position> general = FindGeneral(board, side);
    if (!general) {
        return true;
    }
    return IsSquareAttacked(board, *general, Opposite(side));
}

int PositionalScore(Position p, const Piece& piece) {
    int score = PieceValue(piece.type);
    score += std::max(0, 4 - std::abs(p.col - 4)) * 6;
    score += AdvancementBonus(p, piece);

    if (piece.type == PieceType::Chariot || piece.type == PieceType::Cannon ||
        piece.type == PieceType::Horse) {
        const int mobilityRow = piece.side == Side::Red ? 9 - p.row : p.row;
        score += mobilityRow * 5;
    }

    return score;
}

int EvaluateBoard(const Board& board) {
    if (!HasGeneral(board, Side::Red)) {
        return kWinScore;
    }
    if (!HasGeneral(board, Side::Black)) {
        return -kWinScore;
    }

    int score = 0;
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            const Cell& cell = board[row][col];
            if (!cell) {
                continue;
            }

            const int value = PositionalScore({row, col}, *cell);
            score += cell->side == Side::Black ? value : -value;
        }
    }

    return score;
}

Cell MakeMove(Board& board, const Move& move) {
    Cell captured = board[move.to.row][move.to.col];
    board[move.to.row][move.to.col] = board[move.from.row][move.from.col];
    board[move.from.row][move.from.col].reset();
    return captured;
}

void UndoMove(Board& board, const Move& move, const Cell& captured) {
    board[move.from.row][move.from.col] = board[move.to.row][move.to.col];
    board[move.to.row][move.to.col] = captured;
}

bool IsLegalMove(const Board& board, const Move& move, Side side) {
    const Cell& source = board[move.from.row][move.from.col];
    if (!source || source->side != side || !ValidPieceMove(board, move, *source)) {
        return false;
    }

    Board next = board;
    MakeMove(next, move);
    return !IsInCheck(next, side);
}

std::vector<Move> GenerateStrictLegalMoves(const Board& board, Side side) {
    std::vector<Move> legal;
    std::vector<Move> pseudo = GenerateLegalMoves(board, side);
    legal.reserve(pseudo.size());
    for (const Move& move : pseudo) {
        Board next = board;
        MakeMove(next, move);
        if (!IsInCheck(next, side)) {
            legal.push_back(move);
        }
    }
    return legal;
}

int CaptureScore(const Board& board, const Move& move) {
    const Cell& source = board[move.from.row][move.from.col];
    const Cell& target = board[move.to.row][move.to.col];
    if (!target) {
        return 0;
    }

    const int attackerValue = source ? PieceValue(source->type) : 0;
    return PieceValue(target->type) * 16 - attackerValue;
}

void OrderMoves(const Board& board, std::vector<Move>& moves) {
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        const int captureA = CaptureScore(board, a);
        const int captureB = CaptureScore(board, b);
        if (captureA != captureB) {
            return captureA > captureB;
        }

        const int centerA = std::abs(a.to.col - 4) + std::abs(a.to.row - 5);
        const int centerB = std::abs(b.to.col - 4) + std::abs(b.to.row - 5);
        return centerA < centerB;
    });
}

std::string MoveToUci(const Move& move) {
    std::string text;
    text.push_back(static_cast<char>('a' + move.from.col));
    text.push_back(static_cast<char>('0' + (9 - move.from.row)));
    text.push_back(static_cast<char>('a' + move.to.col));
    text.push_back(static_cast<char>('0' + (9 - move.to.row)));
    return text;
}

std::optional<Move> MoveFromUci(const std::string& text) {
    if (text.size() < 4) {
        return std::nullopt;
    }

    const int fromCol = text[0] - 'a';
    const int fromRank = text[1] - '0';
    const int toCol = text[2] - 'a';
    const int toRank = text[3] - '0';
    Move move{{9 - fromRank, fromCol}, {9 - toRank, toCol}};
    if (!InBounds(move.from) || !InBounds(move.to)) {
        return std::nullopt;
    }
    return move;
}

std::wstring FindEngineExecutable(EngineKind engineKind) {
    const EngineConfig& config = ConfigFor(engineKind);
    const std::wstring engineDir = config.directoryName;
    const std::vector<std::wstring> directories{
        L".\\engines\\" + engineDir,
        L".\\engines\\" + engineDir + L"\\Windows",
        GetExecutableDirectory() + L"\\engines\\" + engineDir,
        GetExecutableDirectory() + L"\\engines\\" + engineDir + L"\\Windows",
        GetExecutableDirectory() + L"\\..\\..\\..\\engines\\" + engineDir,
        GetExecutableDirectory() + L"\\..\\..\\..\\engines\\" + engineDir + L"\\Windows",
    };

    for (const std::wstring& directory : directories) {
        const std::vector<std::wstring> preferredNames{
            std::wstring(config.keyword) + L"-avx2.exe",
            std::wstring(config.keyword) + L"-bmi2.exe",
            std::wstring(config.keyword) + L"-avxvnni.exe",
            std::wstring(config.keyword) + L"-sse41-popcnt.exe",
            std::wstring(config.keyword) + L".exe",
            std::wstring(config.keyword) + L"-modern.exe",
            L"stockfish.exe",
            L"fairy-stockfish.exe",
        };
        for (const std::wstring& name : preferredNames) {
            const std::wstring path = directory + L"\\" + name;
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                return path;
            }
        }

        WIN32_FIND_DATAW data{};
        HANDLE find = FindFirstFileW((directory + L"\\*.exe").c_str(), &data);
        if (find == INVALID_HANDLE_VALUE) {
            continue;
        }

        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                std::wstring name = data.cFileName;
                std::wstring lower = name;
                std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t ch) {
                    return static_cast<wchar_t>(towlower(ch));
                });
                if (lower.find(config.keyword) != std::wstring::npos ||
                    lower.find(L"stockfish") != std::wstring::npos) {
                    FindClose(find);
                    return directory + L"\\" + name;
                }
            }
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }

    return {};
}

struct EngineResult {
    std::optional<Move> move;
    bool engineFound = false;
};

struct AiResult {
    std::optional<Move> move;
    bool engineFound = false;
    EngineKind engineKind = EngineKind::Pikafish;
};

EngineResult QueryEngineMove(EngineKind engineKind, const std::vector<Move>& history) {
    const std::wstring enginePath = FindEngineExecutable(engineKind);
    if (enginePath.empty()) {
        return {};
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE childStdInRead = nullptr;
    HANDLE childStdInWrite = nullptr;
    HANDLE childStdOutRead = nullptr;
    HANDLE childStdOutWrite = nullptr;
    if (!CreatePipe(&childStdInRead, &childStdInWrite, &sa, 0) ||
        !CreatePipe(&childStdOutRead, &childStdOutWrite, &sa, 0)) {
        return {{}, true};
    }
    SetHandleInformation(childStdInWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(childStdOutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = childStdInRead;
    startup.hStdOutput = childStdOutWrite;
    startup.hStdError = childStdOutWrite;

    PROCESS_INFORMATION process{};
    std::wstring commandLine = L"\"" + enginePath + L"\"";
    const std::wstring engineWorkingDirectory = GetParentDirectory(GetParentDirectory(enginePath));
    const BOOL created = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, engineWorkingDirectory.c_str(),
                                        &startup, &process);
    CloseHandle(childStdInRead);
    CloseHandle(childStdOutWrite);
    if (!created) {
        CloseHandle(childStdInWrite);
        CloseHandle(childStdOutRead);
        return {{}, true};
    }

    std::ostringstream commands;
    commands << "uci\n";
    const EngineConfig& config = ConfigFor(engineKind);
    if (engineKind == EngineKind::FairyStockfish ||
        engineKind == EngineKind::FairyStockfishNnue) {
        commands << "setoption name UCI_Variant value xiangqi\n";
    }
    if (config.evalFile[0] != L'\0') {
        commands << "setoption name EvalFile value " << WideToUtf8(config.evalFile) << "\n";
    }
    commands << "isready\n";
    commands << "position startpos";
    if (!history.empty()) {
        commands << " moves";
        for (const Move& move : history) {
            commands << ' ' << MoveToUci(move);
        }
    }
    commands << "\n";
    commands << "go movetime " << kEngineThinkTimeMs << "\n";

    const std::string input = commands.str();
    DWORD written = 0;
    WriteFile(childStdInWrite, input.data(), static_cast<DWORD>(input.size()), &written, nullptr);

    std::string output;
    std::array<char, 512> buffer{};
    const DWORD deadline = GetTickCount() + kEngineThinkTimeMs + 3500;
    while (GetTickCount() < deadline) {
        DWORD available = 0;
        if (!PeekNamedPipe(childStdOutRead, nullptr, 0, nullptr, &available, nullptr)) {
            break;
        }
        if (available == 0) {
            Sleep(10);
            continue;
        }

        DWORD read = 0;
        if (!ReadFile(childStdOutRead, buffer.data(),
                      std::min<DWORD>(available, static_cast<DWORD>(buffer.size())), &read,
                      nullptr) ||
            read == 0) {
            break;
        }
        output.append(buffer.data(), buffer.data() + read);
        if (output.find("bestmove ") != std::string::npos) {
            break;
        }
    }

    const std::string quit = "quit\n";
    WriteFile(childStdInWrite, quit.data(), static_cast<DWORD>(quit.size()), &written, nullptr);
    WaitForSingleObject(process.hProcess, 800);
    TerminateProcess(process.hProcess, 0);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(childStdInWrite);
    CloseHandle(childStdOutRead);

    const std::size_t pos = output.find("bestmove ");
    if (pos == std::string::npos) {
        return {{}, true};
    }

    const std::string best = output.substr(pos + 9, 4);
    return {MoveFromUci(best), true};
}

int AlphaBeta(Board& board, int depth, int alpha, int beta, Side sideToMove) {
    const int staticScore = EvaluateBoard(board);
    if (depth == 0 || std::abs(staticScore) >= kWinScore) {
        return staticScore;
    }

    std::vector<Move> moves = GenerateStrictLegalMoves(board, sideToMove);
    if (moves.empty()) {
        return sideToMove == Side::Black ? -kWinScore + depth : kWinScore - depth;
    }
    OrderMoves(board, moves);

    if (sideToMove == Side::Black) {
        int best = std::numeric_limits<int>::min() / 2;
        for (const Move& move : moves) {
            const Cell captured = MakeMove(board, move);
            const int score = AlphaBeta(board, depth - 1, alpha, beta, Side::Red);
            UndoMove(board, move, captured);

            best = std::max(best, score);
            alpha = std::max(alpha, best);
            if (alpha >= beta) {
                break;
            }
        }
        return best;
    }

    int best = std::numeric_limits<int>::max() / 2;
    for (const Move& move : moves) {
        const Cell captured = MakeMove(board, move);
        const int score = AlphaBeta(board, depth - 1, alpha, beta, Side::Black);
        UndoMove(board, move, captured);

        best = std::min(best, score);
        beta = std::min(beta, best);
        if (alpha >= beta) {
            break;
        }
    }
    return best;
}

std::optional<Move> ChooseAiMove() {
    std::vector<Move> moves = GenerateStrictLegalMoves(g_board, Side::Black);
    if (moves.empty()) {
        return std::nullopt;
    }

    OrderMoves(g_board, moves);

    int bestScore = std::numeric_limits<int>::min() / 2;
    Move bestMove = moves.front();
    int alpha = std::numeric_limits<int>::min() / 2;
    const int beta = std::numeric_limits<int>::max() / 2;

    for (const Move& move : moves) {
        const Cell captured = MakeMove(g_board, move);
        const int score = AlphaBeta(g_board, kSearchDepth - 1, alpha, beta, Side::Red);
        UndoMove(g_board, move, captured);

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
        alpha = std::max(alpha, bestScore);
    }

    return bestMove;
}

std::optional<Move> ChooseFallbackMove(Board& board) {
    std::vector<Move> moves = GenerateStrictLegalMoves(board, Side::Black);
    if (moves.empty()) {
        return std::nullopt;
    }

    OrderMoves(board, moves);

    int bestScore = std::numeric_limits<int>::min() / 2;
    Move bestMove = moves.front();
    int alpha = std::numeric_limits<int>::min() / 2;
    const int beta = std::numeric_limits<int>::max() / 2;

    for (const Move& move : moves) {
        const Cell captured = MakeMove(board, move);
        const int score = AlphaBeta(board, kSearchDepth - 1, alpha, beta, Side::Red);
        UndoMove(board, move, captured);

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
        alpha = std::max(alpha, bestScore);
    }

    return bestMove;
}

AiResult ComputeAiMove(Board board, std::vector<Move> history, EngineKind engineKind) {
    EngineResult engineResult = QueryEngineMove(engineKind, history);
    std::optional<Move> move = engineResult.move;
    if (move && !IsLegalMove(board, *move, Side::Black)) {
        move.reset();
    }
    if (!move) {
        move = ChooseFallbackMove(board);
    }
    return {move, engineResult.engineFound, engineKind};
}

void StartAiThread(HWND hwnd) {
    if (g_aiThinking || g_gameOver || g_turn != Side::Black) {
        return;
    }

    g_aiThinking = true;
    g_aiPending = true;
    const Board boardSnapshot = g_board;
    const std::vector<Move> historySnapshot = g_gameMoves;
    const EngineKind engineSnapshot = g_selectedEngine;
    g_status = L"电脑思考中：" + EngineDisplayName(engineSnapshot);
    SetWindowTextW(hwnd, L"中国象棋 - 电脑思考中");
    InvalidateRect(hwnd, nullptr, TRUE);

    std::thread([hwnd, boardSnapshot, historySnapshot, engineSnapshot] {
        auto* result = new AiResult(ComputeAiMove(boardSnapshot, historySnapshot, engineSnapshot));
        PostMessageW(hwnd, kAiMoveReadyMsg, 0, reinterpret_cast<LPARAM>(result));
    }).detach();
}

bool ApplyMove(HWND hwnd, const Move& move) {
    Cell& source = g_board[move.from.row][move.from.col];
    Cell& target = g_board[move.to.row][move.to.col];
    if (!source || !IsLegalMove(g_board, move, source->side)) {
        return false;
    }

    const bool capturedGeneral = target && target->type == PieceType::General;
    const Side movedSide = source->side;
    const Side opponent = Opposite(movedSide);
    target = source;
    source.reset();
    g_gameMoves.push_back(move);
    g_lastMoveFrom = move.from;
    g_lastMoveTo = move.to;
    g_selected.reset();

    const bool opponentHasNoMoves = !capturedGeneral && GenerateStrictLegalMoves(g_board, opponent).empty();
    if (capturedGeneral || opponentHasNoMoves) {
        g_gameOver = true;
        g_status = movedSide == Side::Red ? L"你赢了！" : L"电脑赢了。";
        SetWindowTextW(hwnd, movedSide == Side::Red ? L"中国象棋 - 你赢了" : L"中国象棋 - 电脑赢了");
    } else {
        g_turn = opponent;
        g_status = g_turn == Side::Red ? L"轮到你，点击棋子走棋。" : L"电脑思考中...";
        SetWindowTextW(hwnd, g_turn == Side::Red ? L"中国象棋 - 轮到你" : L"中国象棋 - 电脑思考中");
    }

    InvalidateRect(hwnd, nullptr, TRUE);
    return true;
}

void ComputerMove(HWND hwnd) {
    if (g_gameOver || g_turn != Side::Black) {
        return;
    }

    StartAiThread(hwnd);
}

void ApplyAiResult(HWND hwnd, AiResult* result) {
    g_aiThinking = false;
    g_aiPending = false;
    if (!result) {
        return;
    }

    g_engineAvailable = result->engineFound;
    const EngineKind usedEngine = result->engineKind;
    std::optional<Move> move = result->move;
    if (move && !IsLegalMove(g_board, *move, Side::Black)) {
        move.reset();
    }

    if (!move || g_gameOver || g_turn != Side::Black) {
        delete result;
        if (g_gameOver || g_turn != Side::Black) {
            return;
        }
        g_gameOver = true;
        g_status = L"电脑无棋可走，你赢了！";
        SetWindowTextW(hwnd, L"中国象棋 - 你赢了");
        InvalidateRect(hwnd, nullptr, TRUE);
        return;
    }

    ApplyMove(hwnd, *move);
    if (!g_gameOver) {
        g_status = result->engineFound ? (EngineDisplayName(usedEngine) + L" 已落子。轮到你。")
                                       : (EngineDisplayName(usedEngine) +
                                          L" 未找到，已用内置搜索。轮到你。");
        InvalidateRect(hwnd, nullptr, TRUE);
    }
    delete result;
}

bool ScreenToPosition(const Layout& layout, int px, int py, Position& pos) {
    const int minX = layout.left - layout.cell / 2;
    const int minY = layout.top - layout.cell / 2;
    const int maxX = layout.left + layout.width + layout.cell / 2;
    const int maxY = layout.top + layout.height + layout.cell / 2;
    if (px < minX || px > maxX || py < minY || py > maxY) {
        return false;
    }

    pos.col = static_cast<int>(std::lround((px - layout.left) / static_cast<double>(layout.cell)));
    pos.row = static_cast<int>(std::lround((py - layout.top) / static_cast<double>(layout.cell)));
    return InBounds(pos);
}

POINT BoardPoint(const Layout& layout, Position p) {
    return POINT{layout.left + p.col * layout.cell, layout.top + p.row * layout.cell};
}

void DrawPalace(HDC hdc, const Layout& layout, int topRow) {
    const POINT a = BoardPoint(layout, {topRow, 3});
    const POINT b = BoardPoint(layout, {topRow + 2, 5});
    const POINT c = BoardPoint(layout, {topRow, 5});
    const POINT d = BoardPoint(layout, {topRow + 2, 3});
    MoveToEx(hdc, a.x, a.y, nullptr);
    LineTo(hdc, b.x, b.y);
    MoveToEx(hdc, c.x, c.y, nullptr);
    LineTo(hdc, d.x, d.y);
}

void DrawPiece(HDC hdc, int cx, int cy, const Piece& piece, bool selected) {
    const COLORREF fill = RGB(246, 220, 160);
    const COLORREF edge = selected ? RGB(25, 120, 210) : RGB(98, 52, 20);
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, selected ? 4 : 2, edge);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    Ellipse(hdc, cx - kPieceRadius, cy - kPieceRadius, cx + kPieceRadius, cy + kPieceRadius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);

    HFONT font = CreateFontW(28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
    HGDIOBJ oldFont = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, piece.side == Side::Red ? RGB(180, 30, 30) : RGB(25, 25, 25));
    RECT textRc{cx - kPieceRadius, cy - kPieceRadius, cx + kPieceRadius, cy + kPieceRadius};
    DrawTextW(hdc, PieceText(piece), -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void PaintWindow(HWND hwnd, HDC hdc) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const Layout layout = CalcLayout(rc);

    HBRUSH bg = CreateSolidBrush(RGB(237, 203, 141));
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    SetBkMode(hdc, TRANSPARENT);
    HFONT titleFont = CreateFontW(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
    HFONT infoFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
    HGDIOBJ oldFont = SelectObject(hdc, titleFont);
    SetTextColor(hdc, RGB(72, 36, 14));
    TextOutW(hdc, 24, 18, L"中国象棋", 4);
    SelectObject(hdc, infoFont);
    TextOutW(hdc, 24, 58, g_status.c_str(), static_cast<int>(g_status.size()));
    const wchar_t* engineLabel = L"电脑引擎";
    TextOutW(hdc, 520, 34, engineLabel, static_cast<int>(lstrlenW(engineLabel)));

    HPEN gridPen = CreatePen(PS_SOLID, 2, RGB(102, 57, 24));
    HGDIOBJ oldPen = SelectObject(hdc, gridPen);

    for (int col = 0; col < kCols; ++col) {
        const POINT top = BoardPoint(layout, {0, col});
        const POINT riverTop = BoardPoint(layout, {4, col});
        const POINT riverBottom = BoardPoint(layout, {5, col});
        const POINT bottom = BoardPoint(layout, {9, col});
        MoveToEx(hdc, top.x, top.y, nullptr);
        LineTo(hdc, riverTop.x, riverTop.y);
        MoveToEx(hdc, riverBottom.x, riverBottom.y, nullptr);
        LineTo(hdc, bottom.x, bottom.y);
    }

    for (int row = 0; row < kRows; ++row) {
        const POINT left = BoardPoint(layout, {row, 0});
        const POINT right = BoardPoint(layout, {row, 8});
        MoveToEx(hdc, left.x, left.y, nullptr);
        LineTo(hdc, right.x, right.y);
    }

    DrawPalace(hdc, layout, 0);
    DrawPalace(hdc, layout, 7);

    HFONT riverFont = CreateFontW(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_ROMAN, L"Microsoft YaHei");
    SelectObject(hdc, riverFont);
    SetTextColor(hdc, RGB(112, 64, 26));
    RECT riverRc{layout.left, BoardPoint(layout, {4, 0}).y + 8,
                 layout.left + layout.width, BoardPoint(layout, {5, 0}).y - 8};
    DrawTextW(hdc, L"楚河        汉界", -1, &riverRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(riverFont);

    if (g_lastMoveTo) {
        const POINT p = BoardPoint(layout, *g_lastMoveTo);
        HPEN markPen = CreatePen(PS_SOLID, 3, RGB(40, 135, 70));
        HGDIOBJ oldMark = SelectObject(hdc, markPen);
        SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(hdc, p.x - kPieceRadius - 4, p.y - kPieceRadius - 4,
                  p.x + kPieceRadius + 4, p.y + kPieceRadius + 4);
        SelectObject(hdc, oldMark);
        DeleteObject(markPen);
    }

    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            if (!g_board[row][col]) {
                continue;
            }
            const Position pos{row, col};
            const POINT p = BoardPoint(layout, pos);
            DrawPiece(hdc, p.x, p.y, *g_board[row][col],
                      g_selected && SamePosition(*g_selected, pos));
        }
    }

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldFont);
    DeleteObject(gridPen);
    DeleteObject(infoFont);
    DeleteObject(titleFont);
}

void HandleClick(HWND hwnd, int x, int y) {
    if (g_gameOver || g_turn != Side::Red || g_aiPending || g_aiThinking) {
        return;
    }

    RECT rc{};
    GetClientRect(hwnd, &rc);
    const Layout layout = CalcLayout(rc);
    Position clicked{};
    if (!ScreenToPosition(layout, x, y, clicked)) {
        return;
    }

    const Cell& clickedCell = g_board[clicked.row][clicked.col];
    if (!g_selected) {
        if (clickedCell && clickedCell->side == Side::Red) {
            g_selected = clicked;
            g_status = L"已选中棋子，点击目标位置。";
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return;
    }

    if (clickedCell && clickedCell->side == Side::Red) {
        g_selected = clicked;
        g_status = L"已切换选中棋子。";
        InvalidateRect(hwnd, nullptr, TRUE);
        return;
    }

    const Move move{*g_selected, clicked};
    if (!IsLegalMove(g_board, move, Side::Red)) {
        g_status = L"这步不合法，不能送将或违反走法。";
        InvalidateRect(hwnd, nullptr, TRUE);
        return;
    }

    if (ApplyMove(hwnd, move) && !g_gameOver) {
        PostMessageW(hwnd, kAiMoveMsg, 0, 0);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        InitCommonControls();
        g_restartButton = CreateWindowW(
            L"BUTTON", L"重新开始", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 712, 28, 112, 34,
            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRestartButtonId)),
            reinterpret_cast<LPCREATESTRUCTW>(lParam)->hInstance, nullptr);
        g_engineCombo = CreateWindowW(
            WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            600, 28, 104, 180, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kEngineComboId)),
            reinterpret_cast<LPCREATESTRUCTW>(lParam)->hInstance, nullptr);
        if (g_engineCombo) {
            for (const EngineConfig& engine : kEngines) {
                SendMessageW(g_engineCombo, CB_ADDSTRING, 0,
                             reinterpret_cast<LPARAM>(engine.displayName));
            }
            SendMessageW(g_engineCombo, CB_SETCURSEL, static_cast<WPARAM>(g_selectedEngine), 0);
        }
        ResetGame(hwnd);
        return 0;
    case WM_SIZE:
        if (g_restartButton) {
            MoveWindow(g_restartButton, LOWORD(lParam) - 136, 24, 112, 34, TRUE);
        }
        if (g_engineCombo) {
            MoveWindow(g_engineCombo, LOWORD(lParam) - 260, 28, 112, 180, TRUE);
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == kRestartButtonId) {
            ResetGame(hwnd);
        } else if (LOWORD(wParam) == kEngineComboId && HIWORD(wParam) == CBN_SELCHANGE) {
            const LRESULT selection = SendMessageW(g_engineCombo, CB_GETCURSEL, 0, 0);
            if (selection >= 0 && selection < static_cast<LRESULT>(kEngines.size())) {
                g_selectedEngine = static_cast<EngineKind>(selection);
                if (!g_aiThinking && !g_aiPending && !g_gameOver) {
                    g_status = L"当前引擎：" + EngineDisplayName(g_selectedEngine) +
                               L"。下一步电脑生效。";
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
        }
        return 0;
    case WM_LBUTTONUP:
        HandleClick(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case kAiMoveMsg:
        ComputerMove(hwnd);
        return 0;
    case kAiMoveReadyMsg:
        ApplyAiResult(hwnd, reinterpret_cast<AiResult*>(lParam));
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = 660;
        info->ptMinTrackSize.y = 760;
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintWindow(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    const wchar_t kClassName[] = L"XiangqiDesktopWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"窗口类注册失败。", L"中国象棋", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, kClassName, L"中国象棋", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, kWindowWidth, kWindowHeight,
                                nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) {
        MessageBoxW(nullptr, L"窗口创建失败。", L"中国象棋", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
