// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "analog.h"
#include "midi.h"

#define ADC_PIN {GP29,GP28,GP27,GP26}
#define LEDS_PIN {GP10,GP11,GP12,GP13}

const pin_t buttonledsPIN[4] = LEDS_PIN;
const pin_t analogPIN[4] = ADC_PIN;
int8_t currentValue[4] = {0};

bool     layer_toggle_reset = true;
uint16_t flash_timer        = 0;
uint8_t  flash_count        = 0;     // Keeps track of how many flashes remaining
bool     flash_state_on     = false; // Tracks if the flash is currently "ON" (Red) or "OFF" (Layer color)
#define FLASH_DURATION 300           // Duration of each flash phase in milliseconds


#define ADC_CHANGE_THRESHOLD 2 //Minimum needed for update
extern MidiDevice midi_device;

enum layers{
    WINBASE,
    MIDIBASE
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [WINBASE] = LAYOUT(
        KC_MPRV,   KC_MPLY,   KC_MNXT, KC_F19,
        KC_F18,    KC_F17,    KC_F16,  KC_F20,
        KC_F15,    KC_F14,    KC_F13,  KC_F21,
        KC_NO,     KC_NO,     KC_NO,   KC_F22
    ),
    [MIDIBASE] = LAYOUT(
        MI_C2,    MI_B1,    MI_As1,   MI_A1,
        MI_Gs1,   MI_G1,    MI_Fs1,   MI_F1,
        MI_E1,    MI_Ds1,   MI_D1,    MI_Cs1,
        KC_NO,    KC_NO,    KC_NO,    MI_Cs2

    )
    
};

bool dip_switch_update_user(uint8_t index, bool active) { 
    // Your specific mapping: Rows 0-3, Column 3
    uint8_t dummy_row = index;
    uint8_t dummy_col = 3;

    // Fetch the keycode dynamically assigned by Vial on the highest active layer
    uint16_t dynamic_keycode = keymap_key_to_keycode(get_highest_layer(default_layer_state), (keypos_t){.row = dummy_row, .col = dummy_col});

    if (IS_QK_MIDI(dynamic_keycode)) {
        
        // Construct a synthetic mock matrix state record for QMK's internal layout processor
        keyrecord_t mock_record;
        mock_record.event.pressed = active;
        mock_record.event.key     = (keypos_t){.row = dummy_row, .col = dummy_col};
        mock_record.event.time    = timer_read();

        process_midi(dynamic_keycode, &mock_record);
        
    } 
    else {
        if (active) {
            register_code16(dynamic_keycode);
        } else {
            unregister_code16(dynamic_keycode);
        }
    }

    gpio_write_pin(buttonledsPIN[index], active);
    return true;
}

void check_layer_combo(void) {
    // Check physical switches: Top-Left (0,0) and Bottom-Right (2,2)
    if (matrix_is_on(0,0) && matrix_is_on(2, 2)) {
        if (layer_toggle_reset) {
            
            // Persistent layer switching toggle
            if (get_highest_layer(default_layer_state) == MIDIBASE) {
                set_single_persistent_default_layer(WINBASE);
                layer_move(WINBASE);
            } else {
                set_single_persistent_default_layer(MIDIBASE);
                layer_move(MIDIBASE);
            }
            
            flash_count = 3 * 2;        // times to to get on and off
            flash_state_on = true;
            flash_timer = timer_read(); 
            
            layer_toggle_reset = false; // Lock out until keys are released
        }
    } else {
        layer_toggle_reset = true;      // Reset flag when keys are released
    }
}

void process_flash_sequence(void) {
    if (flash_count > 0) {
        // Check if it's time to flip the flash state
        if (timer_elapsed(flash_timer) > FLASH_DURATION) {
            flash_state_on = !flash_state_on; // Flip between true and false
            flash_count--;                    // Decrement remaining transitions
            flash_timer = timer_read();       // Reset the timer for the next phase
        }
    }
}

void read_ADC(void) {
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t rawData = analogReadPin(analogPIN[i]);
        uint8_t convertedData = rawData * 127 / 1024; // 0–127
        if (convertedData > 127 - ADC_CHANGE_THRESHOLD) convertedData = 127;
        else if (convertedData < ADC_CHANGE_THRESHOLD) convertedData = 0;

        if (abs(convertedData - currentValue[i]) >= ADC_CHANGE_THRESHOLD) {
            currentValue[i] = convertedData;
            convertedData = 127 - convertedData; // Invert Signal for first revision 
            midi_send_cc(&midi_device, midi_config.channel, i, convertedData);
        }
    }
}

void matrix_scan_user(void) {
    check_layer_combo();      // Listen for the layer hotkeys
    process_flash_sequence(); // Track time for non-blocking flash blinks
    read_ADC();               // Process pots
}

// Modern QMK standard function for handling RGB matrix layers and overrides smoothly
bool rgb_matrix_indicators_user(void) {
    if (flash_count > 0 && flash_state_on) {
        rgb_matrix_set_color_all(255, 0, 0);
        return false;
    }
    return false;
}

void keyboard_post_init_user(void) {
    gpio_set_pin_output(GP10);
    gpio_set_pin_output(GP11);
    gpio_set_pin_output(GP12);
    gpio_set_pin_output(GP13);
}