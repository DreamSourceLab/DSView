##
## This file is part of the libsigrokdecode project.
##
## Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
##

status_bytes = {
    # Channel voice messages
    0x80: 'note off',
    0x90: 'note on', # However, velocity = 0 means "note off".
    0xa0: 'polyphonic key pressure / aftertouch',
    0xb0: 'control change',
    0xc0: 'program change',
    0xd0: 'channel pressure / aftertouch',
    0xe0: 'pitch bend change',

    # Channel mode messages
    # 0xb0: 'select channel mode', # Note: Same as 'control change'.

    # System exclusive messages
    0xf0: 'system exclusive (SysEx)',

    # System common messages
    0xf1: 'MIDI time code quarter frame',
    0xf2: 'song position pointer',
    0xf3: 'song select',
    0xf4: 'undefined',
    0xf5: 'undefined',
    0xf6: 'tune request',
    0xf7: 'end of system exclusive (EOX)',

    # System real time messages
    0xf8: 'timing clock',
    0xf9: 'undefined',
    0xfa: 'start',
    0xfb: 'continue',
    0xfc: 'stop',
    0xfd: 'undefined',
    0xfe: 'active sensing',
    0xff: 'system reset',
}

# Universal system exclusive (SysEx) messages, non-realtime (0x7e)
universal_sysex_nonrealtime = {
    (0x00, None): 'unused',
    (0x01, None): 'sample dump header',
    (0x02, None): 'sample data packet',
    (0x03, None): 'sample dump request',

    (0x04, None): 'MIDI time code',
    (0x04, 0x00): 'special',
    (0x04, 0x01): 'punch in points',
    (0x04, 0x02): 'punch out points',
    (0x04, 0x03): 'delete punch in point',
    (0x04, 0x04): 'delete punch out point',
    (0x04, 0x05): 'event start point',
    (0x04, 0x06): 'event stop point',
    (0x04, 0x07): 'event start points with additional info',
    (0x04, 0x08): 'event stop points with additional info',
    (0x04, 0x09): 'delete event start point',
    (0x04, 0x0a): 'delete event stop point',
    (0x04, 0x0b): 'cue points',
    (0x04, 0x0c): 'cue points with additional info',
    (0x04, 0x0d): 'delete cue point',
    (0x04, 0x0e): 'event name in additional info',

    (0x05, None): 'sample dump extensions',
    (0x05, 0x01): 'multiple loop points',
    (0x05, 0x02): 'loop points request',

    (0x06, None): 'general information',
    (0x06, 0x01): 'identity request',
    (0x06, 0x02): 'identity reply',

    (0x07, None): 'file dump',
    (0x07, 0x01): 'header',
    (0x07, 0x02): 'data packet',
    (0x07, 0x03): 'request',

    (0x08, None): 'MIDI tuning standard',
    (0x08, 0x00): 'bulk dump request',
    (0x08, 0x01): 'bulk dump reply',

    (0x09, None): 'general MIDI',
    (0x09, 0x01): 'general MIDI system on',
    (0x09, 0x02): 'general MIDI system off',

    (0x7b, None): 'end of file',
    (0x7c, None): 'wait',
    (0x7d, None): 'cancel',
    (0x7e, None): 'nak',
    (0x7f, None): 'ack',
}

# Universal system exclusive (SysEx) messages, realtime (0x7f)
universal_sysex_realtime = {
    (0x00, None): 'unused',

    (0x01, None): 'MIDI time code',
    (0x01, 0x01): 'full message',
    (0x01, 0x02): 'user bits',

    (0x02, None): 'MIDI show control',
    (0x02, 0x00): 'MSC extensions',
    # (0x02, TODO): 'TODO', # 0x01 - 0x7f: MSC commands.

    (0x03, None): 'notation information',
    (0x03, 0x01): 'bar number',
    (0x03, 0x02): 'time signature (immediate)',
    (0x03, 0x42): 'time signature (delayed)',

    (0x04, None): 'device control',
    (0x04, 0x01): 'master volume',
    (0x04, 0x02): 'master balance',

    (0x05, None): 'real time MTC cueing',
    (0x05, 0x00): 'special',
    (0x05, 0x01): 'punch in points',
    (0x05, 0x02): 'punch out points',
    (0x05, 0x03): 'reserved',
    (0x05, 0x04): 'reserved',
    (0x05, 0x05): 'event start points',
    (0x05, 0x06): 'event stop points',
    (0x05, 0x07): 'event start points with additional info',
    (0x05, 0x08): 'event stop points with additional info',
    (0x05, 0x09): 'reserved',
    (0x05, 0x0a): 'reserved',
    (0x05, 0x0b): 'cue points',
    (0x05, 0x0c): 'cue points with additional info',
    (0x05, 0x0d): 'reserved',
    (0x05, 0x0e): 'event name in additional info',

    (0x06, None): 'MIDI machine control commands',
    # (0x06, TODO): 'TODO', # 0x00 - 0x7f: MMC commands.

    (0x07, None): 'MIDI machine control responses',
    # (0x07, TODO): 'TODO', # 0x00 - 0x7f: MMC commands.

    (0x08, None): 'MIDI tuning standard',
    (0x85, 0x02): 'note change',
}

