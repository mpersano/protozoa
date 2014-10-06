#ifndef AUDIO_H_
#define AUDIO_H_

enum {
	MUS_STAGE_1,
	NUM_MUSIC
};

enum {
	FX_EXPLOSION_1,		/* foe explosion */
	FX_EXPLOSION_2,		/* ship explosion */
	FX_SHOT_1,		/* standard missile shot */
	FX_SHOT_2,		/* power missile shot */
	FX_SHOT_3,		/* special shot (laser or homing) */
	FX_WAVE_TRANSITION,	/* duh */
	FX_POWERUP_1,		/* standard powerup capture */
	FX_POWERUP_2,		/* extra ship powerup capture */
	FX_MENU_SELECTION,
	NUM_FX
};

void
initialize_audio(void);

void
tear_down_audio(void);

void
play_music(int index);

void
fade_music(void);

void
stop_music(void);

void
update_music(void);

void
play_fx(int index);

void
stop_fx(int index);

#endif /* AUDIO_H_ */
