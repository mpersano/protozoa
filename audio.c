#include <stdio.h>
#include <SDL.h>
#ifdef USE_OPENAL
#include <AL/alc.h>
#include <AL/al.h>
#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>
#else
#include <SDL/SDL_mixer.h>
#endif

#include "panic.h"
#include "settings.h"
#include "audio.h"

static char *music_file[NUM_MUSIC] = { "virus.ogg" };

static char *fx_file[NUM_FX] = {
  "explosion-1.wav",
  "explosion-2.wav",
  "shot-1.wav",
  "shot-2.wav",
  "shot-3.wav",
  "level-transition.wav",
  "powerup-1.wav",
  "powerup-2.wav",
  "menu-selection.wav"
};

#ifdef USE_OPENAL
enum {
	MAX_SOURCES_PER_FX = 32,
	MUSIC_BUFFER_SIZE = 8*4096,
	NUM_MUSIC_BUFFERS = 2,
	TOTAL_MUSIC_FADE_TICS = 30
};

static ALuint fx_buffers[NUM_FX];

static int fx_num_sources[NUM_FX] = { 3, 3, 1, 1, 1, 1, 1, 1, 1 };

static struct fx_source_info {
	int num_sources;
	int cur_source;
	unsigned source_bitmap;
	ALuint sources[MAX_SOURCES_PER_FX];
} fx_source_info[NUM_FX];

static ALuint music_buffers[NUM_MUSIC_BUFFERS];
static ALuint music_source;
static vorbis_info *music_vorbis_info;
static ALenum music_format;

static int music_playing;
static int music_fading;
static int music_fade_tics;

static ALCdevice *al_device;
static ALCcontext *al_context;
static FILE *ogg_file;
static OggVorbis_File ogg_stream;
#else
enum {
	AUDIO_SAMPLE_RATE = 44100,
	AUDIO_FORMAT = AUDIO_S16,
	AUDIO_CHANNELS = 1,
	AUDIO_BUFFERS = 4096
};

static int fx_channel[NUM_FX] = {
  -1, -1, 0, 0, -1, -1, -1, -1, -1,
};

static Mix_Music *music[NUM_MUSIC];

static Mix_Chunk *fx[NUM_FX];
#endif

static void
load_music(void)
{
	int i;
#ifdef USE_OPENAL
	int j;

	/* open music */

	if ((ogg_file = fopen(music_file[0], "rb")) == NULL)
		panic("failed to open %s", music_file[0]);

	if (ov_open(ogg_file, &ogg_stream, NULL, 0) < 0)
		panic("failed to open ogg stream");

	music_vorbis_info = ov_info(&ogg_stream, -1);

	music_format = music_vorbis_info->channels == 1 ?
	  AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

	alGenBuffers(NUM_MUSIC_BUFFERS, music_buffers);
	if (alGetError() != AL_NO_ERROR)
		panic("failed to create buffers");

	alGenSources(1, &music_source);
	if (alGetError() != AL_NO_ERROR)
		panic("failed to create source");

	music_playing = music_fading = 0;

	/* load effects */

	alGenBuffers(NUM_FX, fx_buffers);
	if (alGetError() != AL_NO_ERROR)
		panic("alGenBuffers failed");

	for (i = 0; i < NUM_FX; i++) {
		ALenum format = -1;
		SDL_AudioSpec wavspec;
		uint32_t wavlen;
		uint8_t *wavbuf;
		struct fx_source_info *si;

		/* load buffer */

		if (!SDL_LoadWAV(fx_file[i], &wavspec, &wavbuf, &wavlen))
			panic("failed to load %s", fx_file[i]);

		switch (wavspec.format) {
			case AUDIO_U8:
			case AUDIO_S8:
				format = wavspec.channels == 2 ?
				  AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
				break;

			case AUDIO_U16:
			case AUDIO_S16:
				format = wavspec.channels == 2 ?
				  AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
				break;

			default:
				panic("unknown wav format");
		}

		alBufferData(fx_buffers[i], format, wavbuf, wavlen, wavspec.freq);
		if (alGetError() != AL_NO_ERROR)
			panic("failed to initialize buffer");

		SDL_FreeWAV(wavbuf);

		/* create sources */

		si = &fx_source_info[i];

		si->num_sources = fx_num_sources[i];
		si->cur_source = 0;
		si->source_bitmap = 0;

		alGenSources(si->num_sources, si->sources);
		if (alGetError() != AL_NO_ERROR)
			panic("failed to create sources");

		/* attach buffer to sources */

		for (j = 0; j < si->num_sources; j++) {
			alSourcei(si->sources[j], AL_BUFFER, fx_buffers[i]);
			if (alGetError() != AL_NO_ERROR)
				panic("failed to attach source");
		}
	}
#else
	for (i = 0; i < NUM_MUSIC; i++) {
		if ((music[i] = Mix_LoadMUS(music_file[i])) == NULL)
			panic("failed to load %s", music_file[i]);
	}

	for (i = 0; i < NUM_FX; i++) {
		if ((fx[i] = Mix_LoadWAV(fx_file[i])) == NULL)
			panic("failed to load %s", fx_file[i]);

		Mix_VolumeChunk(fx[i], 80);
	}
#endif
}

void
initialize_audio(void)
{
	if (settings.static_settings->sound) {
#ifdef USE_OPENAL
		if (!(al_device = alcOpenDevice(NULL))) 
			 panic("alcOpenDevice failed");

		if (!(al_context = alcCreateContext(al_device, NULL)))
			panic("alcCreateContext failed");

		alcMakeContextCurrent(al_context);

		alGetError();
#else
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
			panic("SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s",
					SDL_GetError());

		if (Mix_OpenAudio(AUDIO_SAMPLE_RATE, AUDIO_FORMAT,
		   AUDIO_CHANNELS, AUDIO_BUFFERS) < 0)
			panic("Mix_OpenAudio failed: %s", SDL_GetError());
#endif

		load_music();
	}
}

