import os

ttf_file = "Zain-Bold.ttf"  # .ttf here

if not os.path.exists(ttf_file):
    raise FileNotFoundError(f"Input file '{ttf_file}' does not exist.")
if not ttf_file.lower().endswith('.ttf'):
    raise ValueError(f"Input file '{ttf_file}' must be a .ttf file.")

with open(ttf_file, "rb") as f:
    font_data = f.read()

base_name = os.path.splitext(os.path.basename(ttf_file))[0].lower().replace('-', '_')
output_file = f"{base_name}.h"

with open(output_file, "w") as header_file:
    header_guard = f"{base_name.upper()}_H"
    header_file.write(f"#ifndef {header_guard}\n")
    header_file.write(f"#define {header_guard}\n\n")
    header_file.write(f"const unsigned char {base_name}_data[] = {{\n    ")
    for i, byte in enumerate(font_data):
        header_file.write(f"0x{byte:02X}")
        if i < len(font_data) - 1:
            header_file.write(", ")
    header_file.write("\n};\n\n")
    header_file.write(f"const unsigned int {base_name}_data_size = {len(font_data)};\n\n")
    header_file.write(f"#endif")

print(f"Generated '{output_file}' with array '{base_name}_data' ({len(font_data)} bytes).")