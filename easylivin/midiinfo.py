#!/usr/bin/python3
from mido import MidiFile
import sys


def int_to_vlq(value):
    """Convert integer delta time into VLQ bytes (Variable Length Quantity)."""
    bytes_ = []
    while True:
        bytes_.insert(0, value & 0x7F)  # Get lower 7 bits
        if value < 128:
            break
        value >>= 7

    # Set the continuation bit on all but the last byte
    for i in range(len(bytes_) - 1):
        bytes_[i] |= 0x80

    return bytes_


def note_number_to_name(note_number):
    note_names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    octave = note_number // 12 - 1
    note_name = note_names[note_number % 12]
    return f"{note_name}{octave}"

# Load your file (replace with your actual file path)
# midi = MidiFile('bassline1.MID')
midi = MidiFile(sys.argv[1])
# midi.print_tracks()
note_sequence = []

for track in midi.tracks:
    print(f'\nTrack contains {len( track )} events.\n')
    for msg in track:
        bits = ' '.join(format(byte, '08b') for byte in msg.bytes())
        
        vlq_bytes = int_to_vlq(msg.time)
        vlq_bits = ' '.join(format(byte, '08b') for byte in vlq_bytes)

        print(f'Raw Msg: {msg} -> {bits}, VLQ DeltaT: {vlq_bits}.')

        if msg.type == 'note_on' and msg.velocity > 0:
            note_name = note_number_to_name(msg.note)
            note_sequence.append(f'{note_name}')

        print()


print("Note sequence:", note_sequence)