#include "../ui.h"
#include "../../xtouch/ams.h"

// COMPONENT amsPanel: edit an AMS slot's material and colour (sends ams_filament_setting)

typedef struct
{
    const char *type;
    const char *info_idx; // Bambu Studio generic profile filament_id
    int temp_min;
    int temp_max;
} ams_panel_material_t;

// shortcut: generic Bambu profiles only (ids/temps from BambuStudio resources/profiles/BBL/filament), add brand presets here if needed
static const ams_panel_material_t ams_panel_materials[] = {
    {"PLA", "GFL99", 190, 240},
    {"PETG", "GFG99", 220, 270},
    {"ABS", "GFB99", 240, 280},
    {"ASA", "GFB98", 240, 280},
    {"TPU", "GFU99", 200, 250},
    {"PLA-CF", "GFL98", 190, 240},
    {"PETG-CF", "GFG98", 240, 270},
    {"PA", "GFN99", 240, 280},
    {"PC", "GFC99", 260, 290},
    {"PVA", "GFS99", 190, 240},
};
#define AMS_PANEL_MATERIALS (sizeof(ams_panel_materials) / sizeof(ams_panel_materials[0]))

static const char *ams_panel_material_map[] = {"PLA", "PETG", "ABS", "ASA", "TPU", "\n", "PLA-CF", "PETG-CF", "PA", "PC", "PVA", ""};

// subset of Bambu Studio's AMS colour palette
static const uint32_t ams_panel_palette[] = {
    0xFFFFFF, 0xBCBCBC, 0x898989, 0x161616, 0xF72323, 0xF98C36, 0xFFF144, 0x7C4B00,
    0x0ACC38, 0x057748, 0x76D9F4, 0x46A8F9, 0x2850E0, 0xA03CF7, 0xF330F9, 0xF95D73};
#define AMS_PANEL_COLORS (sizeof(ams_panel_palette) / sizeof(ams_panel_palette[0]))

static lv_obj_t *ams_panel;
static lv_obj_t *ams_panel_slots[4];
static lv_obj_t *ams_panel_material;
static lv_obj_t *ams_panel_colors[AMS_PANEL_COLORS];
static lv_obj_t *ams_panel_unit_label;
static lv_obj_t *ams_panel_save;

static int ams_panel_unit;
static int ams_panel_tray;
static int ams_panel_material_sel; // -1 = slot's material isn't in the list yet
static uint32_t ams_panel_color;

static lv_color_t ams_panel_contrast(uint32_t rgb)
{
    int luma = (((rgb >> 16) & 0xFF) * 299 + ((rgb >> 8) & 0xFF) * 587 + (rgb & 0xFF) * 114) / 1000;
    return lv_color_hex(luma > 140 ? 0x000000 : 0xFFFFFF);
}

