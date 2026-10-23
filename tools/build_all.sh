#!/bin/sh
# Todos os passos de montagem, em ordem (chamado pelo setup.sh, na maquina ou no container).
# Uso: sh tools/build_all.sh <pasta do Diablo II> [--zip]
set -e
cd "$(dirname "$0")/.."
sh src-d2nx/build.sh
python3 tools/fetch_medianxl.py
sh tools/build_tools.sh
python3 tools/prepare_game.py --d2 "$1"
python3 tools/assemble_sd_tree.py
if [ "$2" = "--zip" ]; then python3 tools/package_release.py; fi