# Note: Not all IDs are used/listed, i.e. there are some "holes".
sysex_manufacturer_ids = {
    # American group
    (0x01): 'Sequential',
    (0x02): 'IDP',
    (0x03): 'Voyetra/Octave-Plateau',
    (0x04): 'Moog',
    (0x05): 'Passport Designs',
    (0x06): 'Lexicon',
    (0x07): 'Kurzweil',
    (0x08): 'Fender',
    (0x09): 'Gulbransen',
    (0x0a): 'AKG Acoustics',
    (0x0b): 'Voyce Music',
    (0x0c): 'Waveframe Corp',
    (0x0d): 'ADA Signal Processors',
    (0x0e): 'Garfield Electronics',
    (0x0f): 'Ensoniq',
    (0x10): 'Oberheim',
    (0x11): 'Apple Computer',
    (0x12): 'Grey Matter Response',
    (0x13): 'Digidesign',
    (0x14): 'Palm Tree Instruments',
    (0x15): 'JLCooper Electronics',
    (0x16): 'Lowrey',
    (0x17): 'Adams-Smith',
    (0x18): 'Emu Systems',
    (0x19): 'Harmony Systems',
    (0x1a): 'ART',
    (0x1b): 'Baldwin',
    (0x1c): 'Eventide',
    (0x1d): 'Inventronics',
    (0x1f): 'Clarity',

    (0x00, 0x00, 0x01): 'Time Warner Interactive',
    (0x00, 0x00, 0x07): 'Digital Music Corp.',
    (0x00, 0x00, 0x08): 'IOTA Systems',
    (0x00, 0x00, 0x09): 'New England Digital',
    (0x00, 0x00, 0x0a): 'Artisyn',
    (0x00, 0x00, 0x0b): 'IVL Technologies',
    (0x00, 0x00, 0x0c): 'Southern Music Systems',
    (0x00, 0x00, 0x0d): 'Lake Butler Sound Company',
    (0x00, 0x00, 0x0e): 'Alesis',
    (0x00, 0x00, 0x10): 'DOD Electronics',
    (0x00, 0x00, 0x11): 'Studer-Editech',
    (0x00, 0x00, 0x14): 'Perfect Fretworks',
    (0x00, 0x00, 0x15): 'KAT',
    (0x00, 0x00, 0x16): 'Opcode',
    (0x00, 0x00, 0x17): 'Rane Corp.',
    (0x00, 0x00, 0x18): 'Anadi Inc.',
    (0x00, 0x00, 0x19): 'KMX',
    (0x00, 0x00, 0x1a): 'Allen & Heath Brenell',
    (0x00, 0x00, 0x1b): 'Peavy Electronics',
    (0x00, 0x00, 0x1c): '360 Systems',
    (0x00, 0x00, 0x1d): 'Spectrum Design and Development',
    (0x00, 0x00, 0x1e): 'Marquis Music',
    (0x00, 0x00, 0x1f): 'Zeta Systems',

    (0x00, 0x00, 0x20): 'Axxes',
    (0x00, 0x00, 0x21): 'Orban',
    (0x00, 0x00, 0x24): 'KTI',
    (0x00, 0x00, 0x25): 'Breakaway Technologies',
    (0x00, 0x00, 0x26): 'CAE',
    (0x00, 0x00, 0x29): 'Rocktron Corp.',
    (0x00, 0x00, 0x2a): 'PianoDisc',
    (0x00, 0x00, 0x2b): 'Cannon Research Group',
    (0x00, 0x00, 0x2d): 'Rogers Instrument Corp.',
    (0x00, 0x00, 0x2e): 'Blue Sky Logic',
    (0x00, 0x00, 0x2f): 'Encore Electronics',

    (0x00, 0x00, 0x30): 'Uptown',
    (0x00, 0x00, 0x31): 'Voce',
    (0x00, 0x00, 0x32): 'CTI Audio, Inc. (Music. Intel Dev.)',
    (0x00, 0x00, 0x33): 'S&S Research',
    (0x00, 0x00, 0x34): 'Broderbund Software, Inc.',
    (0x00, 0x00, 0x35): 'Allen Organ Co.',
    (0x00, 0x00, 0x37): 'Music Quest',
    (0x00, 0x00, 0x38): 'APHEX',
    (0x00, 0x00, 0x39): 'Gallien Krueger',
    (0x00, 0x00, 0x3a): 'IBM',
    (0x00, 0x00, 0x3c): 'Hotz Instruments Technologies',
    (0x00, 0x00, 0x3d): 'ETA Lighting',
    (0x00, 0x00, 0x3e): 'NSI Corporation',
    (0x00, 0x00, 0x3f): 'Ad Lib, Inc.',

    (0x00, 0x00, 0x40): 'Richmond Sound Design',
    (0x00, 0x00, 0x41): 'Microsoft',
    (0x00, 0x00, 0x42): 'The Software Toolworks',
    (0x00, 0x00, 0x43): 'Niche/RJMG',
    (0x00, 0x00, 0x44): 'Intone',
    (0x00, 0x00, 0x47): 'GT Electronics / Groove Tubes',
    (0x00, 0x00, 0x49): 'Timeline Vista',
    (0x00, 0x00, 0x4a): 'Mesa Boogie',
    (0x00, 0x00, 0x4c): 'Sequoia Development',
    (0x00, 0x00, 0x4d): 'Studio Electronics',
    (0x00, 0x00, 0x4e): 'Euphonix',
    (0x00, 0x00, 0x4f): 'InterMIDI, Inc.',

    (0x00, 0x00, 0x50): 'MIDI Solutions',
    (0x00, 0x00, 0x51): '3DO Company',
    (0x00, 0x00, 0x52): 'Lightwave Research',
    (0x00, 0x00, 0x53): 'Micro-W',
    (0x00, 0x00, 0x54): 'Spectral Synthesis',
    (0x00, 0x00, 0x55): 'Lone Wolf',
    (0x00, 0x00, 0x56): 'Studio Technologies',
    (0x00, 0x00, 0x57): 'Peterson EMP',
    (0x00, 0x00, 0x58): 'Atari',
    (0x00, 0x00, 0x59): 'Marion Systems',
    (0x00, 0x00, 0x5a): 'Design Event',
    (0x00, 0x00, 0x5b): 'Winjammer Software',
    (0x00, 0x00, 0x5c): 'AT&T Bell Labs',
    (0x00, 0x00, 0x5e): 'Symetrix',
    (0x00, 0x00, 0x5f): 'MIDI the World',

    (0x00, 0x00, 0x60): 'Desper Products',
    (0x00, 0x00, 0x61): 'Micros\'N MIDI',
    (0x00, 0x00, 0x62): 'Accordians Intl',
    (0x00, 0x00, 0x63): 'EuPhonics',
    (0x00, 0x00, 0x64): 'Musonix',
    (0x00, 0x00, 0x65): 'Turtle Beach Systems',
    (0x00, 0x00, 0x66): 'Mackie Designs',
    (0x00, 0x00, 0x67): 'Compuserve',
    (0x00, 0x00, 0x68): 'BES Technologies',
    (0x00, 0x00, 0x69): 'QRS Music Rolls',
    (0x00, 0x00, 0x6a): 'P G Music',
    (0x00, 0x00, 0x6b): 'Sierra Semiconductor',
    (0x00, 0x00, 0x6c): 'EpiGraf Audio Visual',
    (0x00, 0x00, 0x6d): 'Electronics Deiversified',
    (0x00, 0x00, 0x6e): 'Tune 1000',
    (0x00, 0x00, 0x6f): 'Advanced Micro Devices',

    (0x00, 0x00, 0x70): 'Mediamation',
    (0x00, 0x00, 0x71): 'Sabine Music',
    (0x00, 0x00, 0x72): 'Woog Labs',
    (0x00, 0x00, 0x73): 'Micropolis',
    (0x00, 0x00, 0x74): 'Ta Horng Musical Inst.',
    (0x00, 0x00, 0x75): 'eTek (formerly Forte)',
    (0x00, 0x00, 0x76): 'Electrovoice',
    (0x00, 0x00, 0x77): 'Midisoft',
    (0x00, 0x00, 0x78): 'Q-Sound Labs',
    (0x00, 0x00, 0x79): 'Westrex',
    (0x00, 0x00, 0x7a): 'NVidia',
    (0x00, 0x00, 0x7b): 'ESS Technology',
    (0x00, 0x00, 0x7c): 'MediaTrix Peripherals',
    (0x00, 0x00, 0x7d): 'Brooktree',
    (0x00, 0x00, 0x7e): 'Otari',
    (0x00, 0x00, 0x7f): 'Key Electronics',

    (0x00, 0x01, 0x01): 'Crystalake Multimedia',
    (0x00, 0x01, 0x02): 'Crystal Semiconductor',
    (0x00, 0x01, 0x03): 'Rockwell Semiconductor',

    # European group
    (0x20): 'Passac',
    (0x21): 'SIEL',
    (0x22): 'Synthaxe',
    (0x24): 'Hohner',
    (0x25): 'Twister',
    (0x26): 'Solton',
    (0x27): 'Jellinghaus MS',
    (0x28): 'Southworth Music Systems',
    (0x29): 'PPG',
    (0x2a): 'JEN',
    (0x2b): 'SSL Limited',
    (0x2c): 'Audio Veritrieb',
    (0x2f): 'Elka',

    (0x30): 'Dynacord',
    (0x31): 'Viscount',
    (0x33): 'Clavia Digital Instruments',
    (0x34): 'Audio Architecture',
    (0x35): 'GeneralMusic Corp.',
    (0x39): 'Soundcraft Electronics',
    (0x3b): 'Wersi',
    (0x3c): 'Avab Elektronik Ab',
    (0x3d): 'Digigram',
    (0x3e): 'Waldorf Electronics',
    (0x3f): 'Quasimidi',

    (0x00, 0x20, 0x00): 'Dream',
    (0x00, 0x20, 0x01): 'Strand Lighting',
    (0x00, 0x20, 0x02): 'Amek Systems',
    (0x00, 0x20, 0x04): 'Böhm Electronic',
    (0x00, 0x20, 0x06): 'Trident Audio',
    (0x00, 0x20, 0x07): 'Real World Studio',
    (0x00, 0x20, 0x09): 'Yes Technology',
    (0x00, 0x20, 0x0a): 'Audiomatica',
    (0x00, 0x20, 0x0b): 'Bontempi/Farfisa',
    (0x00, 0x20, 0x0c): 'F.B.T. Elettronica',
    (0x00, 0x20, 0x0d): 'MidiTemp',
    (0x00, 0x20, 0x0e): 'LA Audio (Larking Audio)',
    (0x00, 0x20, 0x0f): 'Zero 88 Lighting Limited',

    (0x00, 0x20, 0x10): 'Micon Audio Electronics GmbH',
    (0x00, 0x20, 0x11): 'Forefront Technology',
    (0x00, 0x20, 0x13): 'Kenton Electronics',
    (0x00, 0x20, 0x15): 'ADB',
    (0x00, 0x20, 0x16): 'Marshall Products',
    (0x00, 0x20, 0x17): 'DDA',
    (0x00, 0x20, 0x18): 'BSS',
    (0x00, 0x20, 0x19): 'MA Lighting Technology',
    (0x00, 0x20, 0x1a): 'Fatar',
    (0x00, 0x20, 0x1b): 'QSC Audio',
    (0x00, 0x20, 0x1c): 'Artisan Classic Organ',
    (0x00, 0x20, 0x1d): 'Orla Spa',
    (0x00, 0x20, 0x1e): 'Pinnacle Audio',
    (0x00, 0x20, 0x1f): 'TC Electronics',

    (0x00, 0x20, 0x20): 'Doepfer Musikelektronik',
    (0x00, 0x20, 0x21): 'Creative Technology Pte',
    (0x00, 0x20, 0x22): 'Minami/Seiyddo',
    (0x00, 0x20, 0x23): 'Goldstar',
    (0x00, 0x20, 0x24): 'Midisoft s.a.s di M. Cima',
    (0x00, 0x20, 0x25): 'Samick',
    (0x00, 0x20, 0x26): 'Penny and Giles',
    (0x00, 0x20, 0x27): 'Acorn Computer',
    (0x00, 0x20, 0x28): 'LSC Electronics',
    (0x00, 0x20, 0x29): 'Novation EMS',
    (0x00, 0x20, 0x2a): 'Samkyung Mechatronics',
    (0x00, 0x20, 0x2b): 'Medeli Electronics',
    (0x00, 0x20, 0x2c): 'Charlie Lab',
    (0x00, 0x20, 0x2d): 'Blue Chip Music Tech',
    (0x00, 0x20, 0x2e): 'BEE OH Corp',

    # Japanese group
    (0x40): 'Kawai',
    (0x41): 'Roland',
    (0x42): 'Korg',
    (0x43): 'Yamaha',
    (0x44): 'Casio',
    (0x46): 'Kamiya Studio',
    (0x47): 'Akai',
    (0x48): 'Japan Victor',
    (0x49): 'Mesosha',
    (0x4a): 'Hoshino Gakki',
    (0x4b): 'Fujitsu Elect',
    (0x4c): 'Sony',
    (0x4d): 'Nisshin Onpa',
    (0x4e): 'TEAC',
    (0x50): 'Matsushita Electric',
    (0x51): 'Fostex',
    (0x52): 'Zoom',
    (0x53): 'Midori Electronics',
    (0x54): 'Matsushita Communication Industrial',
    (0x55): 'Suzuki Musical Inst. Mfg.',
}

