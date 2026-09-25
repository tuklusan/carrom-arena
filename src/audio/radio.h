#ifndef CARROM_RADIO_H
#define CARROM_RADIO_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The in-game radio: plays the AH.FM stream through the audio device. Starts playing by itself. The play/pause button
 * (radio_toggle) is the player's: after a manual pause the radio stays paused; when the stream merely fails it pauses,
 * keeps trying to reconnect, and resumes by itself as soon as the stream is back. Needs audio_init() to have succeeded
 * (otherwise every call is a no-op and radio_available() is false). */
void radio_init(void);
void radio_shutdown(void);
void radio_update(void);          /* once per frame: moves decoded audio to the device */
void radio_toggle(void);          /* the play/pause button */
bool radio_available(void);       /* an audio device and a stream worker exist: show the icon */
bool radio_is_playing(void);      /* audio is coming out (the button then shows "pause") */

#ifdef __cplusplus
}
#endif

#endif /* CARROM_RADIO_H */
