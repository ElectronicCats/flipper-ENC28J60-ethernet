import os
import shlex
import subprocess

def test_escape(yaml_string, name):
    print(f"\n--- Testing: {name} ---")
    print(f"YAML content equivalent: {yaml_string}")
    
    # Simulate SCons parsing
    try:
        flags = shlex.split(yaml_string)
        print("SCons splits into:", flags)
    except Exception as e:
        print("shlex.split failed:", e)
        return

    # Create dummy C file
    with open("test.c", "w") as f:
        f.write('''
#include <stdio.h>
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

int main() {
    printf("Value: %s\\n", FAP_APP_VERSION);
    printf("Stringified: %s\\n", TOSTRING(FAP_APP_VERSION));
    return 0;
}
''')

    cmd = ["gcc", "test.c", "-o", "test.exe"] + flags
    print("GCC Command:", cmd)
    try:
        subprocess.run(cmd, check=True, capture_output=True)
        res = subprocess.run(["./test.exe"], capture_output=True, text=True)
        print("C Program Output:")
        print(res.stdout.strip())
    except subprocess.CalledProcessError as e:
        print("GCC Failed. Stderr:")
        print(e.stderr.decode('utf-8'))
    except Exception as e:
        print("Execution failed:", e)

# Scenario 1: '-DFAP_APP_VERSION=\"v1.2.3\"' (Single quotes in YAML)
# The string literally contains \ and "
test_escape('-DFAP_APP_VERSION=\\"v1.2.3\\"', "YAML Single Quotes with backslash-quote")

# Scenario 2: "-DFAP_APP_VERSION=\"v1.2.3\"" (Double quotes in YAML)
# YAML parses \" as a double quote, so string literally contains ONLY " (no backslash)
test_escape('-DFAP_APP_VERSION="v1.2.3"', "YAML Double Quotes with backslash-quote")

# Scenario 3: "-DFAP_APP_VERSION='\"v1.2.3\"'" 
test_escape("-DFAP_APP_VERSION='\"v1.2.3\"'", "Nested quotes")
