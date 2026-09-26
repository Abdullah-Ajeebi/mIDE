import re
import urllib.request
import sys

URL = "https://raw.githubusercontent.com/Anuken/Mindustry/refs/heads/master/core/src/mindustry/logic/LExecutor.java"

# Known mlog opcode mappings for LInstruction class names
OPCODE_MAP = {
    "UnitBindI": "ubind",
    "UnitLocateI": "ulocate",
    "UnitControlI": "ucontrol",
    "UnitRadarI": "uradar",
    "ControlI": "control",
    "GetLinkI": "getlink",
    "ReadI": "read",
    "WriteI": "write",
    "SenseI": "sensor",
    "RadarI": "radar",
    "SetI": "set",
    "OpI": "op",
    "SelectI": "select",
    "DrawI": "draw",
    "DrawFlushI": "drawflush",
    "PrintI": "print",
    "PrintCharI": "printchar",
    "FormatI": "format",
    "PrintFlushI": "printflush",
    "JumpI": "jump",
    "SetRateI": "setrate",
    "WaitI": "wait",
    "StopI": "stop",
    "EndI": "end",
    "LookupI": "lookup",
    "PackColorI": "packcolor",
    "UnpackColorI": "unpackcolor",
    "CutsceneI": "cutscene",
    "FetchI": "fetch",
    "SyncI": "sync",
    "EffectI": "effect",
    "ExplosionI": "explosion",
    "SetBlockI": "setblock",
    "SpawnWaveI": "spawnwave",
    "SetRuleI": "setrule",
    "MessageI": "message",
    "SpawnUnitI": "spawnunit",
    "ApplyStatusI": "applystatus",
    "TakeItemsI": "takeitems"
}

# Parameters that mean "This function returns a value instead of void"
OUTPUT_PARAM_NAMES = {"output", "dest", "to", "result", "outx", "outy", "outfound", "outbuild"}

def fetch_source():
    print(f"[*] Fetching LExecutor.java from GitHub...")
    try:
        req = urllib.request.Request(URL, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req) as response:
            return response.read().decode('utf-8')
    except Exception as e:
        print(f"[!] Network fetch failed: {e}")
        print("[*] Trying local 'LExecutor.java'...")
        with open("LExecutor.java", "r", encoding="utf-8") as f:
            return f.read()

def parse_instructions(java_code):
    # Match: public static class [Name]I implements LInstruction { ... }
    class_pattern = re.compile(
        r"public\s+static\s+class\s+(\w+I)\s+implements\s+LInstruction\s*\{(.*?)(?=\n\s*public\s+static\s+class|\n\s*//endregion|\Z)",
        re.DOTALL
    )

    instructions = []

    for match in class_pattern.finditer(java_code):
        class_name = match.group(1)
        body = match.group(2)

        if class_name in ("NoopI", "JumpI", "OpI", "SetI"):
            continue # Handled natively by compiler syntax (if/math/assignment)

        c_name = OPCODE_MAP.get(class_name, class_name.lower().removesuffix("i"))

        # Find parameterized constructor: public NameI(type param, type param2, ...)
        ctor_pattern = re.compile(rf"public\s+{class_name}\s*\((.*?)\)", re.DOTALL)
        ctor_match = ctor_pattern.search(body)

        params = []
        if ctor_match:
            raw_params = ctor_match.group(1).strip()
            if raw_params:
                # Split comma-separated parameters
                for p in raw_params.split(','):
                    parts = p.strip().split()
                    if len(parts) >= 2:
                        p_type, p_name = parts[-2], parts[-1]
                        params.append((p_name, p_type))

        # Separate output/return parameters from input arguments
        input_args = []
        output_arg = None

        for name, p_type in params:
            if name.lower() in OUTPUT_PARAM_NAMES and output_arg is None:
                output_arg = name
            else:
                input_args.append(name)

        instructions.append({
            "class": class_name,
            "c_name": c_name,
            "inputs": input_args,
            "returns_value": output_arg is not None,
            "output_name": output_arg
        })

    return instructions

