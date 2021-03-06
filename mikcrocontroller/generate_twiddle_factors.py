import numpy as np

outfilename = 'Inc/twiddlefactors.h'
# keep both following lines in sync!
size = 16384
bits = 14


def gen_table():
    entries = size // 2
    table = 'const complex twiddleFactors[%s] = {\n' % str(entries)
    rows = int(pow(2, bits-2-1))  # in each row should be 4 entries, -1 for just writing half of the the size
    _size = float(size)
    for row in range(rows):
        table += '    '
        for col in range(4):
            table += '{'
            i = row * 4 + col
            argument = i * 2 * np.pi/_size
            re = np.cos(argument)
            im = np.sin(argument)
            table += '{}, {}'.format(re, im)
            table += '}, '
        table += '\n'
    table = table[:-3]  # remove last \n, colon and whitespace.
    table += '\n};\n'
    return table


def main():
    if int(pow(2, bits)) != size:
        raise ValueError("size and bits settings doesn't match")

    with open(outfilename, 'w') as f:
        f.write('/* Generated by "generate_twiddle_factors.py". Consider changing the script for modifications. */\n')
        f.write('#ifndef TWIDDLE_FACTORS_H_\n')
        f.write('#define TWIDDLE_FACTORS_H_\n\n')

        f.write('#include "fft.h"\n')
        f.write('#define TWIDDLE_FACTOR_TABLE_SIZE {}\n\n'.format(size))
        f.write('#if (MAX_FFT_SIZE > TWIDDLE_FACTOR_TABLE_SIZE)\n')
        f.write('#error "The twiddlefactortable must at least have the resolution of the max fft size"\n')
        f.write('#endif\n\n')
        f.write(gen_table() + '\n')

        f.write('#endif\n')


if __name__ == '__main__':
    main()
