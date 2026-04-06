#include <psxgpu.h>
#include <psxetc.h>
#include <psxapi.h>
#include <psxpad.h>

#define SCREEN_W 320
#define SCREEN_H 240
#define OT_LEN 1
#define PRIM_BUFFER_SIZE 8192

#define CELL_SIZE 10
#define FIELD_W (GRID_W * CELL_SIZE)
#define FIELD_H (GRID_H * CELL_SIZE)
#define FIELD_X ((SCREEN_W - FIELD_W) / 2)
#define FIELD_Y 34

typedef struct {
    DISPENV disp;
    DRAWENV draw;
    uint32_t ot[OT_LEN];
    uint8_t primBuffer[PRIM_BUFFER_SIZE];
} FrameBuffer;

static FrameBuffer frame[2];
static int activeBuffer = 0;

static uint8_t padBuffer1[34];
static uint8_t padBuffer2[34];
static volatile PADTYPE* padState = (volatile PADTYPE*)padBuffer1;
static uint16_t prevPad = 0;

typedef enum {
    SCREEN_MAIN_MENU,
    SCREEN_OPTIONS,
    SCREEN_GAMEPLAY
} ScreenID;

typedef struct {
    int x;
    int y;
} Cell;

typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

typedef enum {
    TRANSITION_NONE,
    TRANSITION_FADE_OUT,
    TRANSITION_FADE_IN
} TransitionPhase;

static const char* menuItems[] = {
    "nuova partita",
    "continua",
    "opzioni",
    "grafica",
    "audio",
    "controlli",
    "extra",
    "crediti",
    "esci"
};

#define MENU_ITEM_COUNT ((int)(sizeof(menuItems) / sizeof(menuItems[0])))
#define MENU_VISIBLE_ROWS 5

static int selectedIndex = 0;
static int firstVisible = 0;

static const char* optionsItems[] = {
    "velocita: normale",
    "musica: off",
    "torna indietro"
};

#define OPTIONS_ITEM_COUNT ((int)(sizeof(optionsItems) / sizeof(optionsItems[0])))

static int optionsSelectedIndex = 0;

static ScreenID currentScreen = SCREEN_MAIN_MENU;
static ScreenID targetScreen = SCREEN_MAIN_MENU;
static TransitionPhase transition = TRANSITION_NONE;
static int fadeLevel = 0;

static const char* lastActionText = "";

#define GRID_W 20
#define GRID_H 15
#define SNAKE_MAX_CELLS (GRID_W * GRID_H)

static Cell snakeBody[SNAKE_MAX_CELLS];
static int snakeLength = 0;
static Direction snakeDir = DIR_RIGHT;
static Direction snakeNextDir = DIR_RIGHT;
static Cell food = { 0, 0 };
static int snakeScore = 0;
static int snakeGameOver = 0;
static int snakeRunning = 0;
static int snakeTick = 0;
static int snakeStepDelay = 7;
static uint32_t rngState = 0x1234ABCDu;

static void initVideo(void) {
    ResetGraph(0);

    SetDefDispEnv(&frame[0].disp, 0, 0, SCREEN_W, SCREEN_H);
    SetDefDrawEnv(&frame[0].draw, 0, SCREEN_H, SCREEN_W, SCREEN_H);

    SetDefDispEnv(&frame[1].disp, 0, SCREEN_H, SCREEN_W, SCREEN_H);
    SetDefDrawEnv(&frame[1].draw, 0, 0, SCREEN_W, SCREEN_H);

    frame[0].draw.isbg = 1;
    setRGB0(&frame[0].draw, 0, 0, 0);

    frame[1].draw.isbg = 1;
    setRGB0(&frame[1].draw, 0, 0, 0);

    PutDispEnv(&frame[0].disp);
    PutDrawEnv(&frame[0].draw);
    SetDispMask(1);
}

static void initInput(void) {
    InitPAD(padBuffer1, sizeof(padBuffer1), padBuffer2, sizeof(padBuffer2));
    StartPAD();
    ChangeClearPAD(0);
}

static uint32_t rand32(void) {
    rngState = rngState * 1664525u + 1013904223u;
    return rngState;
}

