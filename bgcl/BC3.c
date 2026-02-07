#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <time.h>
#include <ctype.h>

#define SIZE 5
#define EMPTY '.'
#define GOAT 'G'
#define TIGER 'T'
#define SAVE_FILE_PREFIX "savegame_slot_"
#define MAX_HISTORY 100
#define TIMER_LIMIT 15 // Seconds allowed per turn

// --- Global Variables ---
char board[SIZE][SIZE];
int goatCount = 20;
int goatsOnBoard = 0;
int goatsCaptured = 0;
int gamePlayed = 0; // Used as a turn counter for auto-saves

// --- Undo/Redo Structures ---
typedef struct {
    char board[SIZE][SIZE];
    int goatCount;
    int goatsOnBoard;
    int goatsCaptured;
    int turn; // 0 for Goat, 1 for Tiger
} GameState;

GameState undoStack[MAX_HISTORY];
int undoTop = -1;

GameState redoStack[MAX_HISTORY];
int redoTop = -1;

// --- Function Prototypes ---
void initBoard();
void printBoard();
void saveState(int currentTurn);
int undoMove(int *currentTurn);
int redoMove(int *currentTurn);
int getConsoleWidth();
void printCentered(const char *text);
void printCenteredInline(const char *text);
void enableVirtualTerminalProcessing();
int isValidPosition(int r, int c);
int isAdjacent(int r1, int c1, int r2, int c2);
int isValidMoveGoat(int r1, int c1, int r2, int c2);
int isValidMoveTiger(int r1, int c1, int r2, int c2, int *capturedRow, int *capturedCol);
int areTigersTrapped();

// --- Console Utilities ---

// Enables ANSI colors in Windows CMD (Windows 10+)
void enableVirtualTerminalProcessing() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

int getConsoleWidth() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    int width = 80;
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleScreenBufferInfo(hStdOut, &csbi)) {
        width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
    return width;
}

void printCentered(const char *text) {
    int width = getConsoleWidth();
    int len = strlen(text);
    int padding = (width - len) / 2;
    if (padding < 0) padding = 0;
    for (int i = 0; i < padding; i++) putchar(' ');
    printf("%s\n", text);
}

void printCenteredInline(const char *text) {
    int width = getConsoleWidth();
    int len = strlen(text);
    int padding = (width - len) / 2;
    if (padding < 0) padding = 0;
    for (int i = 0; i < padding; i++) putchar(' ');
    printf("%s", text);
}

void pressEnterToContinue() {
    printCentered("Press Enter to continue...");
    while(getchar() != '\n'); // clear buffer
    getchar();
}

// --- Undo / Redo System ---

void saveState(int currentTurn) {
    if (undoTop < MAX_HISTORY - 1) {
        undoTop++;
        memcpy(undoStack[undoTop].board, board, sizeof(board));
        undoStack[undoTop].goatCount = goatCount;
        undoStack[undoTop].goatsOnBoard = goatsOnBoard;
        undoStack[undoTop].goatsCaptured = goatsCaptured;
        undoStack[undoTop].turn = currentTurn;
    }
    // Clear redo stack whenever a new move is made
    redoTop = -1;
}

int undoMove(int *currentTurn) {
    if (undoTop >= 0) {
        // Push current state to redo stack before restoring old state
        if (redoTop < MAX_HISTORY - 1) {
            redoTop++;
            memcpy(redoStack[redoTop].board, board, sizeof(board));
            redoStack[redoTop].goatCount = goatCount;
            redoStack[redoTop].goatsOnBoard = goatsOnBoard;
            redoStack[redoTop].goatsCaptured = goatsCaptured;
            redoStack[redoTop].turn = *currentTurn;
        }

        // Restore from undo stack
        memcpy(board, undoStack[undoTop].board, sizeof(board));
        goatCount = undoStack[undoTop].goatCount;
        goatsOnBoard = undoStack[undoTop].goatsOnBoard;
        goatsCaptured = undoStack[undoTop].goatsCaptured;
        *currentTurn = undoStack[undoTop].turn;
        undoTop--;
        return 1;
    }
    return 0;
}

int redoMove(int *currentTurn) {
    if (redoTop >= 0) {
        // Push current state to undo stack before restoring redo state
        if (undoTop < MAX_HISTORY - 1) {
            undoTop++;
            memcpy(undoStack[undoTop].board, board, sizeof(board));
            undoStack[undoTop].goatCount = goatCount;
            undoStack[undoTop].goatsOnBoard = goatsOnBoard;
            undoStack[undoTop].goatsCaptured = goatsCaptured;
            undoStack[undoTop].turn = *currentTurn;
        }

        // Restore from redo stack
        memcpy(board, redoStack[redoTop].board, sizeof(board));
        goatCount = redoStack[redoTop].goatCount;
        goatsOnBoard = redoStack[redoTop].goatsOnBoard;
        goatsCaptured = redoStack[redoTop].goatsCaptured;
        *currentTurn = redoStack[redoTop].turn;
        redoTop--;
        return 1;
    }
    return 0;
}

