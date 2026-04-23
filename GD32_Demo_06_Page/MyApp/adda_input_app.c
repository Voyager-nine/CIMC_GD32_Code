#include "adda_input_app.h"

#include "dac_app.h"
#include "usart_app.h"

enum
{
    ADDA_INPUT_KEY_FREQ_DOWN = 1U,
    ADDA_INPUT_KEY_FREQ_UP,
    ADDA_INPUT_KEY_WAVE_SWITCH,
    ADDA_INPUT_KEY_PRINT_TOGGLE,
    ADDA_INPUT_KEY_PRINT_STATUS,
    ADDA_INPUT_KEY_RESET_DEFAULT,
};

void adda_input_handle_press(uint16_t key_id)
{
    const wave_ctrl_t *ctrl = dac_get_ctrl();

    if (ctrl == NULL)
    {
        return;
    }

    switch (key_id)
    {
    case ADDA_INPUT_KEY_FREQ_DOWN:
        wave_set_frequency(ctrl->freq_hz - 10.0f);
        break;

    case ADDA_INPUT_KEY_FREQ_UP:
        wave_set_frequency(ctrl->freq_hz + 10.0f);
        break;

    case ADDA_INPUT_KEY_WAVE_SWITCH:
        if (ctrl->wave_type == WAVE_TYPE_TRIANGLE)
        {
            wave_set_type(WAVE_TYPE_SINE);
        }
        else
        {
            wave_set_type((wave_type_t)(ctrl->wave_type + 1));
        }
        break;

    case ADDA_INPUT_KEY_PRINT_TOGGLE:
        dac_toggle_print_enable();
        break;

    case ADDA_INPUT_KEY_PRINT_STATUS:
        uart_print_status();
        break;

    case ADDA_INPUT_KEY_RESET_DEFAULT:
        wave_set_type(WAVE_TYPE_SINE);
        wave_set_frequency(100.0f);
        wave_set_vpp(1.0f);
        dac_set_print_enable(0U);
        break;

    default:
        break;
    }
}
