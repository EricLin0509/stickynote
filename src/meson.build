stickynote_sources = [
  'main.c',
  'stickynote-application.c',
  'stickynote-window.c',
  'stickynote-editor-window.c',
  'theme-selector.c',
  'metadata.c',
]

stickynote_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('gtksourceview-5'),
]

subdir('GtkMarkdownView')

stickynote_sources += gnome.compile_resources('stickynote-resources',
  'stickynote.gresource.xml',
  c_name: 'stickynote'
)

executable('stickynote', stickynote_sources,
  dependencies: stickynote_deps,
  link_with: [gtk_markdown_view],
       install: true,
)
