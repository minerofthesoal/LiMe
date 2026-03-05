i18n = import('i18n')
gnome = import('gnome')
pkg = import('pkgconfig')

# Core dependencies
glib_dep = dependency('glib-2.0', version : '>=2.66.0')
gio_dep = dependency('gio-2.0', version : '>=2.66.0')
gobject_dep = dependency('gobject-2.0', version : '>=2.66.0')
gtk_dep = dependency('gtk+-3.0', version : '>=3.24.0', required : false)
clutter_dep = dependency('clutter-1.0', version : '>=1.26.0', required : false)
clutter_gtk_dep = dependency('clutter-gtk-1.0', required : false)
muffin_dep = dependency('libmuffin', version : '>=5.2.0', required : false)
cinnamon_dep = dependency('cinnamon-desktop', version : '>=5.2.0', required : false)

# Multimedia dependencies
gstreamer_dep = dependency('gstreamer-1.0', required : false)
gstreamer_audio_dep = dependency('gstreamer-audio-1.0', required : false)
pulseaudio_dep = dependency('libpulse', required : false)

# Networking/hardware dependencies
upower_dep = dependency('upower-glib', required : false)
networkmanager_dep = dependency('libnm', required : false)
udisks_dep = dependency('udisks2', required : false)
elogind_dep = dependency('libelogind', required : false)

# Build with available dependencies (graceful degradation)
deps = [
  glib_dep,
  gio_dep,
  gobject_dep,
]

# Add optional dependencies if available
if gtk_dep.found()
  deps += gtk_dep
endif
if clutter_dep.found()
  deps += clutter_dep
endif
if muffin_dep.found()
  deps += muffin_dep
endif
if gstreamer_dep.found()
  deps += [gstreamer_dep, gstreamer_audio_dep]
endif

# Core source files
core_sources = [
  'main.c',
  'cinnamon-global.c',
  'shell-util.c',
  'lime-core.c',
  'lime-theme-manager.c',
  'lime-utils.c',
  'lime-settings.c',
  'lime-window-manager.c',
  'lime-workspace.c',
  'lime-window.c',
  'lime-panel.c',
  'lime-launcher.c',
  'lime-compositor.c',
  'lime-notifications.c',
  'lime-screensaver.c',
]

# Manager implementations
manager_sources = [
  'lime-system-monitor.c',
  'lime-network-manager.c',
  'lime-display-settings.c',
  'lime-input-settings.c',
  'lime-power-manager.c',
  'lime-effects-engine.c',
  'lime-file-manager.c',
  'lime-app-store.c',
  'lime-multimedia-manager.c',
  'lime-bluetooth-manager.c',
  'lime-software-updater.c',
  'lime-session-manager.c',
  'lime-ai-integration.c',
  'lime-ai-integration-full.c',
]

# Combine all sources
all_sources = core_sources + manager_sources

# Headers to install
headers = [
  'cinnamon-global.h',
  'shell-util.h',
  'lime-core.h',
  'lime-theme-manager.h',
  'lime-settings.h',
  'lime-system-monitor.h',
  'lime-network-manager.h',
  'lime-display-settings.h',
  'lime-file-manager.h',
  'lime-app-store.h',
  'lime-multimedia-manager.h',
  'lime-bluetooth-manager.h',
  'lime-software-updater.h',
  'lime-session-manager.h',
]

# Main library
lime_lib = library('lime',
  all_sources,
  dependencies : deps,
  install : true,
  soversion : '6',
  version : '6.0.0'
)

# Create pkg-config file
pkg.generate(
  lime_lib,
  name : 'lime-cinnamon',
  description : 'LiMe Desktop Environment Library',
  version : '6.0.0',
)

# Install public headers
install_headers(headers,
  subdir : 'lime-de'
)

# Translations
i18n.gettext(meson.project_name(),
  preset : 'glib'
)
