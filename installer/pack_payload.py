import os
import re
import sys

def bytes_to_c_array(data: bytes, var_name: str) -> str:
    lines = [f"alignas(16) static const unsigned char {var_name}[{len(data)}] = {{"]
    chunk_size = 16
    for i in range(0, len(data), chunk_size):
        chunk = data[i:i + chunk_size]
        hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {hex_str},")
    lines.append("};")
    return "\n".join(lines)

def extract_existing_array(header_path: str, var_name: str) -> bytes:
    with open(header_path, "r", encoding="utf-8") as f:
        text = f.read()
    m = re.search(rf"{re.escape(var_name)}\[\d+\]\s*=\s*\{{(.*?)\}};", text, re.S)
    if not m:
        raise RuntimeError(f"Could not find {var_name} in {header_path}")
    hex_tokens = re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1))
    return bytes(int(h, 16) for h in hex_tokens)

def main():
    if len(sys.argv) < 4:
        print("Usage: pack_payload.py <version_dll_path> <steam_dll_path> <output_header_path>")
        sys.exit(1)

    version_dll_path = sys.argv[1]
    steam_dll_path = sys.argv[2]
    out_header_path = sys.argv[3]

    with open(version_dll_path, "rb") as f:
        version_bytes = f.read()

    if os.path.exists(steam_dll_path):
        with open(steam_dll_path, "rb") as f:
            steam_bytes = f.read()
    elif os.path.exists(out_header_path):
        steam_bytes = extract_existing_array(out_header_path, "kSteamApi64DllBytes")
    else:
        raise FileNotFoundError(f"steam_api64.dll not found at {steam_dll_path} and no existing {out_header_path}")

    os.makedirs(os.path.dirname(os.path.abspath(out_header_path)), exist_ok=True)

    with open(out_header_path, "w", encoding="utf-8") as f:
        f.write("#pragma once\n")
        f.write("#include <cstddef>\n\n")
        f.write("namespace EmbeddedPayload {\n\n")
        f.write(bytes_to_c_array(version_bytes, "kVersionDllBytes"))
        f.write("\n\n")
        f.write(bytes_to_c_array(steam_bytes, "kSteamApi64DllBytes"))
        f.write("\n\n} // namespace EmbeddedPayload\n")

    print(f"[pack_payload] Packed version.dll ({len(version_bytes)} bytes) and steam_api64.dll ({len(steam_bytes)} bytes) -> {out_header_path}")

if __name__ == "__main__":
    main()
