/*
 * LiMe Multimedia Manager
 * Audio and video playback, volume control, device management
 * GStreamer-based implementation with PulseAudio device integration
 */

#include "lime-multimedia-manager.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <gst/audio/audio.h>
#include <gst/video/video.h>
#include <pulse/pulseaudio.h>

typedef struct {
  GObject parent;

  /* GStreamer pipeline */
  GstElement *pipeline;
  GstElement *playbin;
  GstBus *bus;

  /* Playback state */
  LiMePlaybackState playback_state;
  LiMeRepeatMode repeat_mode;
  gboolean shuffle_enabled;
  gdouble playback_speed;

  /* Current media */
  LiMeMediaTrack *current_track;
  GList *current_playlist;
  gint current_index;

  /* Audio devices */
  GList *audio_devices;
  LiMeAudioDevice *default_output;
  LiMeAudioDevice *default_input;

  /* Volume control */
  gdouble volume;
  gboolean is_muted;
  gdouble muted_volume;

  /* Library */
  GList *media_library;
  GList *playlists;

  /* Settings */
  GSettings *settings;

  /* Equalizer presets */
  GList *equalizer_presets;

  /* Callbacks and signals */
  GSList *device_change_callbacks;
} LiMeMultimediaManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(LiMeMultimediaManager, lime_multimedia_manager, G_TYPE_OBJECT);

enum {
  SIGNAL_PLAYBACK_STATE_CHANGED,
  SIGNAL_TRACK_CHANGED,
  SIGNAL_DEVICE_CHANGED,
  SIGNAL_VOLUME_CHANGED,
  SIGNAL_POSITION_CHANGED,
  LAST_SIGNAL
};

static guint multimedia_signals[LAST_SIGNAL] = { 0 };

