"""Single source of truth for the K40 tube-current sensor board.

Every part: ref, value, footprint (lib, name), JLC/LCSC part, position (mm from board
top-left), rotation (deg), and pad -> net map.
"""

BOARD_W, BOARD_H = 68.0, 42.0
ORIGIN = (100.0, 100.0)          # where the board sits on the KiCad page
U2_X = 30.0                      # isolator centre; barrier is U2_X +/- 2.55 mm

R, C = 'Resistor_SMD', 'Capacitor_SMD'
R0603 = (R, 'R_0603_1608Metric')
C0603 = (C, 'C_0603_1608Metric')
C0805 = (C, 'C_0805_2012Metric')

# ref, value, footprint, lcsc, (x, y), rot, {pad: net}
PARTS = [
    # ---- primary (panel) side ----------------------------------------------------
    ('J1', 'I2C to panel', ('TerminalBlock_Phoenix', 'TerminalBlock_Phoenix_PT-1,5-4-3.5-H_1x04_P3.50mm_Horizontal'),
     '', (4.5, 9.5), 270, {'1': 'SCL_P', '2': 'SDA_P', '3': '+3V3', '4': 'GND'}),
    ('J2', '5V in', ('TerminalBlock_Phoenix', 'TerminalBlock_Phoenix_PT-1,5-2-3.5-H_1x02_P3.50mm_Horizontal'),
     '', (4.5, 27.5), 270, {'1': '+5V', '2': 'GND'}),
    ('U2', 'ADM3260ARSZ', ('Package_SO', 'SSOP-20_5.3x7.2mm_P0.65mm'), 'C208558', (U2_X, 21.0), 0, {
        '1': 'GND', '2': 'SCL_P', '3': 'SDA_P', '4': '+3V3', '5': 'GND', '6': '', '7': 'GND',
        '8': 'GND', '9': '+5V', '10': 'GND',
        '11': 'GNDISO', '12': 'VISO', '13': 'VSEL', '14': 'GNDISO', '15': '', '16': 'GNDISO',
        '17': 'VISO', '18': 'SDA_I', '19': 'SCL_I', '20': 'GNDISO'}),
    ('C1', '10uF 25V', C0805, 'C15850', (21.5, 27.0), 90, {'1': '+5V', '2': 'GND'}),
    ('C2', '100nF', C0603, 'C14663', (24.0, 27.0), 90, {'1': '+5V', '2': 'GND'}),
    ('C3', '100nF', C0603, 'C14663', (24.0, 16.0), 90, {'1': '+3V3', '2': 'GND'}),
    ('R6', '4.7k', R0603, 'C23162', (17.0, 14.0), 90, {'1': '+3V3', '2': 'SCL_P'}),
    ('R7', '4.7k', R0603, 'C23162', (19.0, 14.0), 90, {'1': '+3V3', '2': 'SDA_P'}),

    # ---- isolated (tube) side ----------------------------------------------------
    ('C4', '10uF 25V', C0805, 'C15850', (35.5, 27.5), 90, {'1': 'VISO', '2': 'GNDISO'}),
    ('C5', '100nF', C0603, 'C14663', (35.5, 31.0), 0, {'1': 'VISO', '2': 'GNDISO'}),
    ('C6', '100nF', C0603, 'C14663', (35.8, 15.0), 90, {'1': 'VISO', '2': 'GNDISO'}),
    ('R4', '30k', R0603, 'C22984', (38.0, 24.0), 90, {'1': 'VISO', '2': 'VSEL'}),
    ('R5', '10k', R0603, 'C25804', (40.0, 24.0), 90, {'1': 'VSEL', '2': 'GNDISO'}),
    ('R8', '4.7k', R0603, 'C23162', (37.5, 12.0), 90, {'1': 'VISO', '2': 'SCL_I'}),
    ('R9', '4.7k', R0603, 'C23162', (39.5, 12.0), 90, {'1': 'VISO', '2': 'SDA_I'}),
    ('U1', 'INA226AIDGSR', ('Package_SO', 'VSSOP-10_3x3mm_P0.5mm'), 'C49851', (45.0, 14.0), 0, {
        '1': 'GNDISO', '2': 'GNDISO', '3': '', '4': 'SDA_I', '5': 'SCL_I', '6': 'VISO',
        '7': 'GNDISO', '8': 'GNDISO', '9': 'INN_F', '10': 'INP_F'}),
    ('C7', '100nF', C0603, 'C14663', (42.0, 18.5), 0, {'1': 'VISO', '2': 'GNDISO'}),
    ('R2', '10R', R0603, 'C22859', (50.0, 12.5), 0, {'1': 'INP_F', '2': 'TUBE_IN'}),
    ('R3', '10R', R0603, 'C22859', (50.0, 15.5), 0, {'1': 'INN_F', '2': 'GNDISO'}),
    ('C8', '100nF', C0603, 'C14663', (47.0, 18.5), 0, {'1': 'INP_F', '2': 'INN_F'}),
    ('R1', '0.1R 1% 1206 1W', (R, 'R_1206_3216Metric'), 'C7467250', (49.5, 30.0), 0, {'1': 'TUBE_IN', '2': 'GNDISO'}),
    ('D1', 'SMAJ5.0CA', ('Diode_SMD', 'D_SMA'), 'C140894', (44.0, 29.0), 90, {'1': 'TUBE_IN', '2': 'GNDISO'}),
    ('GDT1', 'GDT 200V 1812 (Brightking 4532-201-LF)', (R, 'R_1812_4532Metric'), 'C522630', (50.0, 23.5), 90, {'1': 'TUBE_IN', '2': 'EARTH'}),
    ('J3', 'Tube return', ('TerminalBlock_Phoenix', 'TerminalBlock_Phoenix_MKDS-1,5-3-5.08_1x03_P5.08mm_Horizontal'),
     '', (62.0, 30.0), 90, {'1': 'TUBE_IN', '2': 'GNDISO', '3': 'EARTH'}),
]

HOLES = [(3.5, 3.5), (3.5, 38.5), (64.5, 3.5), (64.5, 38.5)]

PRIMARY_NETS = {'GND', '+3V3', '+5V', 'SCL_P', 'SDA_P'}