static int cellsEqual(Cell a, Cell b) {
    return (a.x == b.x) && (a.y == b.y);
}

static int snakeContains(Cell c, int skipHead) {
    int start = skipHead ? 1 : 0;
    for (int i = start; i < snakeLength; i++) {
        if (cellsEqual(snakeBody[i], c)) {
            return 1;
        }
    }
    return 0;
}

static void spawnFood(void) {
    Cell c;

    do {
        c.x = (int)(rand32() % GRID_W);
        c.y = (int)(rand32() % GRID_H);
    } while (snakeContains(c, 0));

    food = c;
}

static int isOpposite(Direction a, Direction b) {
    return (a == DIR_UP && b == DIR_DOWN) ||
           (a == DIR_DOWN && b == DIR_UP) ||
           (a == DIR_LEFT && b == DIR_RIGHT) ||
           (a == DIR_RIGHT && b == DIR_LEFT);
}

static void resetSnakeGame(void) {
    snakeLength = 4;
    snakeDir = DIR_RIGHT;
    snakeNextDir = DIR_RIGHT;
    snakeScore = 0;
    snakeGameOver = 0;
    snakeRunning = 0;
    snakeTick = 0;
    snakeStepDelay = 7;

    snakeBody[0].x = GRID_W / 2;
    snakeBody[0].y = GRID_H / 2;

    for (int i = 1; i < snakeLength; i++) {
        snakeBody[i].x = snakeBody[0].x - i;
        snakeBody[i].y = snakeBody[0].y;
    }

    spawnFood();
}

static Cell nextHeadPosition(void) {
    Cell h = snakeBody[0];

    if (snakeNextDir == DIR_UP) h.y--;
    if (snakeNextDir == DIR_DOWN) h.y++;
    if (snakeNextDir == DIR_LEFT) h.x--;
    if (snakeNextDir == DIR_RIGHT) h.x++;

    return h;
}

static void stepSnakeGame(void) {
    if (snakeGameOver) {
        return;
    }

    snakeDir = snakeNextDir;
    Cell next = nextHeadPosition();

    if ((next.x < 0) || (next.y < 0) || (next.x >= GRID_W) || (next.y >= GRID_H)) {
        snakeGameOver = 1;
        return;
    }

    if (snakeContains(next, 1)) {
        snakeGameOver = 1;
        return;
    }

    int grow = cellsEqual(next, food);
    int newLength = snakeLength + (grow ? 1 : 0);
    if (newLength > SNAKE_MAX_CELLS) {
        newLength = SNAKE_MAX_CELLS;
    }

    for (int i = newLength - 1; i > 0; i--) {
        snakeBody[i] = snakeBody[i - 1];
    }

    snakeBody[0] = next;
    snakeLength = newLength;

    if (grow) {
        snakeScore += 10;
        if (snakeStepDelay > 3) {
            snakeStepDelay--;
        }
        spawnFood();
    }
}

static uint16_t readPadPressed(void) {
    uint16_t pad = (uint16_t)(~padState->btn);
    uint16_t pressed = (uint16_t)(pad & (uint16_t)(~prevPad));
    prevPad = pad;
    return pressed;
}

static void startFadeTo(ScreenID screen) {
    if ((screen == currentScreen) || (transition != TRANSITION_NONE)) {
        return;
    }

    targetScreen = screen;
    transition = TRANSITION_FADE_OUT;
}

static void updateTransition(void) {
    const int fadeStep = 12;

    if (transition == TRANSITION_FADE_OUT) {
        fadeLevel += fadeStep;
        if (fadeLevel >= 96) {
            fadeLevel = 96;
            currentScreen = targetScreen;
            transition = TRANSITION_FADE_IN;
        }
    } else if (transition == TRANSITION_FADE_IN) {
        fadeLevel -= fadeStep;
        if (fadeLevel <= 0) {
            fadeLevel = 0;
            transition = TRANSITION_NONE;
        }
    }
}

