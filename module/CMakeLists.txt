add_subdirectory(drivers)

if(CONFIG_CHARYBDIS_LED_RENDERER)
    target_sources(app PRIVATE
        layer_color.c
    )
else()
    target_sources(app PRIVATE
        layer_color_stub.c
    )
endif()

target_sources(app PRIVATE
    layer_color_sync.c
)
