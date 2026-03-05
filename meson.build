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
  'src/de/main.c',
  'src/de/shell-util.c',
  'src/de/lime-core.c',
  'src/de/lime-theme-manager.c',
  'src/de/lime-utils.c',
  'src/de/lime-settings.c',
  'src/de/lime-window-manager.c',
  'src/de/lime-workspace.c',
  'src/de/lime-window.c',
  'src/de/lime-panel.c',
  'src/de/lime-launcher.c',
  'src/de/lime-compositor.c',
  'src/de/lime-notifications.c',
  'src/de/lime-screensaver.c',
]

# Manager implementations
manager_sources = [
  'src/de/lime-system-monitor.c',
  'src/de/lime-network-manager.c',
  'src/de/lime-display-settings.c',
  'src/de/lime-input-settings.c',
  'src/de/lime-power-manager.c',
  'src/de/lime-effects-engine.c',
  'src/de/lime-file-manager.c',
  'src/de/lime-app-store.c',
  'src/de/lime-multimedia-manager.c',
  'src/de/lime-bluetooth-manager.c',
  'src/de/lime-software-updater.c',
  'src/de/lime-session-manager.c',
  'src/de/lime-ai-integration.c',
  'src/de/lime-ai-integration-full.c',
]

# Combine all sources
all_sources = core_sources + manager_sources

# Headers to install
headers = [
  'include/lime-de/cinnamon-global.h',
  'include/lime-de/shell-util.h',
  'include/lime-de/lime-core.h',
  'include/lime-de/lime-theme-manager.h',
  'include/lime-de/lime-settings.h',
  'include/lime-de/lime-system-monitor.h',
  'include/lime-de/lime-network-manager.h',
  'include/lime-de/lime-display-settings.h',
  'include/lime-de/lime-file-manager.h',
  'include/lime-de/lime-app-store.h',
  'include/lime-de/lime-multimedia-manager.h',
  'include/lime-de/lime-bluetooth-manager.h',
  'include/lime-de/lime-software-updater.h',
  'include/lime-de/lime-session-manager.h',
]


project_includes = include_directories('include/lime-de', 'src/de')

# Main library
lime_lib = library('lime',
  all_sources,
  dependencies : deps,
  include_directories : project_includes,
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
