/*
 * LiMe Multimedia Manager
 * Audio and video playback, volume control, device management
 * Supports PulseAudio, ALSA, GStreamer with automatic device detection
 */

#ifndef __LIME_MULTIMEDIA_MANAGER_H__
#define __LIME_MULTIMEDIA_MANAGER_H__

#include <glib-object.h>
#include <gst/gst.h>

G_BEGIN_DECLS

#define LIME_TYPE_MULTIMEDIA_MANAGER (lime_multimedia_manager_get_type())
#define LIME_MULTIMEDIA_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_MULTIMEDIA_MANAGER, LiMeMultimediaManager))
#define LIME_IS_MULTIMEDIA_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_MULTIMEDIA_MANAGER))

typedef struct _LiMeMultimediaManager LiMeMultimediaManager;
typedef struct _LiMeMultimediaManagerClass LiMeMultimediaManagerClass;

typedef enum {
  LIME_PLAYBACK_STATE_STOPPED,
  LIME_PLAYBACK_STATE_PAUSED,
  LIME_PLAYBACK_STATE_PLAYING
} LiMePlaybackState;

typedef enum {
  LIME_MEDIA_TYPE_AUDIO,
  LIME_MEDIA_TYPE_VIDEO,
  LIME_MEDIA_TYPE_PLAYLIST
} LiMeMediaType;

typedef enum {
  LIME_AUDIO_DEVICE_TYPE_SPEAKERS,
  LIME_AUDIO_DEVICE_TYPE_HEADPHONES,
  LIME_AUDIO_DEVICE_TYPE_MICROPHONE,
  LIME_AUDIO_DEVICE_TYPE_LINE_IN,
  LIME_AUDIO_DEVICE_TYPE_BLUETOOTH
} LiMeAudioDeviceType;

typedef enum {
  LIME_REPEAT_MODE_NONE,
  LIME_REPEAT_MODE_ONE,
  LIME_REPEAT_MODE_ALL
} LiMeRepeatMode;

typedef struct {
  gchar *device_id;
  gchar *device_name;
  gchar *description;
  LiMeAudioDeviceType device_type;
  gint channels;
  gint sample_rate;
  gdouble volume;
  gboolean is_available;
  gboolean is_default;
  gboolean is_muted;
} LiMeAudioDevice;

typedef struct {
  gchar *track_id;
  gchar *title;
  gchar *artist;
  gchar *album;
  gchar *genre;
  gchar *file_path;
  guint64 duration;
  guint64 current_position;
  gchar *album_art_url;
  gint year;
  gint track_number;
  gdouble rating;
  LiMeMediaType media_type;
} LiMeMediaTrack;

typedef struct {
  gchar *playlist_id;
  gchar *playlist_name;
  GList *tracks;
  guint track_count;
  gint current_track_index;
} LiMePlaylist;

GType lime_multimedia_manager_get_type(void);

LiMeMultimediaManager * lime_multimedia_manager_new(void);

/* Playback control */
gboolean lime_multimedia_manager_play_file(LiMeMultimediaManager *manager,
                                           const gchar *file_path);
gboolean lime_multimedia_manager_play_track(LiMeMultimediaManager *manager,
                                            LiMeMediaTrack *track);
gboolean lime_multimedia_manager_pause(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_resume(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_stop(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_next_track(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_previous_track(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_seek_to(LiMeMultimediaManager *manager,
                                         guint64 position_milliseconds);

/* Playback status */
LiMePlaybackState lime_multimedia_manager_get_playback_state(LiMeMultimediaManager *manager);
LiMeMediaTrack * lime_multimedia_manager_get_current_track(LiMeMultimediaManager *manager);
guint64 lime_multimedia_manager_get_current_position(LiMeMultimediaManager *manager);
guint64 lime_multimedia_manager_get_duration(LiMeMultimediaManager *manager);
gdouble lime_multimedia_manager_get_playback_speed(LiMeMultimediaManager *manager);

/* Volume and device management */
gdouble lime_multimedia_manager_get_volume(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_set_volume(LiMeMultimediaManager *manager,
                                            gdouble volume);
gboolean lime_multimedia_manager_mute(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_unmute(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_is_muted(LiMeMultimediaManager *manager);

/* Audio device management */
GList * lime_multimedia_manager_get_audio_devices(LiMeMultimediaManager *manager);
GList * lime_multimedia_manager_get_input_devices(LiMeMultimediaManager *manager);
GList * lime_multimedia_manager_get_output_devices(LiMeMultimediaManager *manager);
LiMeAudioDevice * lime_multimedia_manager_get_default_output_device(LiMeMultimediaManager *manager);
LiMeAudioDevice * lime_multimedia_manager_get_default_input_device(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_set_default_device(LiMeMultimediaManager *manager,
                                                     const gchar *device_id);

/* Playlist management */
LiMePlaylist * lime_multimedia_manager_create_playlist(LiMeMultimediaManager *manager,
                                                        const gchar *playlist_name);
gboolean lime_multimedia_manager_add_track_to_playlist(LiMeMultimediaManager *manager,
                                                       LiMePlaylist *playlist,
                                                       LiMeMediaTrack *track);
gboolean lime_multimedia_manager_remove_track_from_playlist(LiMeMultimediaManager *manager,
                                                            LiMePlaylist *playlist,
                                                            LiMeMediaTrack *track);
GList * lime_multimedia_manager_get_playlists(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_delete_playlist(LiMeMultimediaManager *manager,
                                                 LiMePlaylist *playlist);

/* Library and metadata */
GList * lime_multimedia_manager_scan_music_library(LiMeMultimediaManager *manager,
                                                    const gchar *directory);
GList * lime_multimedia_manager_get_all_tracks(LiMeMultimediaManager *manager);
GList * lime_multimedia_manager_search_tracks(LiMeMultimediaManager *manager,
                                              const gchar *search_term);
LiMeMediaTrack * lime_multimedia_manager_get_track_info(LiMeMultimediaManager *manager,
                                                        const gchar *file_path);
gboolean lime_multimedia_manager_update_metadata(LiMeMultimediaManager *manager,
                                                  LiMeMediaTrack *track);

/* Repeat and shuffle */
gboolean lime_multimedia_manager_set_repeat_mode(LiMeMultimediaManager *manager,
                                                  LiMeRepeatMode mode);
LiMeRepeatMode lime_multimedia_manager_get_repeat_mode(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_enable_shuffle(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_disable_shuffle(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_is_shuffle_enabled(LiMeMultimediaManager *manager);

/* Equalizer */
gboolean lime_multimedia_manager_set_equalizer_preset(LiMeMultimediaManager *manager,
                                                       const gchar *preset_name);
GList * lime_multimedia_manager_get_equalizer_presets(LiMeMultimediaManager *manager);

/* Video playback */
gboolean lime_multimedia_manager_play_video(LiMeMultimediaManager *manager,
                                            const gchar *file_path);
gboolean lime_multimedia_manager_set_video_scale(LiMeMultimediaManager *manager,
                                                  gdouble scale);
gboolean lime_multimedia_manager_set_video_aspect_ratio(LiMeMultimediaManager *manager,
                                                        const gchar *ratio);

/* Configuration */
gboolean lime_multimedia_manager_save_config(LiMeMultimediaManager *manager);
gboolean lime_multimedia_manager_load_config(LiMeMultimediaManager *manager);

G_END_DECLS

#endif /* __LIME_MULTIMEDIA_MANAGER_H__ */
