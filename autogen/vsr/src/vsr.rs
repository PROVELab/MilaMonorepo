use crate::vsr_gen;

vsr_gen!(
    version: 1;

    motor_power {
        measured_dc_voltage_v<f32>("V") "DC Bus Voltage";
        calculated_dc_current_a<f32>("A") "Calculated DC Current";
        motor_current_limit_arms<f32>("A") "Motor Current Limit";
    }

    motor_speed {
        quadrature_current<f32>("A") "Quadrature Current (Arms)";
        direct_current<f32>("A") "Direct Current (Arms)";
        motor_speed<i16>("RPM") "Motor Speed";
    }

    motor_safety {
        protection_code<u8>("") "Protection Code";
        safety_error_code<u8>("") "Safety Error Code";
        motor_temp<i16>("F") "Motor Temperature";
        inverter_bridge_temp<i16>("F") "Inverter Bridge Temperature";
        bus_cap_temp<i16>("F") "Bus Capacitor Temperature";
        pwm_status<u8>("") "PWM Status";
    }

    motor_control {
        current_reference<i32>("Arms") "Current Reference";
        discharge_limit_pct<u8>("%") "Discharge Limit";
        charge_limit_pct<u8>("%") "Charge Limit";
    }

    motor_error {
        motor_state<u32>("") "High Level Motor State Enum";
    }

    pedal {
        pedal_supply_voltage<f32>("mV") "Pedal Supply Voltage";
        pedal_position_pct<f32>("%") "Pedal Position";
        pedal_raw_1<f32>("") "Pedal Raw ADC 1";
        pedal_raw_2<f32>("") "Pedal Raw ADC 2";
        tx_value<i32>("") "Transmit Value (CAN)";
        use_pedal<bool>("") "Use Pedal Flag";
    }

    motor_prot1 {
        can_timeout_ms<u16>("ms") "CAN Timeout";
        dc_regen_current_limit_neg_a<u16>("A") "DC Regen Current Limit";
        dc_traction_current_limit_a<u16>("A") "DC Traction Current Limit";
        stall_protection_type<u8>("") "Stall Protection Type";
        stall_protection_time_ms<u16>("ms") "Stall Protection Time";
        stall_protection_current_a<u16>("A") "Stall Protection Current";
        overspeed_protection_speed_rpm<u8>("RPM") "Overspeed Protection Speed (x10)";
    }

    motor_prot2 {
        max_motor_temp_c<u8>("C") "Max Motor Temp";
        motor_temp_high_gain_a_per_c<u8>("A/C") "Motor Temp High Gain";
        max_inverter_temp_c<u8>("C") "Max Inverter Temp";
        inverter_temp_high_gain_a_per_c<u8>("A/C") "Inverter Temp High Gain";
        id_overcurrent_limit_a<u16>("A") "Id Overcurrent Limit";
        overvoltage_limit_v<u16>("V") "Overvoltage Limit";
        shutdown_voltage_limit_v<u16>("V") "Shutdown Voltage Limit";
    }
);