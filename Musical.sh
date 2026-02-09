#!/bin/sh

SCRIPT_DIR="$(CDPATH= cd "$(dirname "$0")" 2>/dev/null && pwd)"
MUOS_DIR="$(CDPATH= cd "$SCRIPT_DIR/../.." 2>/dev/null && pwd)"
GAMEDIR="$MUOS_DIR/ports/Musical"

LOG="$GAMEDIR/log.txt"
mkdir -p "$GAMEDIR" "$GAMEDIR/save"

exec >"$LOG" 2>&1

echo "=== Musical launch ==="
date
echo "SCRIPT_DIR=$SCRIPT_DIR"
echo "MUOS_DIR=$MUOS_DIR"
echo "GAMEDIR=$GAMEDIR"
echo "pwd=$(pwd)"
echo "uname=$(uname -a)"

FRONTEND="/opt/muos/script/mux/frontend.sh"

# もとの環境を保存（重要）
ORIG_LD_LIBRARY_PATH="$LD_LIBRARY_PATH"

cleanup() {
  # trap が二重に走らないように
  trap - EXIT INT TERM

  echo "=== cleanup: restarting muOS frontend ==="
  date

  # ここで「ゲーム用の環境」を確実に外す（重要）
  export LD_LIBRARY_PATH="$ORIG_LD_LIBRARY_PATH"
  unset SDL_VIDEODRIVER SDL_RENDER_DRIVER SDL_RENDER_VSYNC SDL_AUDIODRIVER

  # 念のため、残骸がいたら止める（muOSバージョン差分対策）
  killall -q Musical 2>/dev/null || true
  killall -q muxfrontend frontend.sh muxlaunch 2>/dev/null || true

  # フロントエンド再起動（これでメニューに戻る）
  if [ -x "$FRONTEND" ]; then
    echo "exec $FRONTEND"
    exec "$FRONTEND"
  else
    echo "ERROR: frontend script not found: $FRONTEND"
  fi
}

trap cleanup EXIT INT TERM

# バイナリ存在＆権限
ls -la "$GAMEDIR" || true
ls -la "$GAMEDIR/Musical" || true

cd "$GAMEDIR" || exit 1

# muOS: フロントエンド停止（公式系の作法）
killall -q muxfrontend frontend.sh muxlaunch 2>/dev/null || true

echo "--- ldd ---"
ldd "$GAMEDIR/Musical" || true
echo "------------"

# ゲームだけに環境変数を適用（exportしないのがコツ）
env \
  HOME="$GAMEDIR" \
  LD_LIBRARY_PATH="$GAMEDIR/libs.aarch64:$ORIG_LD_LIBRARY_PATH" \
  SDL_RENDER_DRIVER=opengles2 \
  SDL_RENDER_VSYNC=0 \
  SDL_AUDIODRIVER=alsa \
  "$GAMEDIR/Musical"

RET=$?
echo "Musical exited with code=$RET"
exit "$RET"
