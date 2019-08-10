cmake_minimum_required(VERSION 3.10.1)

include(ExternalProject)


# Simple-Web-Server
ExternalProject_Add(Simple-Web-Server-project
  PREFIX "${PROJECT_SOURCE_DIR}/external/Simple-Web-Server"
  GIT_REPOSITORY "https://gitlab.com/eidheim/Simple-Web-Server"
  GIT_PROGRESS 1
  GIT_TAG 0f8b26ff19d757db039d6f70e252219326cc3d8f

  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  )
set(SIMPLE-WEB-SERVER_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/external/Simple-Web-Server/src/Simple-Web-Server-project/)
message("Simple-Web-Server: ${SIMPLE-WEB-SERVER_INCLUDE_DIR}")

message("check for Simple-Web-Server target")
if(NOT TARGET WebServer::WebServer)
  message("Simple-Web-Server target not found")
  add_library(WebServer::WebServer UNKNOWN IMPORTED GLOBAL)
  set_target_properties(WebServer::WebServer PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${SIMPLE-WEB-SERVER_INCLUDE_DIR}
    )
endif()
