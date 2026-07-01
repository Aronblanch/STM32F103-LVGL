/**
 * @file ui_main.c
 * @brief 自定义UI界面实现（从PC端LVGL项目移植）
 */

/*********************
 *      INCLUDES
 *********************/
#include "ui_main.h"
#include "lvgl.h"

/*********************
 *      DEFINES
 *********************/
#define TEMP_MIN 0
#define TEMP_MAX 100
#define TEMP_DEFAULT 10

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_subject_t temp_subject;
static lv_obj_t *temp_label; /* temperature label, needs manual update on language change */
static lv_obj_t *tv_global;   /* tabview */
static lv_obj_t *lbl1_global; /* tab 1 button label */
static lv_obj_t *lbl2_global; /* tab 2 button label */

/**********************
 *  EXTERN FONT
 **********************/
LV_FONT_DECLARE(my_siyuan_font);

/**********************
 *  STATIC PROTOTYPES
 **********************/
static const char *lang_get(const char *tag);
static void temp_label_update(void);
static void temp_label_subject_cb(lv_observer_t *observer, lv_subject_t *subject);
static void btn_plus_event_cb(lv_event_t *e);
static void btn_minus_event_cb(lv_event_t *e);
static void language_dropdown_change_cb(lv_event_t *e);
static void ui_update_language(void);

/**********************
 *      MACROS
 **********************/

/**********************
 *   LANGUAGE SUPPORT
 **********************/
typedef enum {
    LANG_ENGLISH,
    LANG_CHINESE,
} lang_t;

static const char *const lang_tags[] = {
    "tab_home", "tab_settings", "label_temp", NULL};

static const char *const lang_strings_en[] = {
    "Home", "Settings", "temp:%d", NULL};

static const char *const lang_strings_zh[] = {
    "\xe9\xa6\x96\xe9\xa1\xb5", "\xe8\xae\xbe\xe7\xbd\xae", "\xe6\xb8\xa9\xe5\xba\xa6:%d", NULL};

static lang_t current_lang = LANG_ENGLISH;

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* Look up translation by tag */
static const char *lang_get(const char *tag)
{
    const char *const *strings = (current_lang == LANG_ENGLISH) ? lang_strings_en : lang_strings_zh;
    for (int i = 0; lang_tags[i]; i++) {
        if (lv_streq(lang_tags[i], tag))
            return strings[i];
    }
    return tag; /* fallback: return tag itself */
}

static void lang_set(lang_t lang)
{
    current_lang = lang;
}

/* Update temperature label text based on current language and subject value */
static void temp_label_update(void)
{
    int32_t val = lv_subject_get_int(&temp_subject);
    const char *fmt = lang_get("label_temp");
    char buf[32];
    lv_snprintf(buf, sizeof(buf), fmt, (int)val);
    lv_label_set_text(temp_label, buf);
}

/* Observer callback: subject value changed -> update label text */
static void temp_label_subject_cb(lv_observer_t *observer, lv_subject_t *subject)
{
    LV_UNUSED(observer);
    LV_UNUSED(subject);
    temp_label_update();
}

/* Update all translatable UI text */
static void ui_update_language(void)
{
    /* Update tab button labels */
    lv_label_set_text(lbl1_global, lang_get("tab_home"));
    lv_label_set_text(lbl2_global, lang_get("tab_settings"));
    /* Temperature label */
    temp_label_update();
}

static void btn_plus_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    int32_t val = lv_subject_get_int(&temp_subject);
    if (val < TEMP_MAX) {
        lv_subject_set_int(&temp_subject, val + 1);
    }
}

static void btn_minus_event_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    int32_t val = lv_subject_get_int(&temp_subject);
    if (val > TEMP_MIN) {
        lv_subject_set_int(&temp_subject, val - 1);
    }
}