// --- Core Game Logic ---

void initBoard() {
    for (int i = 0; i < SIZE; i++)
        for (int j = 0; j < SIZE; j++)
            board[i][j] = EMPTY;

    board[0][0] = TIGER;
    board[0][SIZE - 1] = TIGER;
    board[SIZE - 1][0] = TIGER;
    board[SIZE - 1][SIZE - 1] = TIGER;

    goatCount = 20;
    goatsCaptured = 0;
    goatsOnBoard = 0;
    undoTop = -1;
    redoTop = -1;
}

int isValidPosition(int r, int c) {
    return (r >= 0 && r < SIZE && c >= 0 && c < SIZE);
}

// Checks if nodes are connected (diagonal movement is valid in Bagh Chal)
int isAdjacent(int r1, int c1, int r2, int c2) {
    int dr = abs(r1 - r2);
    int dc = abs(c1 - c2);
    // Adjacent means distance is 1 in any direction, but not the same spot
    return (dr <= 1 && dc <= 1 && !(dr == 0 && dc == 0));
}

int isValidMoveGoat(int r1, int c1, int r2, int c2) {
    // Source must be a Goat, Dest must be Empty, Must be adjacent
    if (!isValidPosition(r1, c1) || !isValidPosition(r2, c2)) return 0;
    if (board[r1][c1] != GOAT) return 0;
    if (board[r2][c2] != EMPTY) return 0;
    return isAdjacent(r1, c1, r2, c2);
}

// Returns: 0 = Invalid, 1 = Move, 2 = Capture
int isValidMoveTiger(int r1, int c1, int r2, int c2, int *capturedRow, int *capturedCol) {
    if (!isValidPosition(r1, c1) || !isValidPosition(r2, c2)) return 0;
    if (board[r1][c1] != TIGER) return 0;
    if (board[r2][c2] != EMPTY) return 0;

    int dr = r2 - r1;
    int dc = c2 - c1;

    // Simple Move (1 step)
    if (abs(dr) <= 1 && abs(dc) <= 1) {
        return 1;
    }

    // Jump/Capture Move (2 steps)
    // Logic: Must jump exactly 2 squares, and the middle square must contain a GOAT
    if ((abs(dr) == 2 && abs(dc) == 0) || (abs(dr) == 0 && abs(dc) == 2) || (abs(dr) == 2 && abs(dc) == 2)) {
        int midR = r1 + dr / 2;
        int midC = c1 + dc / 2;
        if (board[midR][midC] == GOAT) {
            *capturedRow = midR;
            *capturedCol = midC;
            return 2;
        }
    }

    return 0;
}

int areTigersTrapped() {
    int tigersFound = 0;
    int tigersTrapped = 0;

    // Directions: N, S, E, W, NE, NW, SE, SW
    int dr[] = {-1, 1, 0, 0, -1, -1, 1, 1};
    int dc[] = {0, 0, 1, -1, 1, -1, 1, -1};

    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] == TIGER) {
                tigersFound++;
                int canMove = 0;
                
                // Check all 8 directions for valid move or jump
                for (int i = 0; i < 8; i++) {
                    int r2 = r + dr[i];
                    int c2 = c + dc[i];
                    int r3 = r + (dr[i] * 2);
                    int c3 = c + (dc[i] * 2);

                    // Check 1-step move
                    if (isValidPosition(r2, c2) && board[r2][c2] == EMPTY) {
                        canMove = 1;
                        break;
                    }
                    // Check 2-step jump
                    if (isValidPosition(r3, c3) && board[r3][c3] == EMPTY && board[r2][c2] == GOAT) {
                        canMove = 1;
                        break;
                    }
                }
                if (!canMove) tigersTrapped++;
            }
        }
    }
    // If all 4 tigers are found and all are trapped
    return (tigersFound == 4 && tigersTrapped == 4);
}

// --- Turn Execution Functions ---

