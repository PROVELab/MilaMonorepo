"""Strict, startup-only XML configuration for simulated sensor nodes."""
from pathlib import Path
import math
import xml.etree.ElementTree as ET

DEFAULT_CONFIG = Path(__file__).resolve().with_name('sensor_config.xml')

IMU = {
    'nominal_rate_hz': 'positive', 'seed': 'integer', 'max_gap_seconds': 'positive',
    'reset_bias_on_time_reset': 'bool', 'accel_noise_density_mps2_per_sqrt_hz': 'nonnegative',
    'gyro_noise_density_radps_per_sqrt_hz': 'nonnegative', 'accel_initial_bias_std_mps2': 'nonnegative',
    'gyro_initial_bias_std_radps': 'nonnegative', 'accel_bias_random_walk_mps2_per_sqrt_s': 'nonnegative',
    'gyro_bias_random_walk_radps_per_sqrt_s': 'nonnegative',
}
MAG = {'world_field_x_t': 'float', 'world_field_y_t': 'float', 'world_field_z_t': 'float',
       'white_noise_std_t': 'nonnegative', 'hard_iron_bias_std_t': 'nonnegative',
       'soft_iron_scale_std': 'nonnegative', 'soft_iron_cross_axis_std': 'nonnegative',
       'random_seed': 'integer', 'mag_rate_hz': 'nonnegative'}

def load_config(path, section, schema):
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as exc:
        raise ValueError(f'Cannot read sensor config {path}: {exc}') from exc
    if root.tag != 'sensor_simulation':
        raise ValueError('Expected <sensor_simulation> root')
    sections = root.findall(section)
    if len(sections) != 1:
        raise ValueError(f'Expected exactly one <{section}> section')
    result = {}
    for child in sections[0]:
        key = child.tag
        if key not in schema or key in result or len(child):
            raise ValueError(f'Unknown, duplicated, or nested setting: {section}/{key}')
        value = (child.text or '').strip()
        kind = schema[key]
        try:
            if kind == 'bool':
                if value.lower() not in ('true', 'false'):
                    raise ValueError('must be true or false')
                parsed = value.lower() == 'true'
            elif kind == 'integer':
                parsed = int(value)
            else:
                parsed = float(value)
                if not math.isfinite(parsed) or (kind == 'positive' and parsed <= 0) or (kind == 'nonnegative' and parsed < 0):
                    raise ValueError(f'must be finite and {kind}')
        except ValueError as exc:
            raise ValueError(f'Invalid {section}/{key}: {value!r} ({exc})') from exc
        result[key] = parsed
    missing = schema.keys() - result.keys()
    if missing:
        raise ValueError(f'Missing settings in <{section}>: {sorted(missing)}')
    return result
