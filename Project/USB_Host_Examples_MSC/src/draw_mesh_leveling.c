#include "gui.h"
#include "draw_ui.h"
#include "pic_manager.h"
#include "draw_mesh_leveling.h"

float mesh_data[GRID_ROWS][GRID_COLS];

static uint8_t mesh_cap_active = 0;      // =1 enquanto esperamos a malha
static uint8_t current_mesh_row = 0; 

void draw_MeshLeveling(void) {
    GUI_Clear();
    GUI_SetBkColor(BLACK);
    GUI_SetColor(WHITE);
    GUI_SetFont(&GUI_Font24_ASCII);
    GUI_DispStringAt("Mesh Leveling", 60, 10);

    // Botão "Start"
    Draw_Button(60, 200, BTN_WIDTH, BTN_HEIGHT, "Start", 0);

    // Exibir a malha
    mesh_leveling_draw_points();

    infoMenu.menu[infoMenu.cur] = draw_MeshLeveling;
    disp_state = DRAW_MESH_LEVELING;
}

static void mesh_leveling_draw_points(void) {
    int x0 = 20, y0 = 50;
    int spacing = 30;
    for (int row = 0; row < GRID_ROWS; row++) {
        for (int col = 0; col < GRID_COLS; col++) {
            int x = x0 + col * spacing;
            int y = y0 + row * spacing;

            float z = mesh_data[row][col];

            // Cor baseada na altura (opcional)
            if (z < 0.0f) GUI_SetColor(RED);
            else if (z < 0.1f) GUI_SetColor(YELLOW);
            else GUI_SetColor(GREEN);

            GUI_FillCircle(x, y, 5);
        }
    }
}

static void mesh_leveling_send_g29(void)
{
    initFIFO(&gcodeCmdTxFIFO);

    memset(level_buf, 0, sizeof(level_buf));
    sprintf(level_buf, "G28\n");  // Home antes, opcional
    pushFIFO(&gcodeCmdTxFIFO, level_buf);

    memset(level_buf, 0, sizeof(level_buf));
    sprintf(level_buf, "G29\n");          // ou G81, etc.
    pushFIFO(&gcodeCmdTxFIFO, level_buf);

    /* --- habilita captura --- */
    mesh_cap_active  = 1;
    current_mesh_row = 0;
    memset(mesh_data, 0, sizeof(mesh_data));   // limpa matriz
}

static int current_mesh_row = 0;

void mesh_leveling_parse_line(const char *line)
{
    if (current_mesh_row >= GRID_ROWS)
        return;

    float val;
    uint8_t col = 0;
    const char *p = line;

    while (*p && col < GRID_COLS)
    {
        if (sscanf(p, "%f", &val) == 1)
        {
            mesh_data[current_mesh_row][col++] = val;
            while (*p && *p != ' ') p++;   // avança até próximo espaço
        }
        while (*p == ' ') p++;             // pula espaços
    }

    if (col == GRID_COLS)
        current_mesh_row++;                // uma linha completa lida
}

static void disp_mesh_leveling(void) {
    if (XPT2046_TouchPressed()) {
        uint16_t x, y;
        XPT2046_GetXY(&x, &y);
        Convert_Pos(x, y, &x, &y);

        if (x >= 60 && x <= (60 + BTN_WIDTH) && y >= 200 && y <= (200 + BTN_HEIGHT)) {
            current_mesh_row = 0;
            memset(mesh_data, 0, sizeof(mesh_data));
            mesh_leveling_send_g29();
        }
    }
}
