INCLUDE_DIRECTORIES (
    ${STAR_EXTERN_INCLUDES}
    ${STAR_CORE_INCLUDES}
    ${STAR_PLATFORM_INCLUDES}
    ${STAR_APPLICATION_INCLUDES}
  )

SET (star_application_HEADERS
    StarApplication.hpp
    StarApplicationController.hpp
    StarMainApplication.hpp
    StarRendererFactory.hpp
    StarRenderer.hpp
  )

SET (star_application_SOURCES
    StarApplication.cpp
    StarRendererFactory.cpp
    StarRenderer.cpp
  )

SET (star_application_HEADERS ${star_application_HEADERS}
    StarP2PNetworkingService_pc.hpp
    StarPlatformServices_pc.hpp
  )

SET (star_application_SOURCES ${star_application_SOURCES}
    StarMainApplication_sdl.cpp
    StarP2PNetworkingService_pc.cpp
    StarPlatformServices_pc.cpp
  )

IF (STAR_ENABLE_OPENGL_RENDERER)
  SET (star_application_HEADERS ${star_application_HEADERS}
      StarRenderer_opengl.hpp
    )
  SET (star_application_SOURCES ${star_application_SOURCES}
      StarRenderer_opengl.cpp
    )
ENDIF ()

IF (STAR_ENABLE_VULKAN_RENDERER)
  SET (star_application_HEADERS ${star_application_HEADERS}
      StarRenderer_vulkan.hpp
    )
  SET (star_application_SOURCES ${star_application_SOURCES}
      StarRenderer_vulkan.cpp
    )
ENDIF ()

IF (STAR_ENABLE_STEAM_INTEGRATION)
  SET (star_application_HEADERS ${star_application_HEADERS}
      StarDesktopService_pc_steam.hpp
      StarStatisticsService_pc_steam.hpp
      StarUserGeneratedContentService_pc_steam.hpp
    )
  SET (star_application_SOURCES ${star_application_SOURCES}
      StarDesktopService_pc_steam.cpp
      StarStatisticsService_pc_steam.cpp
      StarUserGeneratedContentService_pc_steam.cpp
    )
ENDIF ()

IF (STAR_ENABLE_DISCORD_INTEGRATION)
  SET (star_application_SOURCES ${star_application_SOURCES}
      discord/core.cpp
      discord/image_manager.cpp
      discord/lobby_manager.cpp
      discord/network_manager.cpp
      discord/overlay_manager.cpp
      discord/relationship_manager.cpp
      discord/storage_manager.cpp
      discord/store_manager.cpp
      discord/types.cpp
      discord/user_manager.cpp
      discord/voice_manager.cpp
      discord/achievement_manager.cpp
      discord/activity_manager.cpp
      discord/application_manager.cpp
    )
ENDIF ()

ADD_LIBRARY (star_application OBJECT ${star_application_SOURCES} ${star_application_HEADERS})

IF(STAR_PRECOMPILED_HEADERS)
  TARGET_PRECOMPILE_HEADERS (star_application REUSE_FROM star_core)
ENDIF()
