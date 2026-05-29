set(WG_SRCS
    "src/wg.cpp"
    "src/esp_wireguard/crypto.c"
    "src/esp_wireguard/wireguard.c"
    "src/esp_wireguard/wireguardif.c"
    "src/esp_wireguard/wireguard-platform.c"
    "src/esp_wireguard/esp_wireguard.c"
    "src/esp_wireguard/crypto/refc/blake2s.c"
    "src/esp_wireguard/crypto/refc/chacha20.c"
    "src/esp_wireguard/crypto/refc/chacha20poly1305.c"
    "src/esp_wireguard/crypto/refc/poly1305-donna.c"
    "src/esp_wireguard/crypto/refc/x25519.c"
    "src/esp_wireguard/nacl/crypto_scalarmult/curve25519/ref/smult.c")

include(${CMAKE_CURRENT_LIST_DIR}/spangap_requires.cmake)
idf_component_register(
    SRCS ${WG_SRCS}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src/esp_wireguard"
    REQUIRES
        ${SPANGAP_REQUIRES}
        driver
        esp_event
        esp_netif
        lwip
        esp_timer
)

# Vendored crypto trips two cosmetic warnings under GCC 14 / IDF 5.5; silence
# them locally rather than divergence-patching upstream:
#   x25519.c: stringop-overread false-positive — mul()'s nb arg dictates the
#       read length, but GCC's static analysis can't see through it.
#   smult.c:  unused loop counter in the curve25519 mainloop.
set_source_files_properties(src/esp_wireguard/crypto/refc/x25519.c
    PROPERTIES COMPILE_FLAGS -Wno-stringop-overread)
set_source_files_properties(src/esp_wireguard/nacl/crypto_scalarmult/curve25519/ref/smult.c
    PROPERTIES COMPILE_FLAGS -Wno-unused-variable)
