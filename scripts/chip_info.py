"""PlatformIO target that identifies the P4 without writing its flash."""
import shlex
import subprocess

Import("env")  # noqa: F821 - injected by PlatformIO/SCons


def chip_info(source, target, env):
    env.AutodetectUploadPort()
    port = env.subst("$UPLOAD_PORT")
    if not port:
        print("No port found. Set upload_port in platformio.ini to the P4-NANO port.")
        return 1
    # Use the platform's configured esptool executable and its own dependencies.
    # A shell is intentionally not used, so spaces in paths/ports stay intact.
    uploader = shlex.split(env.subst("$UPLOADER"))
    return subprocess.call(
        uploader + ["--chip", "esp32p4", "--port", port, "chip-id"],
        env=env["ENV"],
    )


env.AddCustomTarget(  # noqa: F821
    name="chip-info",
    dependencies=None,
    actions=[chip_info],
    title="Chip revision (read only)",
    description="Read the P4 silicon revision with esptool; no flash write",
)


def set_image_revision(source, target, env):
    # The pinned platform omits IDF's revision flags from elf2image, including
    # for its cloned bootloader build environment. Set them on the actual
    # builder environment immediately before each binary is generated.
    import json
    from pathlib import Path

    config = json.loads((Path(env.subst("$BUILD_DIR")) / "config" / "sdkconfig.json").read_text())
    minimum = int(config["ESP32P4_REV_MIN_FULL"])
    maximum = int(config["ESP32P4_REV_MAX_FULL"])
    expected = (300, 399) if env["PIOENV"].endswith("_rev3") else (1, 199)
    if (minimum, maximum) != expected:
        raise RuntimeError("SDK chip revision does not match the environment; remove sdkconfig.<environment> and rebuild")
    env.Append(ELF2BINFLAGS=["--min-rev-full", str(minimum), "--max-rev-full", str(maximum)])


for binary in ("bootloader.bin", "${PROGNAME}.bin"):
    env.AddPreAction("$BUILD_DIR/" + binary, set_image_revision)  # noqa: F821
    env.Depends("$BUILD_DIR/" + binary, "$PROJECT_DIR/scripts/chip_info.py")  # noqa: F821