def generate_cpp(instructions):
    # 1. Register helper
    reg_lines = [
        "// Auto-generated built-in registrar",
        "template<typename T>",
        "void RegisterMindustryBuiltins(T& functions_) {",
    ]
    for inst in instructions:
        args_str = ", ".join([f'L"{arg}"' for arg in inst["inputs"]])
        ret_val = "true" if inst["returns_value"] else "false"
        reg_lines.append(f'    functions_.insert({{ L"{inst["c_name"]}", {{ {{ {args_str} }}, true, {ret_val} }} }});')
    reg_lines.append("}\n")

    # 2. Emit helper function (returns true if handled)
    emit_lines = [
        "// Auto-generated instruction emitter",
        "template<typename TEmit, typename TNewTemp>",
        "bool TryEmitMindustryBuiltin(const std::wstring& name, const std::vector<std::wstring>& arguments, TEmit&& Emit, TNewTemp&& NewTemporary, std::wstring& outResult) {",
    ]
    for inst in instructions:
        c_name = inst["c_name"]
        emit_lines.append(f'    if (name == L"{c_name}") {{')
        if inst["returns_value"]:
            emit_lines.append('        std::wstring temp = NewTemporary();')
            arg_chain = ' + L" " + '.join([f'arguments[{i}]' for i in range(len(inst["inputs"]))])
            if arg_chain:
                emit_lines.append(f'        Emit(L"{c_name} " + temp + L" " + {arg_chain});')
            else:
                emit_lines.append(f'        Emit(L"{c_name} " + temp);')
            emit_lines.append('        outResult = temp;')
            emit_lines.append('        return true;')
        else:
            if inst["inputs"]:
                arg_chain = ' + L" " + '.join([f'arguments[{i}]' for i in range(len(inst["inputs"]))])
                emit_lines.append(f'        Emit(L"{c_name} " + {arg_chain});')
            else:
                emit_lines.append(f'        Emit(L"{c_name}");')
            emit_lines.append('        outResult = L"0";')
            emit_lines.append('        return true;')
        emit_lines.append('    }')
    emit_lines.append("    return false;\n}")

    # 3. CallTips map
    calltip_lines = [
        "// Auto-generated CallTips dictionary",
        "static const std::map<std::wstring, std::wstring> g_CallTipSignatures = {"
    ]
    for inst in instructions:
        c_name = inst["c_name"]
        sig_args = ", ".join([f"double {arg}" for arg in inst["inputs"]])
        ret_type = "double" if inst["returns_value"] else "void"
        calltip_lines.append(f'    {{ L"{c_name}", L"{c_name}({sig_args}) -> {ret_type}" }},')
    calltip_lines.append("};\n")

    # 4. Standard C Header (mindustry.h) - THE MISSING 4TH RETURN ITEM!
    header_lines = [
        "// --- Mindustry Standard Library Header (mindustry.h) ---",
        "#pragma once",
        "typedef double block;",
        "typedef double unit;",
        "typedef double item;",
        "typedef double INT_PTR; // as requested",
        ""
    ]
    for inst in instructions:
        c_name = inst["c_name"]
        sig_args = ", ".join([f"double {arg}" for arg in inst["inputs"]])
        ret_type = "double" if inst["returns_value"] else "void"
        header_lines.append(f"{ret_type} {c_name}({sig_args});")

    return "\n".join(reg_lines), "\n".join(emit_lines), "\n".join(calltip_lines), "\n".join(header_lines)

def main():
    raw_java = fetch_source()
    instructions = parse_instructions(raw_java)
    print(f"[+] Successfully parsed {len(instructions)} Mindustry instructions!")

    reg_cpp, emit_cpp, tips_cpp, header_c = generate_cpp(instructions)

    with open("MindustryBuiltins.inl", "w", encoding="utf-8") as f:
        f.write(reg_cpp + "\n\n" + emit_cpp)
    with open("MindustryCallTips.inl", "w", encoding="utf-8") as f:
        f.write(tips_cpp)
    with open("mindustry.h", "w", encoding="utf-8") as f:
        f.write(header_c)

    print("[*] Output files generated:")
    print("    - MindustryBuiltins.inl (Paste into Compiler.cpp)")
    print("    - MindustryCallTips.inl (Paste into GutteredTextEditor.h)")
    print("    - mindustry.h (C Header for auto-complete/syntax)")

if __name__ == "__main__":
    main()