control_functions = {
    0x00: 'bank select',
    0x01: 'modulation wheel/lever',
    0x02: 'breath controller',
    # 0x03: undefined
    0x04: 'foot controller',
    0x05: 'portamento time',
    0x06: 'data entry MSB',
    0x07: 'channel volume (formerly main volume)',
    0x08: 'balance',
    # 0x09: undefined
    0x0a: 'pan',
    0x0b: 'expression controller',
    0x0c: 'effect control 1',
    0x0d: 'effect control 2',
    # 0x0e-0x0f: undefined
    0x10: 'general purpose controller 1',
    0x11: 'general purpose controller 2',
    0x12: 'general purpose controller 3',
    0x13: 'general purpose controller 4',
    # 0x14-0x1f: undefined
    # 0x20-0x3f: LSB for values 0x00-0x1f
    0x40: 'damper pedal (sustain)',
    0x41: 'portamento on/off',
    0x42: 'sostenuto',
    0x43: 'soft pedal',
    0x44: 'legato footswitch', # vv: 00-3f = normal, 40-7f = legato
    0x45: 'hold 2',
    0x46: 'sound controller 1 (default: sound variation)',
    0x47: 'sound controller 2 (default: timbre / harmonic intensity)',
    0x48: 'sound controller 3 (default: release time)',
    0x49: 'sound controller 4 (default: attack time)',
    0x4a: 'sound controller 5 (default: brightness)',
    0x4b: 'sound controller 6 (GM2 default: decay time)',
    0x4c: 'sound controller 7 (GM2 default: vibrato rate)',
    0x4d: 'sound controller 8 (GM2 default: vibrato depth)',
    0x4e: 'sound controller 9 (GM2 default: vibrato delay)',
    0x4f: 'sound controller 10',
    0x50: 'general purpose controller 5',
    0x51: 'general purpose controller 6',
    0x52: 'general purpose controller 7',
    0x53: 'general purpose controller 8',
    0x54: 'portamento control',
    # 0x55-0x5a: undefined
    0x5b: 'effects 1 depth (formerly external effects depth)',
    0x5c: 'effects 2 depth (formerly tremolo depth)',
    0x5d: 'effects 3 depth (formerly chorus depth)',
    0x5e: 'effects 4 depth (formerly celeste/detune depth)',
    0x5f: 'effects 5 depth (formerly phaser depth)',
    0x60: 'data increment',
    0x61: 'data decrement',
    0x62: 'non-registered parameter number LSB',
    0x63: 'non-registered parameter number MSB',
    0x64: 'registered parameter number LSB',
    0x65: 'registered parameter number MSB',
    # 0x66-0x77: undefined
    # 0x78-0x7f: reserved for channel mode messages
    0x78: 'all sound off',
    0x79: 'reset all controllers',
    0x7a: 'local control on/off',
    0x7b: 'all notes off',
    0x7c: 'omni mode off', # all notes off
    0x7d: 'omni mode on', # all notes off
    0x7e: 'poly mode off', # mono mode on, all notes off
    0x7f: 'poly mode on', # mono mode off, all notes off
}