static void updateMainMenu(uint16_t pressed) {
    if (transition != TRANSITION_NONE) {
        return;
    }

    if (pressed & PAD_UP) {
        if (selectedIndex > 0) {
            selectedIndex--;
        } else {
            selectedIndex = MENU_ITEM_COUNT - 1;
        }
    }

    if (pressed & PAD_DOWN) {
        if (selectedIndex < (MENU_ITEM_COUNT - 1)) {
            selectedIndex++;
        } else {
            selectedIndex = 0;
        }
    }

    if (selectedIndex < firstVisible) {
        firstVisible = selectedIndex;
    }

    if (selectedIndex >= (firstVisible + MENU_VISIBLE_ROWS)) {
        firstVisible = selectedIndex - MENU_VISIBLE_ROWS + 1;
    }

    if (pressed & PAD_CROSS) {
        if (selectedIndex == 0) {
            resetSnakeGame();
            startFadeTo(SCREEN_GAMEPLAY);
        } else if (selectedIndex == 2) {
            startFadeTo(SCREEN_OPTIONS);
        } else {
            lastActionText = menuItems[selectedIndex];
        }
    }
}

static void updateOptionsMenu(uint16_t pressed) {
    if (transition != TRANSITION_NONE) {
        return;
    }

    if (pressed & PAD_UP) {
        if (optionsSelectedIndex > 0) {
            optionsSelectedIndex--;
        } else {
            optionsSelectedIndex = OPTIONS_ITEM_COUNT - 1;
        }
    }

    if (pressed & PAD_DOWN) {
        if (optionsSelectedIndex < (OPTIONS_ITEM_COUNT - 1)) {
            optionsSelectedIndex++;
        } else {
            optionsSelectedIndex = 0;
        }
    }

    if (pressed & PAD_CROSS) {
        if (optionsSelectedIndex == OPTIONS_ITEM_COUNT - 1) {
            startFadeTo(SCREEN_MAIN_MENU);
        } else {
            lastActionText = optionsItems[optionsSelectedIndex];
        }
    }

    if (pressed & PAD_CIRCLE) {
        startFadeTo(SCREEN_MAIN_MENU);
    }
}

static void updateGameplayScreen(uint16_t pressed) {
    if (transition != TRANSITION_NONE) {
        return;
    }

    if (snakeGameOver) {
        if (pressed & PAD_START) {
            resetSnakeGame();
        }
    } else {
        Direction requested = snakeNextDir;

        if (pressed & PAD_UP) requested = DIR_UP;
        if (pressed & PAD_DOWN) requested = DIR_DOWN;
        if (pressed & PAD_LEFT) requested = DIR_LEFT;
        if (pressed & PAD_RIGHT) requested = DIR_RIGHT;

        if (!isOpposite(snakeDir, requested)) {
            snakeNextDir = requested;
        }

        if (!snakeRunning) {
            if ((pressed & PAD_UP) || (pressed & PAD_DOWN) || (pressed & PAD_LEFT) || (pressed & PAD_RIGHT)) {
                snakeRunning = 1;
            }
        }

        if (snakeRunning) {
            snakeTick++;
            if (snakeTick >= snakeStepDelay) {
                snakeTick = 0;
                stepSnakeGame();
            }
        }
    }

    if (pressed & PAD_CIRCLE) {
        startFadeTo(SCREEN_MAIN_MENU);
    }
}

static void drawMainMenu(int uiFont) {
    FntPrint(uiFont, "menu principale\n\n");

    for (int row = 0; row < MENU_VISIBLE_ROWS; row++) {
        int itemIndex = firstVisible + row;
        if (itemIndex >= MENU_ITEM_COUNT) {
            break;
        }

        const char* cursor = (itemIndex == selectedIndex) ? ">" : " ";
        FntPrint(uiFont, "%s %s\n", cursor, menuItems[itemIndex]);
    }

    FntPrint(uiFont, "\nX conferma   su/giu scorri");
    if (lastActionText[0] != '\0') {
        FntPrint(uiFont, "\nultima scelta: %s", lastActionText);
    }
}

static void drawOptionsMenu(int uiFont) {
    FntPrint(uiFont, "opzioni\n\n");

    for (int i = 0; i < OPTIONS_ITEM_COUNT; i++) {
        const char* cursor = (i == optionsSelectedIndex) ? ">" : " ";
        FntPrint(uiFont, "%s %s\n", cursor, optionsItems[i]);
    }

    FntPrint(uiFont, "\nX conferma   O indietro");
    if (lastActionText[0] != '\0') {
        FntPrint(uiFont, "\nultima scelta: %s", lastActionText);
    }
}