void
tear_down_audio(void)
{
	if (settings.static_settings->sound) {
#ifdef USE_OPENAL
		struct fx_source_info *p;

		alSourceStop(music_source);

		alDeleteSources(1, &music_source);
		alDeleteBuffers(NUM_MUSIC_BUFFERS, music_buffers);

		for (p = fx_source_info; p != &fx_source_info[NUM_FX]; p++) {
			int i;

			for (i = 0; i < p->num_sources; i++)
				alSourceStop(p->sources[i]);

			alDeleteSources(p->num_sources, p->sources);
		}

		alDeleteBuffers(NUM_FX, fx_buffers);

		alcMakeContextCurrent(NULL);
		alcDestroyContext(al_context);
		alcCloseDevice(al_device);

		ov_clear(&ogg_stream);
#else
		int i;

		for (i = 0; i < NUM_MUSIC; i++)
			Mix_FreeMusic(music[i]);

		for (i = 0; i < NUM_FX; i++)
			Mix_FreeChunk(fx[i]);

		Mix_CloseAudio();
#endif
	}
}

#ifdef USE_OPENAL
int
load_ogg_data(char *buffer, int buffer_size)
{
	int size, section;

	size = 0;

	while (size < MUSIC_BUFFER_SIZE) {
		int r = ov_read(&ogg_stream, buffer + size,
		  MUSIC_BUFFER_SIZE - size, 0, 2, 1, &section);
		if (r == 0) {
			ov_raw_seek(&ogg_stream, 0);
		} else if (r < 0) {
			panic("ov_read failed");
		}
		size += r;
	}

	return size;
}
#endif

void
play_music(int index)
{
#ifdef USE_OPENAL
	if (settings.static_settings->sound && !music_playing) {
		int i;

		for (i = 0; i < NUM_MUSIC_BUFFERS; i++) {
			static char data[MUSIC_BUFFER_SIZE];
			int size;
			ALuint buffer;
			
			buffer = music_buffers[i];
			size = load_ogg_data(data, sizeof data);

			alBufferData(buffer, music_format, data, size,
			  music_vorbis_info->rate);
			if (alGetError() != AL_NO_ERROR)
				panic("buffer data failed");

			alSourceQueueBuffers(music_source, 1, &buffer);
			if (alGetError() != AL_NO_ERROR)
				panic("queue buffers failed");
		}

		alSourcef(music_source, AL_GAIN, 1);

		alSourcePlay(music_source);
		if (alGetError() != AL_NO_ERROR)
			panic("source play failed");

		music_playing = 1;
		music_fading = 0;
	}
#else
	if (settings.static_settings->sound)
		Mix_PlayMusic(music[index], -1);
#endif
}


void
update_music(void)
{
#ifdef USE_OPENAL
	if (settings.static_settings->sound) {
		if (music_fading) {
			float t = 1.f - (float)music_fade_tics/TOTAL_MUSIC_FADE_TICS;
			alSourcef(music_source, AL_GAIN, t);

			if (++music_fade_tics > TOTAL_MUSIC_FADE_TICS)
				stop_music();
		}

		if (music_playing) {
			int processed;

			alGetSourcei(music_source, AL_BUFFERS_PROCESSED, &processed);

			if (processed > 0) {
				static char data[MUSIC_BUFFER_SIZE];
				int size;

				size = load_ogg_data(data, sizeof data);

				if (size > 0) {
					ALuint buffer;

					alSourceUnqueueBuffers(music_source, 1, &buffer);

					if (buffer != 0) {
						alBufferData(buffer, music_format, data, size,
								music_vorbis_info->rate);
						if (alGetError() != AL_NO_ERROR)
							panic("buffer data failed");

						alSourceQueueBuffers(music_source, 1, &buffer);

						if (alGetError() != AL_NO_ERROR)
							panic("queue buffers failed");
					}
				}
			}
		}
	}
#endif
}

void
fade_music(void)
{
#ifdef USE_OPENAL
	music_fading = 1;
	music_fade_tics = 0;
#else
	if (settings.static_settings->sound)
		Mix_FadeOutMusic(1280);
#endif
}

void
stop_music(void)
{
	if (settings.static_settings->sound) {
#ifdef USE_OPENAL
		int queued;

		alSourceStop(music_source);

		alGetSourcei(music_source, AL_BUFFERS_QUEUED, &queued);

		while (queued--) {
			ALuint buffer;
			alSourceUnqueueBuffers(music_source, 1, &buffer);
		}

		music_playing = music_fading = 0;

		ov_raw_seek(&ogg_stream, 0);
#else
		Mix_HaltMusic();
#endif
	}
}

void
play_fx(int index)
{
	if (settings.static_settings->sound) {
#ifdef USE_OPENAL
		struct fx_source_info *si;
		ALuint source;

		si = &fx_source_info[index];
		source = si->sources[si->cur_source++];

		if (si->cur_source == si->num_sources)
			si->cur_source = 0;

		alSourceStop(source);
		alSourceRewind(source);
		alSourcePlay(source);
#else
		Mix_PlayChannel(fx_channel[index], fx[index], 0);
#endif
	}
}

void
stop_fx(int index)
{
#ifdef USE_OPENAL
#else
	if (settings.static_settings->sound)
		Mix_HaltChannel(fx_channel[index]);
#endif
}
