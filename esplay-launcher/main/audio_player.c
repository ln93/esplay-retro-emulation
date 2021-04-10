#include <audio_player.h>

#include <event.h>
#include <file_ops.h>
#include <gamepad.h>
#include <audio.h>
#include <display.h>
#include <ugui.h>
#include <esplay-ui.h>
#include <graphics.h>
#include <settings.h>
#include <power.h>

#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <acodecs.h>

#define MAX_FILENAME 40
extern uint8_t batlevel;
// TODO: Move these
static AudioCodec choose_codec(FileType ftype)
{
	switch (ftype)
	{
	case FileTypeMOD:
		return AudioCodecMOD;
	case FileTypeMP3:
		return AudioCodecMP3;
	case FileTypeOGG:
		return AudioCodecOGG;
	case FileTypeFLAC:
		return AudioCodecFLAC;
	case FileTypeWAV:
		return AudioCodecWAV;
	case FileTypeGME:
		return AudioCodecGME;
	default:
		return AudioCodecUnknown;
	}
}

typedef struct
{
	char *filename;
	char *filepath;
	AudioCodec codec;
	// TODO: Add more information in later versions
} Song;

// Cmds sent to player task for control
typedef enum PlayerCmd
{
	PlayerCmdNone,
	PlayerCmdTerminate,

	PlayerCmdPause,
	PlayerCmdNext,
	PlayerCmdPrev,
	PlayerCmdReinitAudio,
	PlayerCmdToggleLoopMode,
} PlayerCmd;

typedef enum PlayingMode
{
	PlayingModeNormal = 0,
	PlayingModeRepeatSong,
	PlayingModeRepeatPlaylist,
	PlayingModeMax,
} PlayingMode;

static const char *playing_mode_str[PlayingModeMax] = {"普通", "单曲循环", "全部循环"};

// Owned by player task, can only be modified/written to by player task
typedef struct PlayerState
{
	// Can be modified though Playing/Pause Cmd
	bool playing;

	// Can be modified? by SetPlayList Cmd?
	// NOTE: Maybe make playlist a more dynamic array for later versions
	Song *playlist;
	size_t playlist_length;

	// Marks current song index in playlist
	// Can be modified through PlayNext/PlayPrev
	int playlist_index;

	// TODO: Settings, should probably saved
	PlayingMode playing_mode;
} PlayerState;
static PlayerState player_state = {
	0,
};
static bool keys_locked = false;
static bool backlight_on = true;
static bool speaker_on = false;

// These need to be implemented for SDL/FreeRTOS seperately
static PlayerCmd player_poll_cmd(void);
static void player_send_cmd(PlayerCmd cmd);
static void player_cmd_ack(void);
static void player_start(void);
static void player_terminate(void);
static void player_teardown_task(void);

static void player_task(void *arg);

static bool player_task_running = false;

static QueueHandle_t player_cmd_queue;
static QueueHandle_t player_ack_queue;
static TaskHandle_t audio_player_task_handle;

static PlayerCmd player_poll_cmd(void)
{
	PlayerCmd polled_cmd = PlayerCmdNone;

	xQueueReceive(player_cmd_queue, &polled_cmd, 0);
	return polled_cmd;
}

static void player_send_cmd(PlayerCmd cmd)
{
	xQueueSend(player_cmd_queue, &cmd, 0);
	int tmp;
	xQueueReceive(player_ack_queue, &tmp, 10 / portTICK_PERIOD_MS);
}

static void player_cmd_ack(void)
{
	int tmp = 42;
	xQueueSend(player_ack_queue, &tmp, 0);
}

static void player_start(void)
{
	const int stacksize = 9 * 8192; // dr_mp3 uses a lot of stack memory
	if (xTaskCreate(player_task, "player_task", stacksize, NULL, 5, &audio_player_task_handle) != pdPASS)
	{
		printf("Error creating task\n");
		return;
	}
	player_cmd_queue = xQueueCreate(3, sizeof(PlayerCmd));
	player_ack_queue = xQueueCreate(3, sizeof(int));
}

