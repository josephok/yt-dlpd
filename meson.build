project('yt-dlpd', 'c',
  version : '0.1.0',
  license : 'GPL-3.0-or-later',
  default_options : ['c_std=gnu17', 'warning_level=3'])

add_project_arguments(
  '-DPROGRAM_NAME="@0@"'.format(meson.project_name()),
  '-DPROGRAM_VERSION="@0@"'.format(meson.project_version()),
  language : 'c')

executable('yt-dlpd',
  'src/main.c',
  install : true)