static void ams_panel_refresh()
{
    char unit_text[16];
    lv_snprintf(unit_text, sizeof(unit_text), "AMS %d", ams_panel_unit + 1);
    lv_label_set_text(ams_panel_unit_label, unit_text);

    for (int i = 0; i < 4; i++)
    {
        struct XTouchAmsTray *tray = &bambuStatus.ams_trays[ams_panel_unit][i];
        bool selected = i == ams_panel_tray;
        // the selected slot previews the pending choice
        uint32_t color = selected ? ams_panel_color : tray->color;
        bool filled = selected || tray->exists;
        lv_obj_set_style_bg_color(ams_panel_slots[i], lv_color_hex(color), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ams_panel_slots[i], filled ? 255 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(ams_panel_slots[i], selected ? 3 : 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ams_panel_slots[i], lv_color_hex(selected ? 0xff682a : 0x777777), LV_PART_MAIN);

        lv_obj_t *label = lv_obj_get_child(ams_panel_slots[i], 0);
        const char *type = selected && ams_panel_material_sel >= 0 ? ams_panel_materials[ams_panel_material_sel].type : (tray->exists ? tray->type : "-");
        lv_label_set_text(label, type);
        lv_obj_set_style_text_color(label, filled ? ams_panel_contrast(color) : lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    }

    lv_btnmatrix_clear_btn_ctrl_all(ams_panel_material, LV_BTNMATRIX_CTRL_CHECKED);
    if (ams_panel_material_sel >= 0)
        lv_btnmatrix_set_btn_ctrl(ams_panel_material, ams_panel_material_sel, LV_BTNMATRIX_CTRL_CHECKED);

    for (int i = 0; i < (int)AMS_PANEL_COLORS; i++)
        lv_obj_set_style_border_width(ams_panel_colors[i], ams_panel_palette[i] == ams_panel_color ? 3 : 0, LV_PART_MAIN);

    if (ams_panel_material_sel >= 0)
        lv_obj_clear_state(ams_panel_save, LV_STATE_DISABLED);
    else
        lv_obj_add_state(ams_panel_save, LV_STATE_DISABLED);
}

static void ams_panel_select_tray(int tray_id)
{
    struct XTouchAmsTray *tray = &bambuStatus.ams_trays[ams_panel_unit][tray_id];
    ams_panel_tray = tray_id;
    ams_panel_color = tray->exists ? tray->color : ams_panel_palette[0];
    ams_panel_material_sel = -1;
    for (int i = 0; i < (int)AMS_PANEL_MATERIALS; i++)
        if (tray->exists && strcmp(tray->type, ams_panel_materials[i].type) == 0)
            ams_panel_material_sel = i;
    ams_panel_refresh();
}

static void ams_panel_on_slot(lv_event_t *e)
{
    ams_panel_select_tray((int)(intptr_t)lv_event_get_user_data(e));
}

static void ams_panel_on_material(lv_event_t *e)
{
    uint16_t id = lv_btnmatrix_get_selected_btn(lv_event_get_target(e));
    if (id < AMS_PANEL_MATERIALS)
    {
        ams_panel_material_sel = id;
        ams_panel_refresh();
    }
}

static void ams_panel_on_color(lv_event_t *e)
{
    ams_panel_color = ams_panel_palette[(int)(intptr_t)lv_event_get_user_data(e)];
    ams_panel_refresh();
}

// tap "AMS n" to cycle through the connected units
static void ams_panel_on_unit(lv_event_t *e)
{
    for (int i = 1; i <= XTOUCH_AMS_MAX_UNITS; i++)
    {
        int next = (ams_panel_unit + i) % XTOUCH_AMS_MAX_UNITS;
        if (bambuStatus.ams_unit_bits & (1 << next))
        {
            ams_panel_unit = next;
            break;
        }
    }
    ams_panel_select_tray(0);
}

static void ams_panel_on_cancel(lv_event_t *e)
{
    ui_amsPanel_hide();
}

static void ams_panel_on_save(lv_event_t *e)
{
    if (ams_panel_material_sel < 0)
        return;
    const ams_panel_material_t *mat = &ams_panel_materials[ams_panel_material_sel];
    struct XTOUCH_AMS_FILAMENT_SETTING setting = {
        .ams_id = ams_panel_unit,
        .tray_id = ams_panel_tray,
        .color = ams_panel_color,
        .type = mat->type,
        .info_idx = mat->info_idx,
        .temp_min = mat->temp_min,
        .temp_max = mat->temp_max,
    };
    lv_msg_send(XTOUCH_COMMAND_AMS_FILAMENT_SETTING, &setting);
    ui_amsPanel_hide();
}

// keep the other slots live while the panel is open
static void ams_panel_on_trays(lv_event_t *e)
{
    if (!lv_obj_has_flag(ams_panel, LV_OBJ_FLAG_HIDDEN))
        ams_panel_refresh();
}

void ui_amsPanel_show()
{
    ams_panel_unit = xtouch_ams_display_unit();
    int active = bambuStatus.m_tray_now;
    ams_panel_select_tray(active >= 0 && active < XTOUCH_AMS_MAX_UNITS * 4 && (active >> 2) == ams_panel_unit ? (active & 3) : 0);
    lv_obj_clear_flag(ams_panel, LV_OBJ_FLAG_HIDDEN);
}

void ui_amsPanel_hide()
{
    lv_obj_add_flag(ams_panel, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *ams_panel_row(lv_obj_t *parent, lv_coord_t height)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, height);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(row, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(row, 4, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return row;
}

static lv_obj_t *ams_panel_button(lv_obj_t *parent, const char *text, uint32_t bg, uint32_t bg_pressed, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_height(btn, lv_pct(100));
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_pressed), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x444444), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(0x777777), LV_PART_MAIN | LV_STATE_DISABLED);
    return btn;
}

lv_obj_t *ui_amsPanel_create(lv_obj_t *comp_parent)
{
    ams_panel = lv_obj_create(comp_parent);
    lv_obj_set_size(ams_panel, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(ams_panel, LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ams_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ams_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ams_panel, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius(ams_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ams_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ams_panel, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ams_panel, 255, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ams_panel, 6, LV_PART_MAIN);
    lv_obj_set_style_text_font(ams_panel, &lv_font_montserrat_14, LV_PART_MAIN);

    // slots
    lv_obj_t *slots = ams_panel_row(ams_panel, 40);
    for (int i = 0; i < 4; i++)
    {
        lv_obj_t *slot = lv_obj_create(slots);
        lv_obj_set_height(slot, lv_pct(100));
        lv_obj_set_flex_grow(slot, 1);
        lv_obj_clear_flag(slot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(slot, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(slot, 0, LV_PART_MAIN);
        lv_obj_add_event_cb(slot, ams_panel_on_slot, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *label = lv_label_create(slot);
        lv_obj_center(label);
        ams_panel_slots[i] = slot;
    }

    // materials
    ams_panel_material = lv_btnmatrix_create(ams_panel);
    lv_btnmatrix_set_map(ams_panel_material, ams_panel_material_map);
    lv_btnmatrix_set_btn_ctrl_all(ams_panel_material, LV_BTNMATRIX_CTRL_CHECKABLE | LV_BTNMATRIX_CTRL_NO_REPEAT);
    lv_btnmatrix_set_one_checked(ams_panel_material, true);
    lv_obj_set_size(ams_panel_material, lv_pct(100), 64);
    lv_obj_set_style_bg_opa(ams_panel_material, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ams_panel_material, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ams_panel_material, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(ams_panel_material, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(ams_panel_material, 6, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(ams_panel_material, lv_color_hex(0x555555), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(ams_panel_material, lv_color_hex(0xff682a), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(ams_panel_material, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_add_event_cb(ams_panel_material, ams_panel_on_material, LV_EVENT_VALUE_CHANGED, NULL);

    // colours
    lv_obj_t *colors = ams_panel_row(ams_panel, 52);
    for (int i = 0; i < (int)AMS_PANEL_COLORS; i++)
    {
        lv_obj_t *swatch = lv_obj_create(colors);
        lv_obj_set_size(swatch, lv_pct(11), 24);
        lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(swatch, 4, LV_PART_MAIN);
        lv_obj_set_style_bg_color(swatch, lv_color_hex(ams_panel_palette[i]), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(swatch, 255, LV_PART_MAIN);
        lv_obj_set_style_border_color(swatch, lv_color_hex(0xff682a), LV_PART_MAIN);
        lv_obj_add_event_cb(swatch, ams_panel_on_color, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        ams_panel_colors[i] = swatch;
    }

    // unit / cancel / save
    lv_obj_t *actions = ams_panel_row(ams_panel, 36);
    lv_obj_t *unit = ams_panel_button(actions, "", 0x555555, 0x777777, ams_panel_on_unit);
    ams_panel_unit_label = lv_obj_get_child(unit, 0);
    ams_panel_button(actions, "Cancel", 0xAA2A00, 0x552A00, ams_panel_on_cancel);
    ams_panel_save = ams_panel_button(actions, "Save", 0x2AAA00, 0x2A5500, ams_panel_on_save);

    lv_obj_add_event_cb(ams_panel, ams_panel_on_trays, LV_EVENT_MSG_RECEIVED, NULL);
    lv_msg_subsribe_obj(XTOUCH_ON_AMS_TRAYS, ams_panel, NULL);

    return ams_panel;
}
