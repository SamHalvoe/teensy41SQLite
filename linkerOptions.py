Import("env")

# Replace original teensy41 linker script with our "sqlite3" linker script
linkflags = env.get("LINKFLAGS")
linkflags.remove("-T")
linkflags.remove("imxrt1062_t41.ld")
linkflags.append("-T$PROJECT_DIR/linkerScript/imxrt1062_t41_sqlite3.ld")

# Dump build environment (for debug)
#print(env.Dump())
