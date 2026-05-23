Import("env")
import datetime
import re

now = datetime.datetime.now()

# Leer config.h para extraer HW_STATUS y FW_REVISION
hw_status_str = ""
config_path = "src/config.h"
hw_components = [
    "Motor", "RS485", "Display", "ADC", "Fader",
    "FaderTouch", "NeoPixels", "Encoder", "Buttons"
]

try:
    with open(config_path, "r") as f:
        content = f.read()
        for comp in hw_components:
            match = re.search(rf"//\s*HW_STATUS:\s*{comp}=(\d)", content)
            if match:
                hw_status_str += match.group(1)
            else:
                hw_status_str += "0"  # default: no implementado
        rev_match = re.search(r'#define\s+FW_REVISION\s+(\d+)', content)
        fw_revision = int(rev_match.group(1)) if rev_match else 0
except:
    hw_status_str = "0" * len(hw_components)
    fw_revision = 0

# 0=debug | MINOR=sistemas status=2 | PATCH=FW_REVISION (contador acumulado sesiones)
status_values = [int(c) for c in hw_status_str]
fw_ver = f"0.{status_values.count(2)}.{fw_revision}"
ver_comp = fw_ver.replace(".", "")
build_id = now.strftime("%Y%m%d.%H%M") + f".{ver_comp}"

env.Append(CCFLAGS=[
    f'-DFW_VERSION=\\"{fw_ver}\\"',
    f'-DFW_BUILD_ID=\\"{build_id}\\"',
    f'-DHW_STATUS=\\"{hw_status_str}\\"',
])
print(f"[version] {build_id}")
print(f"[hw_status] {hw_status_str}")
