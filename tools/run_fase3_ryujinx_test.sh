#!/bin/bash
set -e

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$BASE_DIR"

echo "=== FASE 3: Smoke Test Ryujinx / Wine-NX (Median XL 2.14.6) ==="

# 1. Assegurar sincronizacao da arvore SD
python3 tools/assemble_sd_tree.py

# 2. Limpar logs anteriores
rm -f build/ryujinx-test/sdcard/switch/wine/logs/*
rm -f build/ryujinx-test/sdcard/switch/wine/drive_c/DiabloII/D2Autorun.log
rm -f build/ryujinx-test/sdcard/switch/wine/drive_c/DiabloII/d2nx/d2nxpad.log
rm -f build/ryujinx-test/ryu_fase3_test.log
rm -f build/ryujinx-test/sdcard/switch/wine/run-next.txt.previous

echo "Ponteiro run-next.txt: $(cat build/ryujinx-test/sdcard/switch/wine/run-next.txt)"

# 3. Habilitar telemetria de profiling para monitorar frames
touch build/ryujinx-test/sdcard/switch/wine/profile.txt

# 4. Iniciar Ryujinx
echo "Iniciando Ryujinx..."
ryujinx \
    --root-data-dir "$BASE_DIR/build/ryujinx-test" \
    --handheld-mode \
    --hide-updates \
    "$BASE_DIR/build/ryujinx-test/hbl/ncas/nca-1.nca" > "$BASE_DIR/build/ryujinx-test/ryu_fase3_test.log" 2>&1 &

RYU_PID=$!
echo "Ryujinx iniciado com PID $RYU_PID. Monitorando (janela de ate 180s)..."

# 5. Monitorar telemetria
python3 -c '
import time, os, sys

ryu_pid = int(sys.argv[1])
log_dir = "build/ryujinx-test/sdcard/switch/wine/logs"
d2_dir = "build/ryujinx-test/sdcard/switch/wine/drive_c/DiabloII"
start = time.time()
last_rt = ""
last_d2 = ""
last_pad = ""
success = False

while time.time() - start < 180:
    time.sleep(3)
    elapsed = time.time() - start
    
    rt_log = os.path.join(log_dir, "autorun_runtime.log")
    if os.path.exists(rt_log):
        lines = open(rt_log, errors="replace").readlines()
        prog = [l.strip() for l in lines if "[PROGRESS]" in l or "[NXWIN]" in l or "gl_frames=" in l or "Halt" in l or "c0000017" in l or "Game.exe" in l]
        if prog and prog[-1] != last_rt:
            print(f"T={elapsed:5.1f}s [RT]: {prog[-1]}", flush=True)
            last_rt = prog[-1]
            if "gl_frames=" in prog[-1]:
                # Check frame count
                for part in prog[-1].split():
                    if part.startswith("gl_frames="):
                        val = int(part.split("=")[1])
                        if val > 10:
                            success = True
            if "Halt" in prog[-1]:
                print("FATAL: Halt detectado no runtime!", flush=True)
                break
                
    d2_log = os.path.join(d2_dir, "D2Autorun.log")
    if os.path.exists(d2_log):
        lines = open(d2_log, errors="replace").readlines()
        if lines and lines[-1].strip() != last_d2:
            print(f"T={elapsed:5.1f}s [D2Auto]: {lines[-1].strip()}", flush=True)
            last_d2 = lines[-1].strip()
            
    pad_log = os.path.join(d2_dir, "d2nx", "d2nxpad.log")
    if os.path.exists(pad_log):
        lines = open(pad_log, errors="replace").readlines()
        if lines and lines[-1].strip() != last_pad:
            print(f"T={elapsed:5.1f}s [PAD]: {lines[-1].strip()}", flush=True)
            last_pad = lines[-1].strip()

    if success and elapsed > 20:
        print("Sucesso confirmado: D2 Sigma + Autorun renderizando frames estaveis!", flush=True)
        break

print("Fim da janela de monitoramento.")
' "$RYU_PID"

echo "Encerrando Ryujinx PID $RYU_PID..."
kill -15 $RYU_PID 2>/dev/null || true
sleep 2
kill -9 $RYU_PID 2>/dev/null || true
wait $RYU_PID 2>/dev/null || true

echo "=== Resumo dos Logs da Fase 3 ==="
if [ -f "build/ryujinx-test/sdcard/switch/wine/drive_c/DiabloII/D2Autorun.log" ]; then
    echo "--- D2Autorun.log ---"
    cat "build/ryujinx-test/sdcard/switch/wine/drive_c/DiabloII/D2Autorun.log"
fi

if [ -f "build/ryujinx-test/sdcard/switch/wine/drive_c/DiabloII/d2nx/d2nxpad.log" ]; then
    echo "--- d2nxpad.log ---"
    head -n 20 "build/ryujinx-test/sdcard/switch/wine/drive_c/DiabloII/d2nx/d2nxpad.log"
fi

echo "--- Ultimas 10 linhas do autorun_runtime.log ---"
if [ -f "build/ryujinx-test/sdcard/switch/wine/logs/autorun_runtime.log" ]; then
    tail -n 10 "build/ryujinx-test/sdcard/switch/wine/logs/autorun_runtime.log"
else
    echo "autorun_runtime.log nao foi gerado."
fi
