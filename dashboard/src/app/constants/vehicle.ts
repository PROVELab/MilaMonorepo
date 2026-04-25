/**
 * Convert wheel RPM to MPH for the speed readout:
 * circumference per wheel revolution = 19 inches * pi,
 * miles per revolution = circumference / 63,360 inches per mile,
 * miles per hour = RPM * miles per revolution * 60 minutes per hour.
 */
export const TIRE_DIAMETER_INCHES = 19;
export const TIRE_CIRCUMFERENCE_INCHES = TIRE_DIAMETER_INCHES * Math.PI;
export const MOTOR_SPEED_RPM_TO_MPH = (TIRE_CIRCUMFERENCE_INCHES / 63_360) * 60;
