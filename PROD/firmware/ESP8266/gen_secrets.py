"""
===============================================================================
FILE: gen_secrets.py
ROLE:
    Script PRE-BUILD PlatformIO : génère `include/secrets.h` à partir de la
    configuration centrale `.machine_config.ini` (racine PROD/).

ARCHITECTURE:
    .machine_config.ini  ->  gen_secrets.py (au `pio run`)  ->  include/secrets.h
        ->  config.h  ->  main.cpp  (WIFI_SSID / WIFI_PASSWORD / AUTH_TOKEN / HTTP_PORT)

POURQUOI:
    Le firmware C++ est COMPILÉ : il ne peut pas lire un .ini au runtime. On
    transforme donc le .ini en #define au moment du build. Résultat : une seule
    source de vérité pour les secrets (le .ini), partagée avec le backend.

ACTIVATION (platformio.ini):
    extra_scripts = pre:gen_secrets.py

MAINTAINER NOTES:
    - `secrets.h` est GITIGNORÉ (contient le SSID/mot de passe/jeton réels).
    - Si `.machine_config.ini` est introuvable, on n'écrase PAS un secrets.h
      existant et on laisse config.h utiliser ses valeurs par défaut (le build
      ne casse jamais à cause d'un .ini manquant).
    - Après modif du .ini : `pio run` régénère secrets.h, puis RE-FLASHER l'ESP.
===============================================================================
"""

import configparser
from pathlib import Path

Import("env")  # noqa: F821  (injecté par PlatformIO/SCons)

CONFIG_FILENAME = ".machine_config.ini"
PROJECT_DIR = Path(env["PROJECT_DIR"])  # noqa: F821  -> firmware/ESP8266
SECRETS_H = PROJECT_DIR / "include" / "secrets.h"


def find_config() -> Path | None:
    """Remonte depuis le projet jusqu'à trouver `.machine_config.ini`."""
    for folder in [PROJECT_DIR, *PROJECT_DIR.parents]:
        candidate = folder / CONFIG_FILENAME
        if candidate.is_file():
            return candidate
    return None


def c_escape(value: str) -> str:
    """Échappe une valeur pour une chaîne C (guillemets / antislash)."""
    return value.replace("\\", "\\\\").replace('"', '\\"')


def main():
    cfg_path = find_config()
    if cfg_path is None:
        print(f"[gen_secrets] ⚠️  {CONFIG_FILENAME} introuvable — "
              f"secrets.h non régénéré (config.h utilisera ses défauts).")
        return

    ini = configparser.ConfigParser()
    ini.read(cfg_path, encoding="utf-8")

    ssid = ini.get("wifi", "ssid", fallback="NodeMCU-Control")
    password = ini.get("wifi", "password", fallback="")
    token = ini.get("nodemcu", "key", fallback="")
    http_port = ini.get("esp8266", "http_port", fallback=ini.get("nodemcu", "port", fallback="8080"))

    SECRETS_H.parent.mkdir(parents=True, exist_ok=True)
    SECRETS_H.write_text(
        "// AUTO-GÉNÉRÉ par gen_secrets.py depuis .machine_config.ini — NE PAS COMMITTER.\n"
        "#pragma once\n"
        f'#define SECRET_WIFI_SSID "{c_escape(ssid)}"\n'
        f'#define SECRET_WIFI_PASSWORD "{c_escape(password)}"\n'
        f'#define SECRET_AUTH_TOKEN "{c_escape(token)}"\n'
        f"#define SECRET_HTTP_PORT {int(http_port)}\n",
        encoding="utf-8",
    )
    print(f"[gen_secrets] ✓ secrets.h généré depuis {cfg_path} "
          f"(SSID='{ssid}', port={http_port}).")


main()