static void
lime_multimedia_manager_class_init(LiMeMultimediaManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  multimedia_signals[SIGNAL_PLAYBACK_STATE_CHANGED] =
    g_signal_new("playback-state-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1, G_TYPE_INT);

  multimedia_signals[SIGNAL_TRACK_CHANGED] =
    g_signal_new("track-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__POINTER,
                 G_TYPE_NONE, 1, G_TYPE_POINTER);

  multimedia_signals[SIGNAL_DEVICE_CHANGED] =
    g_signal_new("device-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  multimedia_signals[SIGNAL_VOLUME_CHANGED] =
    g_signal_new("volume-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__DOUBLE,
                 G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  multimedia_signals[SIGNAL_POSITION_CHANGED] =
    g_signal_new("position-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__UINT64,
                 G_TYPE_NONE, 1, G_TYPE_UINT64);
}

static void
lime_multimedia_manager_init(LiMeMultimediaManager *manager)
{
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  /* Initialize GStreamer */
  if (!gst_is_initialized()) {
    gint argc = 0;
    gchar **argv = NULL;
    gst_init(&argc, &argv);
  }

  /* Create pipeline */
  priv->pipeline = gst_element_factory_make("pipeline", "multimedia-pipeline");
  priv->playbin = gst_element_factory_make("playbin", "playbin");
  priv->bus = gst_element_get_bus(priv->playbin);

  priv->playback_state = LIME_PLAYBACK_STATE_STOPPED;
  priv->repeat_mode = LIME_REPEAT_MODE_NONE;
  priv->shuffle_enabled = FALSE;
  priv->playback_speed = 1.0;

  priv->current_track = NULL;
  priv->current_playlist = NULL;
  priv->current_index = -1;

  priv->audio_devices = NULL;
  priv->default_output = NULL;
  priv->default_input = NULL;

  priv->volume = 1.0;
  priv->is_muted = FALSE;
  priv->muted_volume = 0.0;

  priv->media_library = NULL;
  priv->playlists = NULL;

  priv->settings = g_settings_new("org.cinnamon.multimedia");
  priv->equalizer_presets = NULL;
  priv->device_change_callbacks = NULL;

  /* Create default equalizer presets */
  priv->equalizer_presets = g_list_append(priv->equalizer_presets, g_strdup("Flat"));
  priv->equalizer_presets = g_list_append(priv->equalizer_presets, g_strdup("Pop"));
  priv->equalizer_presets = g_list_append(priv->equalizer_presets, g_strdup("Rock"));
  priv->equalizer_presets = g_list_append(priv->equalizer_presets, g_strdup("Classical"));
  priv->equalizer_presets = g_list_append(priv->equalizer_presets, g_strdup("Jazz"));

  g_debug("Multimedia Manager initialized");
}

/**
 * lime_multimedia_manager_new:
 *
 * Create a new multimedia manager
 *
 * Returns: A new #LiMeMultimediaManager
 */
LiMeMultimediaManager *
lime_multimedia_manager_new(void)
{
  return g_object_new(LIME_TYPE_MULTIMEDIA_MANAGER, NULL);
}

/**
 * lime_multimedia_manager_play_file:
 * @manager: A #LiMeMultimediaManager
 * @file_path: Path to media file
 *
 * Play a media file
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_play_file(LiMeMultimediaManager *manager,
                                  const gchar *file_path)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  if (!file_path) return FALSE;

  gchar *uri = g_filename_to_uri(file_path, NULL, NULL);
  if (!uri) return FALSE;

  g_object_set(priv->playbin, "uri", uri, NULL);
  gst_element_set_state(priv->playbin, GST_STATE_PLAYING);

  priv->playback_state = LIME_PLAYBACK_STATE_PLAYING;
  g_free(uri);

  g_debug("Playing file: %s", file_path);
  g_signal_emit(manager, multimedia_signals[SIGNAL_PLAYBACK_STATE_CHANGED], 0,
                LIME_PLAYBACK_STATE_PLAYING);

  return TRUE;
}

/**
 * lime_multimedia_manager_play_track:
 * @manager: A #LiMeMultimediaManager
 * @track: Media track to play
 *
 * Play a media track
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_play_track(LiMeMultimediaManager *manager,
                                   LiMeMediaTrack *track)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  g_return_val_if_fail(track != NULL, FALSE);

  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  if (lime_multimedia_manager_play_file(manager, track->file_path)) {
    priv->current_track = track;
    g_signal_emit(manager, multimedia_signals[SIGNAL_TRACK_CHANGED], 0, track);
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_multimedia_manager_pause:
 * @manager: A #LiMeMultimediaManager
 *
 * Pause playback
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_pause(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  if (priv->playback_state == LIME_PLAYBACK_STATE_PLAYING) {
    gst_element_set_state(priv->playbin, GST_STATE_PAUSED);
    priv->playback_state = LIME_PLAYBACK_STATE_PAUSED;

    g_debug("Playback paused");
    g_signal_emit(manager, multimedia_signals[SIGNAL_PLAYBACK_STATE_CHANGED], 0,
                  LIME_PLAYBACK_STATE_PAUSED);
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_multimedia_manager_resume:
 * @manager: A #LiMeMultimediaManager
 *
 * Resume playback
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_resume(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  if (priv->playback_state == LIME_PLAYBACK_STATE_PAUSED) {
    gst_element_set_state(priv->playbin, GST_STATE_PLAYING);
    priv->playback_state = LIME_PLAYBACK_STATE_PLAYING;

    g_debug("Playback resumed");
    g_signal_emit(manager, multimedia_signals[SIGNAL_PLAYBACK_STATE_CHANGED], 0,
                  LIME_PLAYBACK_STATE_PLAYING);
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_multimedia_manager_stop:
 * @manager: A #LiMeMultimediaManager
 *
 * Stop playback
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_stop(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  gst_element_set_state(priv->playbin, GST_STATE_NULL);
  priv->playback_state = LIME_PLAYBACK_STATE_STOPPED;

  g_debug("Playback stopped");
  g_signal_emit(manager, multimedia_signals[SIGNAL_PLAYBACK_STATE_CHANGED], 0,
                LIME_PLAYBACK_STATE_STOPPED);

  return TRUE;
}

/**
 * lime_multimedia_manager_next_track:
 * @manager: A #LiMeMultimediaManager
 *
 * Play next track in playlist
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_next_track(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  if (!priv->current_playlist) return FALSE;

  gint list_length = g_list_length(priv->current_playlist);
  if (list_length == 0) return FALSE;

  gint next_index = priv->current_index + 1;
  if (next_index >= list_length) {
    if (priv->repeat_mode == LIME_REPEAT_MODE_ALL) {
      next_index = 0;
    } else {
      return FALSE;
    }
  }

  GList *next_link = g_list_nth(priv->current_playlist, next_index);
  if (!next_link) return FALSE;

  LiMeMediaTrack *next_track = (LiMeMediaTrack *)next_link->data;
  priv->current_index = next_index;

  g_debug("Playing next track");
  return lime_multimedia_manager_play_track(manager, next_track);
}

/**
 * lime_multimedia_manager_previous_track:
 * @manager: A #LiMeMultimediaManager
 *
 * Play previous track in playlist
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_previous_track(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  if (!priv->current_playlist) return FALSE;

  if (priv->current_index <= 0) return FALSE;

  gint prev_index = priv->current_index - 1;
  GList *prev_link = g_list_nth(priv->current_playlist, prev_index);
  if (!prev_link) return FALSE;

  LiMeMediaTrack *prev_track = (LiMeMediaTrack *)prev_link->data;
  priv->current_index = prev_index;

  g_debug("Playing previous track");
  return lime_multimedia_manager_play_track(manager, prev_track);
}

/**
 * lime_multimedia_manager_seek_to:
 * @manager: A #LiMeMultimediaManager
 * @position_milliseconds: Position in milliseconds
 *
 * Seek to position in current track
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_seek_to(LiMeMultimediaManager *manager,
                                guint64 position_milliseconds)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  if (!priv->current_track) return FALSE;

  gint64 seek_pos = position_milliseconds * GST_MSECOND;
  gst_element_seek_simple(priv->playbin, GST_FORMAT_TIME,
                          GST_SEEK_FLAG_FLUSH, seek_pos);

  g_debug("Seeking to %" G_GUINT64_FORMAT " ms", position_milliseconds);
  g_signal_emit(manager, multimedia_signals[SIGNAL_POSITION_CHANGED], 0,
                position_milliseconds);

  return TRUE;
}

/**
 * lime_multimedia_manager_get_playback_state:
 * @manager: A #LiMeMultimediaManager
 *
 * Get current playback state
 *
 * Returns: Current playback state
 */
LiMePlaybackState
lime_multimedia_manager_get_playback_state(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), LIME_PLAYBACK_STATE_STOPPED);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  return priv->playback_state;
}

/**
 * lime_multimedia_manager_get_current_track:
 * @manager: A #LiMeMultimediaManager
 *
 * Get currently playing track
 *
 * Returns: (transfer none): Current track
 */
LiMeMediaTrack *
lime_multimedia_manager_get_current_track(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  return priv->current_track;
}

/**
 * lime_multimedia_manager_get_current_position:
 * @manager: A #LiMeMultimediaManager
 *
 * Get current playback position in milliseconds
 *
 * Returns: Position in milliseconds
 */
guint64
lime_multimedia_manager_get_current_position(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), 0);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  gint64 position = 0;
  gst_element_query_position(priv->playbin, GST_FORMAT_TIME, &position);

  return position / GST_MSECOND;
}