static void addTile(uint8_t** nextPrim, uint32_t* ot, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    TILE* t = (TILE*)(*nextPrim);

    setTile(t);
    setXY0(t, x, y);
    setWH(t, w, h);
    setRGB0(t, r, g, b);
    addPrim(ot, t);

    *nextPrim += sizeof(TILE);
}

static void drawGameplayGraphics(void) {
    FrameBuffer* fb = &frame[activeBuffer];
    uint8_t* nextPrim = fb->primBuffer;

    for (int i = 0; i < snakeLength; i++) {
        int x = FIELD_X + snakeBody[i].x * CELL_SIZE;
        int y = FIELD_Y + snakeBody[i].y * CELL_SIZE;

        if (i == 0) {
            addTile(&nextPrim, fb->ot, x + 1, y + 1, CELL_SIZE - 2, CELL_SIZE - 2, 85, 250, 110);
        } else {
            addTile(&nextPrim, fb->ot, x + 1, y + 1, CELL_SIZE - 2, CELL_SIZE - 2, 40, 190, 80);
        }
    }

    addTile(
        &nextPrim,
        fb->ot,
        FIELD_X + food.x * CELL_SIZE + 1,
        FIELD_Y + food.y * CELL_SIZE + 1,
        CELL_SIZE - 2,
        CELL_SIZE - 2,
        255,
        105,
        80
    );

    addTile(&nextPrim, fb->ot, FIELD_X, FIELD_Y, FIELD_W, FIELD_H, 6, 7, 10);
    addTile(&nextPrim, fb->ot, FIELD_X - 3, FIELD_Y - 3, FIELD_W + 6, FIELD_H + 6, 40, 42, 50);

    if (snakeGameOver) {
        addTile(&nextPrim, fb->ot, FIELD_X + 18, FIELD_Y + FIELD_H / 2 - 10, FIELD_W - 36, 20, 175, 26, 26);
    }
}

static void drawGameplayScreen(int uiFont) {
    FntPrint(uiFont, "snake\n");
    FntPrint(uiFont, "punteggio: %d\n", snakeScore);

    if (!snakeRunning && !snakeGameOver) {
        FntPrint(uiFont, "premi una direzione per iniziare\n\n");
    } else {
        FntPrint(uiFont, "su/giu/sx/dx muovi\n\n");
    }

    if (snakeGameOver) {
        FntPrint(uiFont, "GAME OVER - START restart\n");
    }
    FntPrint(uiFont, "\nO torna al menu");
}

int main(void) {
    initVideo();
    initInput();

    FntLoad(960, 0);

    int uiFont = FntOpen(40, 24, 256, 196, 0, 512);

    while (1) {
        ClearOTagR(frame[activeBuffer].ot, OT_LEN);

        uint16_t pressed = readPadPressed();

        if (currentScreen == SCREEN_MAIN_MENU) {
            updateMainMenu(pressed);
        } else if (currentScreen == SCREEN_OPTIONS) {
            updateOptionsMenu(pressed);
        } else {
            updateGameplayScreen(pressed);
        }

        updateTransition();

        setRGB0(&frame[activeBuffer].draw, fadeLevel, fadeLevel, fadeLevel);

        // Clear the text stream each frame to avoid overflow/truncation artifacts.
        FntPrint(uiFont, "\f");
        if (currentScreen == SCREEN_MAIN_MENU) {
            drawMainMenu(uiFont);
        } else if (currentScreen == SCREEN_OPTIONS) {
            drawOptionsMenu(uiFont);
        } else {
            drawGameplayGraphics();
            drawGameplayScreen(uiFont);
        }

        FntFlush(-1);

        DrawSync(0);
        VSync(0);

        PutDispEnv(&frame[activeBuffer].disp);
        PutDrawEnv(&frame[activeBuffer].draw);
        DrawOTag(frame[activeBuffer].ot + OT_LEN - 1);

        activeBuffer = !activeBuffer;
    }

    return 0;
}
