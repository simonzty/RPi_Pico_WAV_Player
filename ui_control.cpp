/*------------------------------------------------------/
/ Copyright (c) 2020-2021, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include <cstdio>
#include "hardware/adc.h"
#include "st7735_80x160/lcd.h"
extern "C" {
#include "pico/util/queue.h"
}
#include "ui_control.h"

static const uint32_t RELEASE_IGNORE_COUNT = 8;
static const uint32_t LONG_PUSH_COUNT = 10;
static const uint32_t LONG_LONG_PUSH_COUNT = 30;

static button_status_t button_prv[NUM_BTN_HISTORY] = {}; // initialized as HP_BUTTON_OPEN
static uint32_t button_repeat_count = LONG_LONG_PUSH_COUNT; // to ignore first buttton press when power-on

static queue_t btn_evt_queue;
static const int QueueLength = 1;
static repeating_timer_t timer;

UIVars vars;
UIMode *ui_mode = nullptr;
UIMode *ui_mode_ary[3] = {};

static button_status_t adc0_get_hp_button(void)
{
    // ADC Calibration Coefficients
    const int16_t coef_a = 3350;
    const int16_t coef_b = -50;
    uint16_t result = adc_read();
    int16_t voltage = result * coef_a / (1<<12) + coef_b;
    //if (voltage < 1000) { printf("adc0 = %d mv\n", voltage); }
    button_status_t ret;
    // 3.3V support
    if (voltage < 100) { // < 100mV  4095*100/3300 (CENTER)
        ret = ButtonCenter;
    } else if (voltage >= 142 && voltage < 238) { // 142mv ~ 238mV (D: 190mV)
        ret = ButtonD;
    } else if (voltage >= 240 && voltage < 400) { // 240mV ~ 400mV (PLUS: 320mV)
        ret = ButtonPlus;
    } else if (voltage >= 435 && voltage < 725) { // 435mV ~ 725mV (MINUS: 580mV)
        ret = ButtonMinus;
    } else { // others
        ret = ButtonOpen;
    }
    return ret;
}

static int count_center_clicks(void)
{
    int i;
    int detected_fall = 0;
    int count = 0;
    for (i = 0; i < 4; i++) {
        if (button_prv[i] != ButtonOpen) {
            return 0;
        }
    }
    for (i = 4; i < NUM_BTN_HISTORY; i++) {
        if (detected_fall == 0 && button_prv[i-1] == ButtonOpen && button_prv[i] == ButtonCenter) {
            detected_fall = 1;
        } else if (detected_fall == 1 && button_prv[i-1] == ButtonCenter && button_prv[i] == ButtonOpen) {
            count++;
            detected_fall = 0;
        }
    }
    if (count > 0) {
        for (i = 0; i < NUM_BTN_HISTORY; i++) button_prv[i] = ButtonOpen;
    }
    return count;
}

static void trigger_event(button_action_t button_action)
{
    element_t element = {
        .button_action = button_action
    };
    if (!queue_try_add(&btn_evt_queue, &element)) {
        //printf("FIFO was full\n");
    }
    return;
}

static void update_button_action()
{
    int i;
    int center_clicks;
    button_status_t button = adc0_get_hp_button();
    if (button == ButtonOpen) {
        // Ignore button release after long push
        if (button_repeat_count > LONG_PUSH_COUNT) {
            for (i = 0; i < NUM_BTN_HISTORY; i++) {
                button_prv[i] = ButtonOpen;
            }
            button = ButtonOpen;
        }
        button_repeat_count = 0;
        if (button_prv[RELEASE_IGNORE_COUNT] == ButtonCenter) { // center release
            center_clicks = count_center_clicks(); // must be called once per tick because button_prv[] status has changed
            switch (center_clicks) {
                case 1:
                    trigger_event(ButtonCenterSingle);
                    break;
                case 2:
                    trigger_event(ButtonCenterDouble);
                    break;
                case 3:
                    trigger_event(ButtonCenterTriple);
                    break;
                default:
                    break;
            }
        }
    } else if (button_prv[0] == ButtonOpen) { // push
        if (button == ButtonD || button == ButtonPlus) {
            trigger_event(ButtonPlusSingle);
        } else if (button == ButtonMinus) {
            trigger_event(ButtonMinusSingle);
        }
    } else if (button_repeat_count == LONG_PUSH_COUNT) { // long push
        if (button == ButtonCenter) {
            trigger_event(ButtonCenterLong);
            button_repeat_count++; // only once and step to longer push event
        } else if (button == ButtonD || button == ButtonPlus) {
            trigger_event(ButtonPlusLong);
        } else if (button == ButtonMinus) {
            trigger_event(ButtonMinusLong);
        }
    } else if (button_repeat_count == LONG_LONG_PUSH_COUNT) { // long long push
        if (button == ButtonCenter) {
            trigger_event(ButtonCenterLongLong);
        }
        button_repeat_count++; // only once and step to longer push event
    } else if (button == button_prv[0]) {
        button_repeat_count++;
    }
    // Button status shift
    for (i = NUM_BTN_HISTORY-2; i >= 0; i--) {
        button_prv[i+1] = button_prv[i];
    }
    button_prv[0] = button;
}

bool timer_callback_adc(repeating_timer_t *rt) {
    update_button_action();
    return true; // keep repeating
}

static int adc_timer_init()
{
    // ADC Initialize
    adc_init();
    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(26);
    // Select ADC input 0 (GPIO26)
    adc_select_input(0);

    const int timer_hz = 20;
    // negative timeout means exact delay (rather than delay between callbacks)
    if (!add_repeating_timer_us(-1000000 / timer_hz, timer_callback_adc, nullptr, &timer)) {
        //printf("Failed to add timer\n");
        return 0;
    }

    return 1;
}

bool ui_get_btn_evt(button_action_t *btn_act)
{
    int count = queue_get_level(&btn_evt_queue);
    if (count) {
        element_t element;
        queue_remove_blocking(&btn_evt_queue, &element);
        *btn_act = element.button_action;
        return true;
    }
    return false;
}

void ui_clear_btn_evt()
{
    // queue doesn't work as intended when removing rest items after removed or poke once
    // Therefore set QueueLength = 1 at main.cpp instead of removing here
    /*
    int count = queue_get_level(btn_evt_queue);
    while (count) {
        element_t element;
        queue_remove_blocking(btn_evt_queue, &element);
        count--;
    }
    */
}