static void language_dropdown_change_cb(lv_event_t *e)
{
    lv_obj_t *dropdown = lv_event_get_target_obj(e);
    uint32_t selected = lv_dropdown_get_selected(dropdown);
    if (selected == 0) {
        lang_set(LANG_ENGLISH);
    } else {
        lang_set(LANG_CHINESE);
    }
    ui_update_language();
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void create_temp_ui(void)
{
    /* --- Subject: temperature state (Observer Pattern) --- */
    lv_subject_init_int(&temp_subject, TEMP_DEFAULT);
    lv_subject_set_min_value_int(&temp_subject, TEMP_MIN);
    lv_subject_set_max_value_int(&temp_subject, TEMP_MAX);

    /* Create TabView */
    lv_obj_t *tv = lv_tabview_create(lv_screen_active());
    tv_global = tv;
    lv_tabview_set_tab_bar_position(tv, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tv, 50);
    /* Apply Chinese font to the entire TabView (cascades to all children) */
    lv_obj_set_style_text_font(tv, &my_siyuan_font, 0);

    /* Create tabs and set translatable text directly */
    lv_obj_t *tab1 = lv_tabview_add_tab(tv, lang_get("tab_home"));
    lv_obj_t *tab2 = lv_tabview_add_tab(tv, lang_get("tab_settings"));

    /* Get tab button labels for later language updates */
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tv);
    lv_obj_t *btn1 = lv_obj_get_child_by_type(tab_bar, 0, &lv_button_class);
    lbl1_global = lv_obj_get_child_by_type(btn1, 0, &lv_label_class);
    lv_obj_t *btn2 = lv_obj_get_child_by_type(tab_bar, 1, &lv_button_class);
    lbl2_global = lv_obj_get_child_by_type(btn2, 0, &lv_label_class);

    /* --- Tab 1 (Home): Temperature control --- */

    /* Temperature label: observer for value changes */
    temp_label = lv_label_create(tab1);
    lv_subject_add_observer(&temp_subject, temp_label_subject_cb, NULL);
    temp_label_update(); /* set initial text */
    lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 60);

    /* Plus button */
    lv_obj_t *btn_plus = lv_button_create(tab1);
    lv_obj_set_size(btn_plus, 60, 60);
    lv_obj_align(btn_plus, LV_ALIGN_CENTER, -50, 0);
    lv_obj_set_style_radius(btn_plus, 10, 0);

    lv_obj_t *label_plus = lv_label_create(btn_plus);
    lv_label_set_text(label_plus, "+");
    lv_obj_set_style_text_font(label_plus, &lv_font_montserrat_28, 0);
    lv_obj_center(label_plus);
    lv_obj_add_event_cb(btn_plus, btn_plus_event_cb, LV_EVENT_CLICKED, NULL);

    /* Minus button */
    lv_obj_t *btn_minus = lv_button_create(tab1);
    lv_obj_set_size(btn_minus, 60, 60);
    lv_obj_align(btn_minus, LV_ALIGN_CENTER, 50, 0);
    lv_obj_set_style_radius(btn_minus, 10, 0);

    lv_obj_t *label_minus = lv_label_create(btn_minus);
    lv_label_set_text(label_minus, "-");
    lv_obj_set_style_text_font(label_minus, &lv_font_montserrat_28, 0);
    lv_obj_center(label_minus);
    lv_obj_add_event_cb(btn_minus, btn_minus_event_cb, LV_EVENT_CLICKED, NULL);

    /* Slider: bidirectional binding to temp_subject */
    lv_obj_t *slider = lv_slider_create(tab1);
    lv_slider_set_range(slider, TEMP_MIN, TEMP_MAX);
    lv_slider_bind_value(slider, &temp_subject);
    lv_obj_set_width(slider, lv_pct(80));
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -20);

    /* --- Tab 2 (Settings): Language selection --- */
    lv_obj_t *lang_dropdown = lv_dropdown_create(tab2);
    lv_dropdown_clear_options(lang_dropdown);
    lv_dropdown_add_option(lang_dropdown, "English", 0);
    lv_dropdown_add_option(lang_dropdown, "\xe4\xb8\xad\xe6\x96\x87", 1);
    lv_obj_align(lang_dropdown, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_add_event_cb(lang_dropdown, language_dropdown_change_cb, LV_EVENT_VALUE_CHANGED, NULL);
    /* Remove dropdown arrow to avoid missing FontAwesome glyph */
    lv_dropdown_set_symbol(lang_dropdown, NULL);
    /* Apply Chinese font to dropdown and its list */
    lv_obj_set_style_text_font(lang_dropdown, &my_siyuan_font, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(lang_dropdown), &my_siyuan_font, 0);
}