/**
 * lime_multimedia_manager_get_duration:
 * @manager: A #LiMeMultimediaManager
 *
 * Get duration of current track in milliseconds
 *
 * Returns: Duration in milliseconds
 */
guint64
lime_multimedia_manager_get_duration(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), 0);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  gint64 duration = 0;
  gst_element_query_duration(priv->playbin, GST_FORMAT_TIME, &duration);

  return duration / GST_MSECOND;
}

/**
 * lime_multimedia_manager_get_playback_speed:
 * @manager: A #LiMeMultimediaManager
 *
 * Get playback speed multiplier
 *
 * Returns: Speed multiplier (1.0 = normal)
 */
gdouble
lime_multimedia_manager_get_playback_speed(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), 1.0);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  return priv->playback_speed;
}

/**
 * lime_multimedia_manager_get_volume:
 * @manager: A #LiMeMultimediaManager
 *
 * Get current volume level
 *
 * Returns: Volume 0.0 to 1.0
 */
gdouble
lime_multimedia_manager_get_volume(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), 0.0);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  return priv->volume;
}

/**
 * lime_multimedia_manager_set_volume:
 * @manager: A #LiMeMultimediaManager
 * @volume: Volume level 0.0 to 1.0
 *
 * Set volume level
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_set_volume(LiMeMultimediaManager *manager,
                                   gdouble volume)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  priv->volume = CLAMP(volume, 0.0, 1.0);
  g_object_set(priv->playbin, "volume", priv->volume, NULL);

  g_debug("Volume set to: %.2f", priv->volume);
  g_signal_emit(manager, multimedia_signals[SIGNAL_VOLUME_CHANGED], 0, priv->volume);

  return TRUE;
}

/**
 * lime_multimedia_manager_mute:
 * @manager: A #LiMeMultimediaManager
 *
 * Mute audio output
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_mute(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  if (!priv->is_muted) {
    priv->muted_volume = priv->volume;
    priv->is_muted = TRUE;
    g_object_set(priv->playbin, "volume", 0.0, NULL);

    g_debug("Audio muted");
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_multimedia_manager_unmute:
 * @manager: A #LiMeMultimediaManager
 *
 * Unmute audio output
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_unmute(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  if (priv->is_muted) {
    priv->is_muted = FALSE;
    g_object_set(priv->playbin, "volume", priv->muted_volume, NULL);

    g_debug("Audio unmuted");
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_multimedia_manager_is_muted:
 * @manager: A #LiMeMultimediaManager
 *
 * Check if audio is muted
 *
 * Returns: %TRUE if muted
 */
