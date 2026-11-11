"""Startup-only XML configuration for simulated sensor nodes.

The XML exposes the primary noise settings. Other settings use the
defaults below unless explicitly included in the XML.

Restart the sensor nodes after changing the configuration.
"""

from pathlib import Path
import math
import xml.etree.ElementTree as ET


DEFAULT_CONFIG = Path(__file__).resolve().with_name("sensor_config.xml")


# Keep these schema names and setting names available to the sensor nodes.
IMU = {
    "nominal_rate_hz": "positive",
    "seed": "integer",
    "max_gap_seconds": "positive",
    "reset_bias_on_time_reset": "bool",
    "accel_noise_density_mps2_per_sqrt_hz": "nonnegative",
    "gyro_noise_density_radps_per_sqrt_hz": "nonnegative",
    "accel_initial_bias_std_mps2": "nonnegative",
    "gyro_initial_bias_std_radps": "nonnegative",
    "accel_bias_random_walk_mps2_per_sqrt_s": "nonnegative",
    "gyro_bias_random_walk_radps_per_sqrt_s": "nonnegative",
}

MAG = {
    "world_field_x_t": "float",
    "world_field_y_t": "float",
    "world_field_z_t": "float",
    "white_noise_std_t": "nonnegative",
    "hard_iron_bias_std_t": "nonnegative",
    "soft_iron_scale_std": "nonnegative",
    "soft_iron_cross_axis_std": "nonnegative",
    "random_seed": "integer",
    "mag_rate_hz": "nonnegative",
}


# These are the same values as in the previous XML.
IMU_DEFAULTS = {
    "nominal_rate_hz": 60.0,
    "seed": 17,
    "max_gap_seconds": 1.0,
    "reset_bias_on_time_reset": True,
    "accel_noise_density_mps2_per_sqrt_hz": 0.000588399,
    "gyro_noise_density_radps_per_sqrt_hz": 0.00013962634015954636,
    "accel_initial_bias_std_mps2": 0.03,
    "gyro_initial_bias_std_radps": 0.005,
    "accel_bias_random_walk_mps2_per_sqrt_s": 0.0002,
    "gyro_bias_random_walk_radps_per_sqrt_s": 0.00002,
}

MAG_DEFAULTS = {
    "world_field_x_t": 0.000020,
    "world_field_y_t": 0.000008,
    "world_field_z_t": -0.000042,
    "white_noise_std_t": 0.00000004,
    "hard_iron_bias_std_t": 0.000002,
    "soft_iron_scale_std": 0.03,
    "soft_iron_cross_axis_std": 0.01,
    "random_seed": 17,
    "mag_rate_hz": 0.0,
}


DEFAULTS_BY_SECTION = {
    "imu": IMU_DEFAULTS,
    "magnetometer": MAG_DEFAULTS,
}

# Require the main settings to be present so an accidental deletion
# doesn't silently change what the user expects to configure.
REQUIRED_BY_SECTION = {
    "imu": {
        "accel_noise_density_mps2_per_sqrt_hz",
        "gyro_noise_density_radps_per_sqrt_hz",
    },
    "magnetometer": {
        "white_noise_std_t",
    },
}


def load_config(path, section, schema):
    """Load one sensor section and fill omitted settings from defaults."""

    if section not in DEFAULTS_BY_SECTION:
        raise ValueError(f"Unknown sensor configuration section: {section}")

    defaults = DEFAULTS_BY_SECTION[section]

    if set(defaults) != set(schema):
        raise ValueError(
            f"Default settings and schema do not match for <{section}>"
        )

    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as exc:
        raise ValueError(f"Cannot read sensor config {path}: {exc}") from exc

    if root.tag != "sensor_simulation":
        raise ValueError("Expected <sensor_simulation> root")

    sections = root.findall(section)

    if len(sections) != 1:
        raise ValueError(f"Expected exactly one <{section}> section")

    result = defaults.copy()
    seen = set()

    for child in sections[0]:
        key = child.tag

        if key not in schema or key in seen or len(child):
            raise ValueError(
                f"Unknown, duplicated, or nested setting: {section}/{key}"
            )

        seen.add(key)
        value = (child.text or "").strip()
        kind = schema[key]

        try:
            if kind == "bool":
                if value.lower() not in ("true", "false"):
                    raise ValueError("must be true or false")

                parsed = value.lower() == "true"

            elif kind == "integer":
                parsed = int(value)

            else:
                parsed = float(value)

                if not math.isfinite(parsed):
                    raise ValueError("must be finite")

                if kind == "positive" and parsed <= 0:
                    raise ValueError("must be positive")

                if kind == "nonnegative" and parsed < 0:
                    raise ValueError("must be nonnegative")

        except ValueError as exc:
            raise ValueError(
                f"Invalid {section}/{key}: {value!r} ({exc})"
            ) from exc

        result[key] = parsed

    missing = REQUIRED_BY_SECTION[section] - seen

    if missing:
        raise ValueError(
            f"Missing required settings in <{section}>: {sorted(missing)}"
        )

    return result