UIMode *getUIMode(ui_mode_enm_t ui_mode_enm)
{
    return ui_mode_ary[ui_mode_enm];
}

void ui_init(ui_mode_enm_t init_dest_ui_mode, stack_t *dir_stack, uint8_t fs_type)
{
    vars.init_dest_ui_mode = init_dest_ui_mode;
    vars.fs_type = fs_type;
    vars.num_list_lines = LCD_H/16;

    // ADC and Timer setting
    adc_timer_init();

    // button event queue
    queue_init(&btn_evt_queue, sizeof(element_t), QueueLength);

    ui_mode_ary[InitialMode]  = (UIMode *) new UIInitialMode(&vars);
    ui_mode_ary[FileViewMode] = (UIMode *) new UIFileViewMode(&vars, dir_stack);
    ui_mode_ary[PlayMode]     = (UIMode *) new UIPlayMode(&vars);
    ui_mode = getUIMode(InitialMode);
    ui_mode->entry(ui_mode);
}

void ui_update()
{
    //printf("%s\n", ui_mode->getName());
    UIMode *ui_mode_next = ui_mode->update();
    if (ui_mode_next != ui_mode) {
        ui_mode_next->entry(ui_mode);
        ui_mode = ui_mode_next;
    } else {
        ui_mode->draw();
    }
}

void ui_force_update(ui_mode_enm_t ui_mode_enm)
{
    //printf("%s\n", ui_mode->getName());
    ui_mode->update();
    UIMode *ui_mode_next = getUIMode(ui_mode_enm);
    if (ui_mode_next != ui_mode) {
        ui_mode_next->entry(ui_mode);
        ui_mode = ui_mode_next;
    } else {
        ui_mode->draw();
    }
}