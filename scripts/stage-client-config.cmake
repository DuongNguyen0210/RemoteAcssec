cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CLIENT_CONFIG OR NOT EXISTS "${CLIENT_CONFIG}" OR NOT IS_DIRECTORY "${PACKAGE_DIRECTORY}")
    message(FATAL_ERROR "Create clients/RemoteAccessApp/.env from .env.example before packaging.")
endif()

# Reconstruct the file so comments, unsupported keys and credentials cannot
# hitch a ride in an otherwise valid client configuration.
file(STRINGS "${CLIENT_CONFIG}" lines ENCODING UTF-8)
foreach(line IN LISTS lines)
    string(STRIP "${line}" line)
    if(line STREQUAL "" OR line MATCHES "^#")
        continue()
    endif()
    if(NOT line MATCHES "^(REMOTE_API_URL|REMOTE_RELAY_MODE|REMOTE_RELAY_TLS)[ \t]*=[ \t]*(.+)$")
        message(FATAL_ERROR "Client .env may contain only the three public release settings.")
    endif()
    set(key "${CMAKE_MATCH_1}")
    if(DEFINED "${key}")
        message(FATAL_ERROR "Each public release setting must appear exactly once.")
    endif()
    set("${key}" "${CMAKE_MATCH_2}")
endforeach()

if(NOT REMOTE_API_URL MATCHES [=[^https://(\[[0-9A-Fa-f:]+\]|[A-Za-z0-9.-]+)(:[0-9]+)?/?$]=]
   OR NOT REMOTE_RELAY_MODE STREQUAL "dynamic"
   OR NOT REMOTE_RELAY_TLS STREQUAL "required")
    message(FATAL_ERROR "Client .env needs a plain HTTPS origin, dynamic relay mode and required TLS.")
endif()

file(WRITE "${PACKAGE_DIRECTORY}/.env"
    "REMOTE_API_URL=${REMOTE_API_URL}\nREMOTE_RELAY_MODE=dynamic\nREMOTE_RELAY_TLS=required\n")