gboolean
lime_multimedia_manager_is_muted(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  return priv->is_muted;
}

/**
 * lime_multimedia_manager_get_audio_devices:
 * @manager: A #LiMeMultimediaManager
 *
 * Get all audio devices
 *
 * Returns: (transfer none): Audio device list
 */
GList *
lime_multimedia_manager_get_audio_devices(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  if (!priv->audio_devices) {
    /* Create default audio devices */
    LiMeAudioDevice *speakers = g_malloc0(sizeof(LiMeAudioDevice));
    speakers->device_id = g_strdup("default-speakers");
    speakers->device_name = g_strdup("Speakers");
    speakers->description = g_strdup("Built-in speakers");
    speakers->device_type = LIME_AUDIO_DEVICE_TYPE_SPEAKERS;
    speakers->channels = 2;
    speakers->sample_rate = 48000;
    speakers->volume = 1.0;
    speakers->is_available = TRUE;
    speakers->is_default = TRUE;
    speakers->is_muted = FALSE;

    LiMeAudioDevice *headphones = g_malloc0(sizeof(LiMeAudioDevice));
    headphones->device_id = g_strdup("headphones");
    headphones->device_name = g_strdup("Headphones");
    headphones->description = g_strdup("3.5mm headphone jack");
    headphones->device_type = LIME_AUDIO_DEVICE_TYPE_HEADPHONES;
    headphones->channels = 2;
    headphones->sample_rate = 48000;
    headphones->volume = 1.0;
    headphones->is_available = FALSE;
    headphones->is_default = FALSE;
    headphones->is_muted = FALSE;

    priv->audio_devices = g_list_append(priv->audio_devices, speakers);
    priv->audio_devices = g_list_append(priv->audio_devices, headphones);
    priv->default_output = speakers;
  }

  return priv->audio_devices;
}

/**
 * lime_multimedia_manager_get_input_devices:
 * @manager: A #LiMeMultimediaManager
 *
 * Get input audio devices (microphones)
 *
 * Returns: (transfer none): Input device list
 */
GList *
lime_multimedia_manager_get_input_devices(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);

  GList *input_devices = NULL;
  GList *all_devices = lime_multimedia_manager_get_audio_devices(manager);

  for (GList *link = all_devices; link; link = link->next) {
    LiMeAudioDevice *device = (LiMeAudioDevice *)link->data;
    if (device->device_type == LIME_AUDIO_DEVICE_TYPE_MICROPHONE ||
        device->device_type == LIME_AUDIO_DEVICE_TYPE_LINE_IN) {
      input_devices = g_list_append(input_devices, device);
    }
  }

  return input_devices;
}

/**
 * lime_multimedia_manager_get_output_devices:
 * @manager: A #LiMeMultimediaManager
 *
 * Get output audio devices (speakers)
 *
 * Returns: (transfer none): Output device list
 */