// Returns 1 if turn successful, 0 if undo/redo/invalid, -1 for exit
int goatTurn(int *currentTurn) {
    int r1, c1, r2, c2;
    char input[10];
    
    printCentered("--- GOAT'S TURN ---");
    
    if (goatsOnBoard < 20) {
        // Phase 1: Placement
        printCenteredInline("Place Goat (Row Col [1-5]): ");
        
        // Timer Start
        time_t start = time(NULL);
        
        // Handling Input as string to catch commands
        if (scanf("%s", input) != 1) return 0;
        
        if (strcasecmp(input, "u") == 0) { undoMove(currentTurn); return 0; }
        if (strcasecmp(input, "r") == 0) { redoMove(currentTurn); return 0; }
        if (strcasecmp(input, "exit") == 0) return -1;

        r1 = atoi(input) - 1; // Convert to 0-index
        scanf("%d", &c1);
        c1 -= 1;

        // Timer End
        if (difftime(time(NULL), start) > TIMER_LIMIT) {
            printf(" \033[33m(WARNING: You took too long!)\033[0m\n");
        }

        if (isValidPosition(r1, c1) && board[r1][c1] == EMPTY) {
            saveState(*currentTurn); // Save before changing
            board[r1][c1] = GOAT;
            goatsOnBoard++;
            goatCount--;
            return 1;
        } else {
            printCentered("Invalid placement. Spot occupied or out of bounds.");
            return 0;
        }
    } else {
        // Phase 2: Movement
        printCenteredInline("Move Goat (FromRow FromCol ToRow ToCol): ");
        
        time_t start = time(NULL);
        if (scanf("%s", input) != 1) return 0;

        if (strcasecmp(input, "u") == 0) { undoMove(currentTurn); return 0; }
        if (strcasecmp(input, "r") == 0) { redoMove(currentTurn); return 0; }
        if (strcasecmp(input, "exit") == 0) return -1;

        r1 = atoi(input) - 1;
        scanf("%d %d %d", &c1, &r2, &c2);
        c1--; r2--; c2--;

        if (difftime(time(NULL), start) > TIMER_LIMIT) {
            printf(" \033[33m(WARNING: You took too long!)\033[0m\n");
        }

        if (isValidMoveGoat(r1, c1, r2, c2)) {
            saveState(*currentTurn);
            board[r1][c1] = EMPTY;
            board[r2][c2] = GOAT;
            return 1;
        } else {
            printCentered("Invalid move.");
            return 0;
        }
    }
}

int tigerTurn(int *currentTurn) {
    int r1, c1, r2, c2;
    char input[10];
    
    printCentered("--- TIGER'S TURN ---");
    printCenteredInline("Move Tiger (FromRow FromCol ToRow ToCol): ");

    time_t start = time(NULL);
    if (scanf("%s", input) != 1) return 0;

    if (strcasecmp(input, "u") == 0) { undoMove(currentTurn); return 0; }
    if (strcasecmp(input, "r") == 0) { redoMove(currentTurn); return 0; }
    if (strcasecmp(input, "exit") == 0) return -1;

    r1 = atoi(input) - 1;
    scanf("%d %d %d", &c1, &r2, &c2);
    c1--; r2--; c2--;

    if (difftime(time(NULL), start) > TIMER_LIMIT) {
        printf(" \033[33m(WARNING: You took too long!)\033[0m\n");
    }

    int capR = -1, capC = -1;
    int moveType = isValidMoveTiger(r1, c1, r2, c2, &capR, &capC);

    if (moveType == 1) {
        // Simple Move
        saveState(*currentTurn);
        board[r1][c1] = EMPTY;
        board[r2][c2] = TIGER;
        return 1;
    } else if (moveType == 2) {
        // Capture Move
        saveState(*currentTurn);
        board[r1][c1] = EMPTY;
        board[r2][c2] = TIGER;
        board[capR][capC] = EMPTY; // Remove eaten goat
        goatsCaptured++;
        return 1;
    } else {
        printCentered("Invalid move.");
        return 0;
    }
}

// --- File I/O ---

void saveGame(int slot) {
    char filename[50];
    sprintf(filename, "%s%d.dat", SAVE_FILE_PREFIX, slot);
    FILE *f = fopen(filename, "wb");
    if (f) {
        fwrite(board, sizeof(char), SIZE * SIZE, f);
        fwrite(&goatCount, sizeof(int), 1, f);
        fwrite(&goatsOnBoard, sizeof(int), 1, f);
        fwrite(&goatsCaptured, sizeof(int), 1, f);
        fwrite(&gamePlayed, sizeof(int), 1, f);
        fclose(f);
        printCentered("Game Saved Automatically.");
    }
}

int loadGame(int slot) {
    char filename[50];
    sprintf(filename, "%s%d.dat", SAVE_FILE_PREFIX, slot);
    FILE *f = fopen(filename, "rb");
    if (!f) return 0;
    
    fread(board, sizeof(char), SIZE * SIZE, f);
    fread(&goatCount, sizeof(int), 1, f);
    fread(&goatsOnBoard, sizeof(int), 1, f);
    fread(&goatsCaptured, sizeof(int), 1, f);
    fread(&gamePlayed, sizeof(int), 1, f);
    
    // Reset History on load to prevent corrupt states
    undoTop = -1;
    redoTop = -1;
    
    fclose(f);
    return 1;
}

