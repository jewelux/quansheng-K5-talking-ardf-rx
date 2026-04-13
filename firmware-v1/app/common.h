
#ifndef APP_COMMON_H
#define APP_COMMON_H

#include "functions.h"
#include "settings.h"
#include "ui/ui.h"

void COMMON_KeypadLockToggle();
void COMMON_SwitchVFOs();
void COMMON_SwitchVFOMode();

#ifdef ENABLE_VOICE
void COMMON_PlayChannelVoice(uint8_t channel_save, uint32_t frequency);
void COMMON_PlayCurrentVfoVoice(void);
#endif

#endif
