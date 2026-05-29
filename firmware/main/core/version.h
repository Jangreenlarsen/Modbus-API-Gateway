#pragma once

// Eneste kilde til version og build-nummer.
// Ændringer her recompilerer KUN de filer der inkluderer denne header
// (main.c, serial_cli.c, system.c, ota_manager.c) — ikke alle 13 config.h-filer.
#define GATEWAY_VERSION "0.1.0"
#define GATEWAY_BUILD   "0019"
