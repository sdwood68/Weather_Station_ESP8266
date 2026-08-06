# Weather Station Hardware Roadmap

This is the source of truth for electrical, mechanical, sensor-interface, power,
and physical validation work. Share it with the Weather Station Hardware
Project. `ROADMAP.md` owns firmware-only work and segregates software tasks
blocked by hardware.

## Priority 0 - Correct unsafe I2C logic levels

- [ ] Move installed 10 kOhm SDA and SCL pull-ups from 5 V to 3.3 V.
- [ ] Confirm at both ends of the four-foot cable that neither line exceeds 3.6 V.
- [ ] Inventory breakout-board pull-ups and calculate effective resistance.
- [ ] Inspect routing, grounds, connectors, corrosion, and moisture ingress.

Exit criteria: all controller-facing I2C logic is verified safe at 3.3 V.

## Priority 1 - Stabilize the outdoor sensor bus

- [ ] Measure I2C rise time with the installed cable after correcting the rail.
- [ ] If needed, test an effective pull-up near 4.7 kOhm while accounting for
      parallel pull-ups and capacitance.
- [ ] Compare at least 100 AM2315 transactions on installed and short cables.
- [ ] Capture NACK, short frame, invalid header, stuck-low, and CRC failures.
- [ ] Exercise cold boot, slow power rise, Wi-Fi, OTA reboot, cable disturbance,
      and several hours of operation.
- [ ] Decide whether the installed I2C distance needs a local controller,
      differential extender, or long-distance interface.

## Priority 2 - Weather sensor interfaces and calibration

- [ ] Document anemometer contact type, pulses per revolution, transfer function,
      cable protection, and maximum pulse rate.
- [ ] Record vane ADC readings at all 16 positions on every supported board.
- [ ] Provide true-north alignment and calibrated anemometer comparison data.
- [ ] Document rain switch, cable, surge protection, inches per tip, and maximum
      tip rate.
- [ ] Validate one edge per tip, controlled volume, high-rate behavior, leveling,
      and outdoor installation.
- [ ] Define pressure mounting, ventilation, elevation reference, and a traceable
      same-height calibration setup.

## Priority 3 - Power integrity and monitoring

- [ ] Evaluate Feather HUZZAH ESP8266 regulator, charging, and switchover limits.
- [ ] Select and test a supervisor such as TPS3839; distinguish reset supervision
      from true undervoltage lockout.
- [ ] Add an INA219 or equivalent monitor; reserve ESP8266 ADC for the wind vane.
- [ ] Evaluate protected LiPo ride-through, charging safety, temperature,
      lifetime, and replacement.
- [ ] Evaluate ESP32-S3 Feather, MAX17048 telemetry, ADCs, supervisor, and current
      monitoring requirements.
- [ ] Test Wi-Fi current steps, slow ramps, outages, sustained undervoltage,
      recovery, filesystem integrity, and reboot loops.
- [ ] Capture voltage, current, power, and energy for all operating phases.

## Priority 4 - Board-family and low-power architecture

- [ ] Publish pin maps and limits for D1 Mini, NodeMCU, Feather ESP8266, and ESP32.
- [ ] Decide whether wind and rain pulse counting continues during sleep.
- [ ] If deep sleep is required, select external pulse-counting and wake hardware.
- [ ] Define voltage, current, battery, and brownout diagnostic interfaces.
- [ ] Physically validate OTA, sensors, interrupts, ADC, and reboot recovery on
      every supported board family.

## Deliverables shared back to firmware

- [ ] Released schematic, BOM, board revision, assembly notes, and pin maps.
- [ ] Electrical safety and power-integrity report.
- [ ] Sensor calibration and physical-validation datasets.
- [ ] Tabulated constants, scaling factors, and acceptable ranges.
- [ ] Supported-board list and migration constraints.
- [ ] Hardware revision identifier for Home Assistant diagnostics.