static void player_terminate(void)
{
	if (!player_task_running)
	{
		return;
	}
	printf("Trying to terminate player..\n");
	PlayerCmd term = PlayerCmdTerminate;
	xQueueSend(player_cmd_queue, &term, portMAX_DELAY);
	while (player_task_running)
	{
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
	printf("Terminated?\n");
}

static void player_teardown_task(void) { vTaskDelete(NULL); }

typedef enum PlayerResult
{
	PlayerResultNone = 0,
	PlayerResultError,

	PlayerResultDone,	  ///< Playing the song completed, same effect as NextSong
	PlayerResultNextSong, ///< User requested skipping of next song
	PlayerResultPrevSong, ///< User requested to play previous song
	PlayerResultStop,	  ///< User exited player/requested player termination
} PlayerResult;

static void push_audio_event(const AudioPlayerEvent audio_event)
{
	event_t ev = {.audio_player.head.type = EVENT_TYPE_AUDIO_PLAYER, .audio_player.event = audio_event};
	push_event(&ev);
}
static void push_update_event()
{
	event_t ev = {.audio_player.head.type = EVENT_TYPE_UPDATE};
	push_event(&ev);
}

/// Handle a playercmd, return 1 if we should stop?
static PlayerResult handle_cmd(PlayerState *const state, const AudioInfo info, const PlayerCmd received_cmd)
{
	if (received_cmd == PlayerCmdNone)
	{
		return PlayerResultDone;
	}
	fprintf(stderr, "Received cmd: %d\n", received_cmd);
	PlayerResult res = PlayerResultDone;
	switch (received_cmd)
	{
	case PlayerCmdPause:
		state->playing = !state->playing;
		if (state->playing)
		{
			audio_init((int)info.sample_rate);
			speaker_on ? audio_amp_enable() : audio_amp_disable();
		}
		else
		{
			audio_terminate();
		}
		push_audio_event(AudioPlayerEventStateChanged);
		break;
	case PlayerCmdReinitAudio:
		if (state->playing)
			audio_terminate();
		audio_init((int)info.sample_rate);
		speaker_on ? audio_amp_enable() : audio_amp_disable();
		push_audio_event(AudioPlayerEventStateChanged);
		break;
	case PlayerCmdToggleLoopMode:
		state->playing_mode = (state->playing_mode + 1) % PlayingModeMax;
		settings_save(SettingPlayingMode, (int32_t)state->playing_mode);
		push_audio_event(AudioPlayerEventStateChanged);
		break;
	case PlayerCmdTerminate:
		res = PlayerResultStop;
		break;
	case PlayerCmdNext:
		res = PlayerResultNextSong;
		break;
	case PlayerCmdPrev:
		res = PlayerResultPrevSong;
		break;
	case PlayerCmdNone:
		break;
	}
	player_cmd_ack();
	return res;
}

static PlayerResult play_song(const Song *const song)
{
	PlayerState *state = &player_state;
	AudioInfo info;
	void *acodec = NULL;
	static unsigned int cou = 0;

	printf("Playing file: %s, codec: %d\n", song->filepath, song->codec);
	AudioDecoder *decoder = acodec_get_decoder(song->codec);
	if (decoder == NULL)
	{
		printf("error determining deocer for song %s\n", song->filepath);
		return PlayerResultError;
	}

	if (decoder->open(&acodec, song->filepath) != 0)
	{
		printf("error opening song %s\n", song->filepath);
		return PlayerResultError;
	}
	if (decoder->get_info(acodec, &info) != 0)
	{
		decoder->close(acodec);
		printf("error retreiving song info %s\n", song->filepath);
		return PlayerResultError;
	}

	int16_t *audio_buf = calloc(1, info.buf_size * sizeof(uint16_t));
	if (audio_buf == NULL)
	{
		decoder->close(acodec);
		printf("error allocating audio buffer\n");
		return PlayerResultError;
	}

	audio_init((int)info.sample_rate);
	speaker_on ? audio_amp_enable() : audio_amp_disable();

	int n_frames = 0;
	state->playing = true;
	printf("Pushing audio event...\n");
	push_audio_event(AudioPlayerEventStateChanged);

	PlayerResult result = PlayerResultDone;
	printf("starting to play audio...\n");
	do
	{

		// 	cou++;
		// if (cou>20)
		// {
		// 	cou=0;
		// 	//draw_player(&player_state);
		// 	if (batlevel>4)
		// 	{
		// 		drawBattery(100);
		// 	}
		// 	else
		// 	{
		// 		drawBattery(batlevel*25);
		// 	}
		// }
		// React on user control
		if ((result = handle_cmd(state, info, player_poll_cmd())) != PlayerResultDone)
		{
			break;
		}

		// Play if not paused
		// TODO: Fix mono playback
		if (state->playing)
		{
			n_frames = decoder->decode(acodec, audio_buf, (int)info.channels, info.buf_size);
			audio_submit(audio_buf, n_frames);
			push_update_event();
		}
		else
		{
			usleep(10 * 1000);
		}
	} while (n_frames > 0);

	// TODO: Notify user that we are done
	decoder->close(acodec);
	free(audio_buf);

	if (state->playing)
	{
		audio_terminate();
	}
	return result;
}

/** This task/thread plays a playlist of files automatically. */
static void player_task(void *arg)
{
	player_task_running = true;
	player_state.playing = false;
	struct PlayerState *state = &player_state;
	printf("Playing playlist of length: %zu\n", state->playlist_length);

	for (;;)
	{
		int song_index = state->playlist_index;
		Song *const song = &state->playlist[song_index];
		PlayerResult res = play_song(song);

		if (res == PlayerResultDone || res == PlayerResultNextSong)
		{
			if (state->playing_mode == PlayingModeNormal && song_index == (int)(state->playlist_length - 1))
			{
				// Reached last song in playlist. Quit player.
				push_audio_event(AudioPlayerEventDone);
				break;
			}

			if (state->playing_mode != PlayingModeRepeatSong || res == PlayerResultNextSong)
			{
				song_index = (song_index + 1) % (int)state->playlist_length;
			}

			state->playlist_index = song_index;
			push_audio_event(AudioPlayerEventStateChanged);
		}
		else if (res == PlayerResultPrevSong)
		{
			if (--song_index < 0)
			{
				song_index = (int)state->playlist_length - 1;
			}
			state->playlist_index = song_index;
			push_audio_event(AudioPlayerEventStateChanged);
		}
		else if (res == PlayerResultStop)
		{
			push_audio_event(AudioPlayerEventDone);
			break;
		}
		else if (res == PlayerResultError)
		{
			push_audio_event(AudioPlayerEventError);
			break;
		}
	}

	player_task_running = false;
	player_teardown_task();
}

static void draw_player(const PlayerState *const state, int cycle)
{
	ui_clear_screen();
	renderGraphics(0, 0, 0, 346, 240, 192);

	UG_FontSelect(&FONT_8X12);

	int32_t volume;
	settings_load(SettingAudioVolume, &volume);
	char volStr[3];
	sprintf(volStr, "%i", volume);

	UG_SetForecolor(ui_get_word_color());
	UG_SetBackcolor(ui_get_title_color());

	UG_PutString(30, 8, volStr);

	UG_SetForecolor(ui_get_word_color());
	UG_SetBackcolor(ui_get_title_color());

	battery_state bat_state;
	battery_level_read(&bat_state);

	drawVolume(volume);
	drawBattery(bat_state.percentage);

	const int line_height = 16;
	short y = 34;

	char str_buf[300];
	Song *song = &state->playlist[state->playlist_index];

	char truncnm[MAX_FILENAME];
	// Song name
	strncpy(truncnm, song->filename, MAX_FILENAME);
	truncnm[MAX_FILENAME - 1] = 0;
	snprintf(str_buf, 300, "%s", truncnm);
	UG_FontSelect(&FONT_8X12);
	//UG_PutSingleString(3, y, str_buf);
	UG_PutCycleSingleString(24, cycle, 20, y, str_buf); //use cycle display instead,size=24 EngChar or 12 chinese char
	y += 20;
	// Song playmode
	snprintf(str_buf, 300, "播放模式: %s", playing_mode_str[state->playing_mode]);
	UG_PutString(20, y, str_buf);
	y += 18;

	// Show volume
	snprintf(str_buf, 300, "音量: %d%%", audio_volume_get());
	UG_PutString(20, y, str_buf);

	//Generate waveValue(fake)
	char waveValue[11];
	for (int i = 0; i < 14; i++)
	{
		waveValue[i] = (rand() % 5) + 1;
	}
	drawWave(180, 70, 11, waveValue);
	ui_flush();
}

static void handle_keypress(event_keypad_t keys, bool *quit)
{
	if (!keys.last_state.values[GAMEPAD_INPUT_A] && keys.state.values[GAMEPAD_INPUT_A])
		player_send_cmd(PlayerCmdPause);
	if (!keys.last_state.values[GAMEPAD_INPUT_B] && keys.state.values[GAMEPAD_INPUT_B])
		*quit = true;
	if (!keys.last_state.values[GAMEPAD_INPUT_UP] && keys.state.values[GAMEPAD_INPUT_UP])
	{
		int vol = audio_volume_get() + 5;
		if (vol > 99)
			vol = 99;
		audio_volume_set(vol);
		settings_save(SettingAudioVolume, (int32_t)vol);
		draw_player(&player_state, 0);
	}
	if (!keys.last_state.values[GAMEPAD_INPUT_DOWN] && keys.state.values[GAMEPAD_INPUT_DOWN])
	{
		int vol = audio_volume_get() - 5;
		if (vol < 1)
			vol = 1;
		audio_volume_set(vol);

		if (vol == 1)
			audio_terminate();
		settings_save(SettingAudioVolume, (int32_t)vol);
		draw_player(&player_state, 0);
	}
	if (!keys.last_state.values[GAMEPAD_INPUT_RIGHT] && keys.state.values[GAMEPAD_INPUT_RIGHT])
		player_send_cmd(PlayerCmdNext);
	if (!keys.last_state.values[GAMEPAD_INPUT_LEFT] && keys.state.values[GAMEPAD_INPUT_LEFT])
		player_send_cmd(PlayerCmdPrev);
	if (!keys.last_state.values[GAMEPAD_INPUT_START] && keys.state.values[GAMEPAD_INPUT_START])
		player_send_cmd(PlayerCmdToggleLoopMode);
	if (!keys.last_state.values[GAMEPAD_INPUT_SELECT] && keys.state.values[GAMEPAD_INPUT_SELECT])
	{
		set_display_brightness(backlight_on ? -5 : 50);
		backlight_on = !backlight_on;
	}
	if (!keys.last_state.values[GAMEPAD_INPUT_L] && keys.state.values[GAMEPAD_INPUT_L])
	{
		speaker_on = !speaker_on;
		speaker_on ? audio_amp_enable() : audio_amp_disable();
		draw_player(&player_state, 0);
	}
}

#define MAX_SONGS 1024

/// Make playlist out of entries in params and write them to state.
static int make_playlist(PlayerState *state, const AudioPlayerParam params)
{
	char pathbuf[PATH_MAX];

	if (params.play_all)
	{
		size_t start_song = 0;
		size_t n_songs = 0;
		size_t song_indices[MAX_SONGS];
		for (size_t i = 0, j = 0; i < (size_t)params.n_entries; i++)
		{
			Entry *entry = &params.entries[i];
			AudioCodec codec = choose_codec(fops_determine_filetype(entry));
			if (codec != AudioCodecUnknown)
			{
				if ((size_t)params.index == i)
				{
					start_song = j;
				}
				song_indices[j++] = i;
				n_songs++;
			}
		}

		state->playlist = malloc(n_songs * sizeof(Song));
		state->playlist_length = n_songs;

		for (size_t i = 0; i < n_songs; i++)
		{
			Entry *entry = &params.entries[song_indices[i]];

			AudioCodec codec = choose_codec(fops_determine_filetype(entry));
			state->playlist[i].codec = codec;

			int printed = snprintf(pathbuf, PATH_MAX, "%s/%s", params.cwd, entry->name);
			state->playlist[i].filename = malloc(strlen(entry->name) + 1);
			state->playlist[i].filepath = malloc((size_t)(printed + 1));
			strncpy(state->playlist[i].filename, entry->name, (size_t)(strlen(entry->name) + 1));
			strncpy(state->playlist[i].filepath, pathbuf, (size_t)(printed + 1));
		}
		state->playlist_index = (int)start_song;
	}
	else
	{
		Entry *entry = &params.entries[params.index];
		AudioCodec codec = choose_codec(fops_determine_filetype(entry));
		if (codec == AudioCodecUnknown)
		{
			return -1;
		}

		state->playlist = malloc(1 * sizeof(Song));
		state->playlist[0].codec = codec;
		state->playlist_length = 1;
		int printed = snprintf(pathbuf, PATH_MAX, "%s/%s", params.cwd, entry->name);
		state->playlist[0].filepath = malloc((size_t)(printed + 1));
		strncpy(state->playlist[0].filepath, pathbuf, (size_t)(printed + 1));
	}

	return 0;
}

/// Free previously allocated/created playlist.
static void free_playlist(PlayerState *state)
{
	const size_t len = state->playlist_length;
	if (len < 1)
		return;
	for (size_t i = 0; i < len; i++)
	{
		free(state->playlist[i].filepath);
		free(state->playlist[i].filename);
	}
	free(state->playlist);
}

static void load_settings(PlayerState *state)
{
	int32_t mode;
	if (settings_load(SettingPlayingMode, &mode) == 0)
	{
		if (mode >= 0 && mode <= PlayingModeMax)
			state->playing_mode = (PlayingMode)mode;
	}
}

int audio_player(AudioPlayerParam params)
{

	event_init();
	memset(&player_state, 0, sizeof(PlayerState));
	load_settings(&player_state);

	if (make_playlist(&player_state, params) != 0)
	{
		printf("Could not determine audio codec\n");
		return -1;
	}

	// Start playing the slected file
	player_start();

	bool quit = false;
	event_t event;
	unsigned int cycle = 0;
	while (!quit)
	{
		// Handle inputs
		if (wait_event(&event) < 0)
		{
			draw_player(&player_state, 0);
			continue;
		}
		switch (event.type)
		{
		case EVENT_TYPE_KEYPAD:
			handle_keypress(event.keypad, &quit);
			break;
		case EVENT_TYPE_AUDIO_PLAYER:
			if (event.audio_player.event == AudioPlayerEventDone)
			{
				quit = true;
			}
			else if (event.audio_player.event == AudioPlayerEventError)
			{
				printf("Error playing audio file\n");
				quit = true;
			}
			else
			{
				draw_player(&player_state, 0);
			}
			break;
		case EVENT_TYPE_QUIT:
			quit = true;
			break;
		case EVENT_TYPE_UPDATE:
			cycle++;
			if (cycle % 10 == 0)
				draw_player(&player_state, cycle / 10);
			break;
		}
	}

	player_terminate();
	free_playlist(&player_state);
	keys_locked = false;
	event_deinit();
	//set_display_brightness(50);

	return 0;
}
