#!/usr/bin/env bash

EDITOR_PATH="/bin/editor"
GAME_PATH="/bin/game"

ENV_VARS="XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
ARGS=""

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --target) TARGET="$2"; shift;; # Choose between game and editor apps
        --debug) ARGS="$ARGS --debug";; # Enables validation layers
        *) echo "Unknown argument: $1"; exit 1 ;;
    esac
    shift
done

case $TARGET in
    editor) RUN_PATH=$EDITOR_PATH;;
    game) RUN_PATH=$GAME_PATH;;
    *) echo "Unknown target: $TARGET"; exit 1;;
esac

# running with sudo might make VK_PRESENT_MODE_IMMEDIATE_KHR available
echo "Running $TARGET app"
sudo $ENV_VARS .$RUN_PATH $ARGS
