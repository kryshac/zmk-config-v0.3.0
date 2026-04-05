add_subdirectory(drivers)

target_sources_ifdef(CONFIG_ZMK_RGB_UNDERGLOW app PRIVATE
    layer_color.c
)