// --- UI Functions ---

void printBoard() {
    system("cls");
    printf("\n");
    
    // Top coordinates
    printCentered("    1   2   3   4   5 ");
    printCentered("  +---+---+---+---+---+");

    for (int i = 0; i < SIZE; i++) {
        char rowBuffer[200] = ""; // Buffer for building the row string
        int currentLen = 0;

        // We print the row number
        printf("%*s%d |", (getConsoleWidth() / 2) - 12, "", i + 1);

        for (int j = 0; j < SIZE; j++) {
            // We cannot use printCentered for individual cells, so we print directly
            if (board[i][j] == GOAT) {
                printf(" \033[32mG\033[0m |"); // Green
            } else if (board[i][j] == TIGER) {
                printf(" \033[31mT\033[0m |"); // Red
            } else {
                printf(" \033[90m.\033[0m |"); // Gray dot
            }
        }
        printf("\n");
        printCentered("  +---+---+---+---+---+");
    }
    printf("\n");
    
    char status[100];
    sprintf(status, "Goats To Place: %d   Goats Captured: %d/5", 20 - goatsOnBoard, goatsCaptured);
    printCentered(status);
    printCentered("Controls: 'U' = Undo, 'R' = Redo, 'exit' = Quit");
}

void displayMenu() {
    system("cls");
    printCentered("=== BAGHCHAL GAME MENU ===");
    printCentered("1. Start New Game");
    printCentered("2. Load Game");
    printCentered("3. Game Rules");
    printCentered("4. About");
    printCentered("5. Exit");
    printf("\n");
    printCenteredInline("Enter your choice: ");
}

void showRules() {
    system("cls");
    printCentered("=== GAME RULES ===");
    printCentered("1. 20 Goats vs 4 Tigers.");
    printCentered("2. GOATS: Place all 20, then move adjacent.");
    printCentered("   Goal: Surround tigers so they cannot move.");
    printCentered("3. TIGERS: Move adjacent or jump over goats to capture.");
    printCentered("   Goal: Capture 5 goats.");
    pressEnterToContinue();
}

void showAbout() {
    system("cls");
    printCentered("=== ABOUT THE PROJECT ===");
    printCentered("Bagh-Chal Game - Improved Version");
    printCentered("Features: Undo/Redo, Timer, Save System, Colors");
    pressEnterToContinue();
}

void startGame() {
    int turn = 0; // 0 = Goat, 1 = Tiger
    int result;

    while (1) {
        printBoard();

        // Win Conditions
        if (goatsCaptured >= 5) {
            printCentered("\n\033[31mTIGERS WIN! They captured 5 goats.\033[0m");
            pressEnterToContinue();
            break;
        }
        if (areTigersTrapped()) {
            printCentered("\n\033[32mGOATS WIN! All tigers are trapped.\033[0m");
            pressEnterToContinue();
            break;
        }

        // Execute Turn
        if (turn == 0)
            result = goatTurn(&turn);
        else
            result = tigerTurn(&turn);

        if (result == -1) break; // Exit command
        if (result == 1) {
            // Successful move
            saveGame(0); // Auto-save to slot 0
            turn = 1 - turn; // Switch turn
        }
        // If result == 0, turn does not change (invalid move or undo/redo happened)
    }
}

int main() {
    // Setup console
    setvbuf(stdout, NULL, _IONBF, 0);
    enableVirtualTerminalProcessing();
    system("title Bagh Chal - Tiger and Goat Game");

    while (1) {
        displayMenu();
        int choice;
        if (scanf("%d", &choice) != 1) {
            while(getchar() != '\n'); // Flush invalid input
            continue;
        }
        while (getchar() != '\n'); // Flush newline

        switch (choice) {
            case 1:
                initBoard();
                startGame();
                break;
            case 2: {
                printf("\nEnter save slot (0 for autosave, 1-5 manual): ");
                int slot;
                scanf("%d", &slot);
                if (loadGame(slot)) {
                    printf("Game loaded successfully.\n");
                    Sleep(1000);
                    startGame();
                } else {
                    printf("No saved game found in slot %d.\n", slot);
                    Sleep(1000);
                }
                break;
            }
            case 3:
                showRules();
                break;
            case 4:
                showAbout();
                break;
            case 5:
                printCentered("Thanks for playing!");
                exit(0);
            default:
                printCentered("Invalid choice.");
                Sleep(1000);
        }
    }
    return 0;
}