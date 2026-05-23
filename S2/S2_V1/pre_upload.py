Import("env")
import re, subprocess, os
from datetime import datetime

def bump_and_log(source, target, env):
    project_dir = env.subst("$PROJECT_DIR")
    config_path = os.path.join(project_dir, "src", "config.h")
    changelog   = os.path.join(project_dir, "..", "..", "CHANGELOG.md")
    env_name    = env["PIOENV"]

    try:
        with open(config_path, "r") as f:
            content = f.read()

        # Leer FW_REVISION actual
        rev_match = re.search(r"(#define\s+FW_REVISION\s+)(\d+)", content)
        if not rev_match:
            print("[FW_REVISION] ⚠️  no encontrado en config.h")
            return

        old_rev = int(rev_match.group(2))
        new_rev = old_rev + 1

        # Calcular MINOR desde HW_STATUS
        hw_components = ["Motor","RS485","Display","ADC","Fader","FaderTouch","NeoPixels","Encoder","Buttons"]
        hw_vals = []
        for comp in hw_components:
            m = re.search(rf"//\s*HW_STATUS:\s*{comp}=(\d)", content)
            hw_vals.append(int(m.group(1)) if m else 0)
        minor  = hw_vals.count(2)
        fw_ver = f"0.{minor}.{new_rev}"

        # Bump FW_REVISION en config.h
        content = re.sub(r"(#define\s+FW_REVISION\s+)\d+",
                         f"{rev_match.group(1)}{new_rev}", content)
        with open(config_path, "w") as f:
            f.write(content)

        # Log en CHANGELOG
        ts       = datetime.now().strftime("%Y-%m-%d %H:%M")
        log_line = f"- `{ts}` · Flash S2 · **FW {fw_ver}** · `{env_name}`\n"
        marker   = "### Upload log S2\n"

        with open(changelog, "r") as f:
            ch = f.read()

        if marker not in ch:
            ch = ch.replace("## [Unreleased]\n",
                            f"## [Unreleased]\n\n{marker}{log_line}\n", 1)
        else:
            ch = ch.replace(marker, marker + log_line, 1)

        with open(changelog, "w") as f:
            f.write(ch)

        # Stage config.h para que el próximo commit lo incluya
        subprocess.run(["git", "add", config_path], cwd=project_dir, check=False)

        print(f"[FW_REVISION] {old_rev} → {new_rev}  |  FW {fw_ver}  |  {ts}")

    except Exception as e:
        print(f"[FW_REVISION] ERROR: {e}")

env.AddPreAction("upload", bump_and_log)
