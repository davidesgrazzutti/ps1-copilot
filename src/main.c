#include <psxgpu.h>
#include <psxetc.h>
#include <psxapi.h>
#include <psxpad.h>

#define SCREEN_W 320
#define SCREEN_H 240
#define OT_LEN 1

typedef struct {
    DISPENV disp;
    DRAWENV draw;
    uint32_t ot[OT_LEN];
} FrameBuffer;

static FrameBuffer frame[2];
static int activeBuffer = 0;

static uint8_t padBuffer1[34];
static uint8_t padBuffer2[34];
static volatile PADTYPE* padState = (volatile PADTYPE*)padBuffer1;
static uint16_t prevPad = 0;

typedef enum {
    SCREEN_MAIN_MENU,
    SCREEN_OPTIONS
} ScreenID;

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
        if (selectedIndex == 2) {
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
        } else {
            updateOptionsMenu(pressed);
        }

        updateTransition();

        setRGB0(&frame[activeBuffer].draw, fadeLevel, fadeLevel, fadeLevel);

        // Clear the text stream each frame to avoid overflow/truncation artifacts.
        FntPrint(uiFont, "\f");
        if (currentScreen == SCREEN_MAIN_MENU) {
            drawMainMenu(uiFont);
        } else {
            drawOptionsMenu(uiFont);
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
