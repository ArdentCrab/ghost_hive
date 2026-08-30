# Warte auf PSP-USB, kopiert nur peer.bind (112 Byte). Nie root.key.
import os
import shutil
import sys
import time

src = None
for _ in range(60):
    for d in "DEFGHIJ":
        p = d + ":\\ghost_hive\\k\\peer.bind"
        if os.path.isfile(p) and os.path.getsize(p) == 112:
            src = p
            break
    if src:
        break
    time.sleep(1)
if not src:
    print("Kein peer.bind 112B. GHOST laufen lassen, dann USB.")
    sys.exit(1)
dst_win = os.path.join(
    os.environ["USERPROFILE"], "Documents", "ghost_hive_peers", "k", "peer.bind")
os.makedirs(os.path.dirname(dst_win), exist_ok=True)
shutil.copy2(src, dst_win)
os.chmod(dst_win, 0o600)
print("ok", dst_win, os.path.getsize(dst_win))
sys.exit(0)
