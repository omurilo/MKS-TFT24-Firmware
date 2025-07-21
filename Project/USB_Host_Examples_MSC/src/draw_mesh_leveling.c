#include "gui.h"
#include "draw_ui.h"
#include "pic_manager.h"
#include "draw_mesh_leveling.h"

float mesh_data[GRID_ROWS][GRID_COLS];

static uint8_t mesh_cap_active = 0;
static uint8_t current_mesh_row = 0;

#define MESH_ORIGIN_X 10
#define MESH_ORIGIN_Y 40
#define MESH_SPACING  22   // menor que no TFT35
#define MESH_RADIUS   3    // ponto menor para caber na tela
#define MESH_BTN_X    100
#define MESH_BTN_Y    200

void draw_MeshLeveling(void) {
    GUI_Clear();
    GUI_SetBkColor(BLACK);
    GUI_SetColor(WHITE);
    GUI_SetFont(&GUI_Font20_ASCII);  // fonte menor
    GUI_DispStringAt("Mesh Leveling", 70, 5);

    // Botão "Start"
    Draw_Button(MESH_BTN_X, MESH_BTN_Y, BTN_WIDTH, BTN_HEIGHT, "Start", 0);

    // Exibir a malha
    mesh_leveling_draw_points();

    infoMenu.menu[infoMenu.cur] = draw_MeshLeveling;
    disp_state = DRAW_MESH_LEVELING;
}

static void mesh_leveling_draw_points(void) {
    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            int x = MESH_ORIGIN_X + col * MESH_SPACING;
            int y = MESH_ORIGIN_Y + row * MESH_SPACING;

            float z = mesh_data[row][col];

            // Cor baseada na altura
            if (z < 0.0f) GUI_SetColor(RED);
            else if (z < 0.1f) GUI_SetColor(YELLOW);
            else GUI_SetColor(GREEN);

            GUI_FillCircle(x, y, MESH_RADIUS);
        }
    }
}

static void mesh_leveling_send_g29(void) {
    initFIFO(&gcodeCmdTxFIFO);

    memset(level_buf, 0, sizeof(level_buf));
    sprintf(level_buf, "G28\n");
    pushFIFO(&gcodeCmdTxFIFO, level_buf);

    memset(level_buf, 0, sizeof(level_buf));
    sprintf(level_buf, "G29\n");
    pushFIFO(&gcodeCmdTxFIFO, level_buf);

    mesh_cap_active  = 1;
    current_mesh_row = 0;
    memset(mesh_data, 0, sizeof(mesh_data));
}

void mesh_leveling_parse_line(const char *line) {
    if (current_mesh_row >= GRID_ROWS)
        return;

    float val;
    uint8_t col = 0;
    const char *p = line;

    while (*p && col < GRID_COLS) {
        if (sscanf(p, "%f", &val) == 1) {
            mesh_data[current_mesh_row][col++] = val;
            while (*p && *p != ' ') p++;
        }
        while (*p == ' ') p++;
    }

    if (col == GRID_COLS)
        current_mesh_row++;
}

static void disp_mesh_leveling(void) {
    if (XPT2046_TouchPressed()) {
        uint16_t x, y;
        XPT2046_GetXY(&x, &y);
        Convert_Pos(x, y, &x, &y);

        if (x >= MESH_BTN_X && x <= (MESH_BTN_X + BTN_WIDTH) &&
            y >= MESH_BTN_Y && y <= (MESH_BTN_Y + BTN_HEIGHT)) {
            current_mesh_row = 0;
            memset(mesh_data, 0, sizeof(mesh_data));
            mesh_leveling_send_g29();
        }
    }
}