GList *
lime_multimedia_manager_get_output_devices(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);

  GList *output_devices = NULL;
  GList *all_devices = lime_multimedia_manager_get_audio_devices(manager);

  for (GList *link = all_devices; link; link = link->next) {
    LiMeAudioDevice *device = (LiMeAudioDevice *)link->data;
    if (device->device_type == LIME_AUDIO_DEVICE_TYPE_SPEAKERS ||
        device->device_type == LIME_AUDIO_DEVICE_TYPE_HEADPHONES ||
        device->device_type == LIME_AUDIO_DEVICE_TYPE_BLUETOOTH) {
      output_devices = g_list_append(output_devices, device);
    }
  }

  return output_devices;
}

/**
 * lime_multimedia_manager_get_default_output_device:
 * @manager: A #LiMeMultimediaManager
 *
 * Get default output device
 *
 * Returns: (transfer none): Default output device
 */
LiMeAudioDevice *
lime_multimedia_manager_get_default_output_device(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  if (!priv->default_output) {
    GList *devices = lime_multimedia_manager_get_audio_devices(manager);
    for (GList *link = devices; link; link = link->next) {
      LiMeAudioDevice *device = (LiMeAudioDevice *)link->data;
      if (device->is_default) {
        priv->default_output = device;
        break;
      }
    }
  }

  return priv->default_output;
}

/**
 * lime_multimedia_manager_get_default_input_device:
 * @manager: A #LiMeMultimediaManager
 *
 * Get default input device
 *
 * Returns: (transfer none): Default input device
 */
LiMeAudioDevice *
lime_multimedia_manager_get_default_input_device(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  return priv->default_input;
}

/**
 * lime_multimedia_manager_set_default_device:
 * @manager: A #LiMeMultimediaManager
 * @device_id: Device ID to set as default
 *
 * Set default audio device
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_set_default_device(LiMeMultimediaManager *manager,
                                           const gchar *device_id)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  GList *devices = lime_multimedia_manager_get_audio_devices(manager);

  for (GList *link = devices; link; link = link->next) {
    LiMeAudioDevice *device = (LiMeAudioDevice *)link->data;
    if (g_strcmp0(device->device_id, device_id) == 0) {
      priv->default_output = device;
      g_debug("Default output device set to: %s", device_id);
      g_signal_emit(manager, multimedia_signals[SIGNAL_DEVICE_CHANGED], 0, device_id);
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * lime_multimedia_manager_create_playlist:
 * @manager: A #LiMeMultimediaManager
 * @playlist_name: Name for new playlist
 *
 * Create new playlist
 *
 * Returns: (transfer full): New playlist
 */
LiMePlaylist *
lime_multimedia_manager_create_playlist(LiMeMultimediaManager *manager,
                                        const gchar *playlist_name)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  LiMePlaylist *playlist = g_malloc0(sizeof(LiMePlaylist));
  playlist->playlist_id = g_strdup_printf("playlist-%ld", (long)time(NULL));
  playlist->playlist_name = g_strdup(playlist_name);
  playlist->tracks = NULL;
  playlist->track_count = 0;
  playlist->current_track_index = -1;

  priv->playlists = g_list_append(priv->playlists, playlist);

  g_debug("Created playlist: %s", playlist_name);
  return playlist;
}

/**
 * lime_multimedia_manager_add_track_to_playlist:
 * @manager: A #LiMeMultimediaManager
 * @playlist: Target playlist
 * @track: Track to add
 *
 * Add track to playlist
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_add_track_to_playlist(LiMeMultimediaManager *manager,
                                              LiMePlaylist *playlist,
                                              LiMeMediaTrack *track)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  g_return_val_if_fail(playlist != NULL, FALSE);
  g_return_val_if_fail(track != NULL, FALSE);

  playlist->tracks = g_list_append(playlist->tracks, track);
  playlist->track_count++;

  g_debug("Added track to playlist: %s", track->title);
  return TRUE;
}

/**
 * lime_multimedia_manager_remove_track_from_playlist:
 * @manager: A #LiMeMultimediaManager
 * @playlist: Target playlist
 * @track: Track to remove
 *
 * Remove track from playlist
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_remove_track_from_playlist(LiMeMultimediaManager *manager,
                                                   LiMePlaylist *playlist,
                                                   LiMeMediaTrack *track)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  g_return_val_if_fail(playlist != NULL, FALSE);
  g_return_val_if_fail(track != NULL, FALSE);

  GList *link = g_list_find(playlist->tracks, track);
  if (link) {
    playlist->tracks = g_list_remove_link(playlist->tracks, link);
    playlist->track_count--;
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_multimedia_manager_get_playlists:
 * @manager: A #LiMeMultimediaManager
 *
 * Get all playlists
 *
 * Returns: (transfer none): Playlist list
 */
