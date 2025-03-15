#!/usr/bin/env bash

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --target) TARGET="$2"; shift ;;   # Store value and skip to next argument
        # --debug) DEBUG=true ;;         # Set a flag (no value needed)
        *) echo "Unknown argument: $1"; exit 1 ;;
    esac
    shift
done

EDITOR_PATH="/bin/editor"
GAME_PATH="/bin/game"

ARGS="XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"

case $TARGET in
    editor) RUN_PATH=$EDITOR_PATH;;
    game) RUN_PATH=$GAME_PATH;;
    *) echo "Unknown target: $TARGET"; exit 1;;
esac

# running with sudo might make VK_PRESENT_MODE_IMMEDIATE_KHR available
echo "Running $TARGET app"
sudo $ARGS .$RUN_PATH