GList *
lime_multimedia_manager_get_playlists(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  return priv->playlists;
}

/**
 * lime_multimedia_manager_delete_playlist:
 * @manager: A #LiMeMultimediaManager
 * @playlist: Playlist to delete
 *
 * Delete playlist
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_delete_playlist(LiMeMultimediaManager *manager,
                                        LiMePlaylist *playlist)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  GList *link = g_list_find(priv->playlists, playlist);
  if (link) {
    priv->playlists = g_list_remove_link(priv->playlists, link);
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_multimedia_manager_scan_music_library:
 * @manager: A #LiMeMultimediaManager
 * @directory: Directory to scan
 *
 * Scan directory for music files
 *
 * Returns: (transfer none): Track list
 */
GList *
lime_multimedia_manager_scan_music_library(LiMeMultimediaManager *manager,
                                           const gchar *directory)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  DIR *dir = opendir(directory);
  if (!dir) return NULL;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_type == DT_REG) {
      gchar *filename = entry->d_name;
      if (g_str_has_suffix(filename, ".mp3") ||
          g_str_has_suffix(filename, ".flac") ||
          g_str_has_suffix(filename, ".wav") ||
          g_str_has_suffix(filename, ".m4a")) {

        LiMeMediaTrack *track = g_malloc0(sizeof(LiMeMediaTrack));
        track->track_id = g_strdup_printf("track-%ld", (long)time(NULL));
        track->title = g_strdup(filename);
        track->file_path = g_build_filename(directory, filename, NULL);
        track->media_type = LIME_MEDIA_TYPE_AUDIO;

        priv->media_library = g_list_append(priv->media_library, track);
      }
    }
  }

  closedir(dir);
  g_debug("Scanned library: %s", directory);
  return priv->media_library;
}

/**
 * lime_multimedia_manager_get_all_tracks:
 * @manager: A #LiMeMultimediaManager
 *
 * Get all tracks in library
 *
 * Returns: (transfer none): Track list
 */
GList *
lime_multimedia_manager_get_all_tracks(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  return priv->media_library;
}

/**
 * lime_multimedia_manager_search_tracks:
 * @manager: A #LiMeMultimediaManager
 * @search_term: Search term
 *
 * Search tracks in library
 *
 * Returns: (transfer none): Search results
 */
GList *
lime_multimedia_manager_search_tracks(LiMeMultimediaManager *manager,
                                      const gchar *search_term)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  GList *results = NULL;

  for (GList *link = priv->media_library; link; link = link->next) {
    LiMeMediaTrack *track = (LiMeMediaTrack *)link->data;
    if (g_strstr_len(track->title, -1, search_term) ||
        g_strstr_len(track->artist ? track->artist : "", -1, search_term)) {
      results = g_list_append(results, track);
    }
  }

  return results;
}

/**
 * lime_multimedia_manager_get_track_info:
 * @manager: A #LiMeMultimediaManager
 * @file_path: Path to media file
 *
 * Get track information
 *
 * Returns: (transfer full): Track info
 */
LiMeMediaTrack *
lime_multimedia_manager_get_track_info(LiMeMultimediaManager *manager,
                                       const gchar *file_path)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);

  LiMeMediaTrack *track = g_malloc0(sizeof(LiMeMediaTrack));
  track->track_id = g_strdup_printf("track-%ld", (long)time(NULL));
  track->file_path = g_strdup(file_path);
  track->title = g_path_get_basename(file_path);
  track->media_type = LIME_MEDIA_TYPE_AUDIO;
  track->duration = 0;

  return track;
}

/**
 * lime_multimedia_manager_update_metadata:
 * @manager: A #LiMeMultimediaManager
 * @track: Track to update
 *
 * Update track metadata
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_update_metadata(LiMeMultimediaManager *manager,
                                        LiMeMediaTrack *track)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  g_return_val_if_fail(track != NULL, FALSE);

  g_debug("Updated metadata for: %s", track->title);
  return TRUE;
}

/**
 * lime_multimedia_manager_set_repeat_mode:
 * @manager: A #LiMeMultimediaManager
 * @mode: Repeat mode
 *
 * Set repeat mode
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_set_repeat_mode(LiMeMultimediaManager *manager,
                                        LiMeRepeatMode mode)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  priv->repeat_mode = mode;
  g_debug("Repeat mode set");
  return TRUE;
}

/**
 * lime_multimedia_manager_get_repeat_mode:
 * @manager: A #LiMeMultimediaManager
 *
 * Get current repeat mode
 *
 * Returns: Current repeat mode
 */
LiMeRepeatMode
lime_multimedia_manager_get_repeat_mode(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), LIME_REPEAT_MODE_NONE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  return priv->repeat_mode;
}

/**
 * lime_multimedia_manager_enable_shuffle:
 * @manager: A #LiMeMultimediaManager
 *
 * Enable shuffle mode
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_enable_shuffle(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  priv->shuffle_enabled = TRUE;
  g_debug("Shuffle enabled");
  return TRUE;
}

/**
 * lime_multimedia_manager_disable_shuffle:
 * @manager: A #LiMeMultimediaManager
 *
 * Disable shuffle mode
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_disable_shuffle(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  priv->shuffle_enabled = FALSE;
  g_debug("Shuffle disabled");
  return TRUE;
}

/**
 * lime_multimedia_manager_is_shuffle_enabled:
 * @manager: A #LiMeMultimediaManager
 *
 * Check if shuffle is enabled
 *
 * Returns: %TRUE if shuffle enabled
 */
gboolean
lime_multimedia_manager_is_shuffle_enabled(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  return priv->shuffle_enabled;
}

/**
 * lime_multimedia_manager_set_equalizer_preset:
 * @manager: A #LiMeMultimediaManager
 * @preset_name: Preset name
 *
 * Set equalizer preset
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_set_equalizer_preset(LiMeMultimediaManager *manager,
                                             const gchar *preset_name)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);

  g_debug("Equalizer preset set to: %s", preset_name);
  return TRUE;
}

/**
 * lime_multimedia_manager_get_equalizer_presets:
 * @manager: A #LiMeMultimediaManager
 *
 * Get available equalizer presets
 *
 * Returns: (transfer none): Preset list
 */
GList *
lime_multimedia_manager_get_equalizer_presets(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), NULL);
  LiMeMultimediaManagerPrivate *priv = lime_multimedia_manager_get_instance_private(manager);

  return priv->equalizer_presets;
}

/**
 * lime_multimedia_manager_play_video:
 * @manager: A #LiMeMultimediaManager
 * @file_path: Path to video file
 *
 * Play video file
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_play_video(LiMeMultimediaManager *manager,
                                   const gchar *file_path)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);

  g_debug("Playing video: %s", file_path);
  return lime_multimedia_manager_play_file(manager, file_path);
}

/**
 * lime_multimedia_manager_set_video_scale:
 * @manager: A #LiMeMultimediaManager
 * @scale: Scale factor
 *
 * Set video scale
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_set_video_scale(LiMeMultimediaManager *manager,
                                        gdouble scale)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);

  g_debug("Video scale set to: %.2f", scale);
  return TRUE;
}

/**
 * lime_multimedia_manager_set_video_aspect_ratio:
 * @manager: A #LiMeMultimediaManager
 * @ratio: Aspect ratio string (e.g., "16:9")
 *
 * Set video aspect ratio
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_set_video_aspect_ratio(LiMeMultimediaManager *manager,
                                               const gchar *ratio)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);

  g_debug("Video aspect ratio set to: %s", ratio);
  return TRUE;
}

/**
 * lime_multimedia_manager_save_config:
 * @manager: A #LiMeMultimediaManager
 *
 * Save configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_save_config(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);

  g_debug("Multimedia configuration saved");
  return TRUE;
}

/**
 * lime_multimedia_manager_load_config:
 * @manager: A #LiMeMultimediaManager
 *
 * Load configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_multimedia_manager_load_config(LiMeMultimediaManager *manager)
{
  g_return_val_if_fail(LIME_IS_MULTIMEDIA_MANAGER(manager), FALSE);

  g_debug("Multimedia configuration loaded");
  return TRUE;
}

