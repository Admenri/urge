# Copyright 2024 Admenri.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

def write_dump(src_filename, dst_filename, data_type, len_type, data_symbol, add_null_term):
    try:
        with open(src_filename, 'rb') as src_file:
            src_data = src_file.read()

        if add_null_term:
            src_data += b'\x00'

        with open(dst_filename, 'w') as dst_file:
            dst_file.write(f'extern const {data_type} {data_symbol}[] = {{\n')

            byte_columns = 0x10
            column_ind = 0
            byte_count = 0

            for byte in src_data:
                dst_file.write(' ')
                if column_ind == 0:
                    dst_file.write(' ')

                dst_file.write(f'0x{byte:02x},')
                column_ind += 1

                if column_ind == byte_columns:
                    dst_file.write('\n')
                    column_ind = 0

                byte_count += 1

            if add_null_term:
                byte_count -= 1

            dst_file.write('\n};\n')
            dst_file.write(f'extern const {len_type} {data_symbol}_len = {byte_count};\n')

    except IOError as e:
        print(f'Failed to open file: {e}')

def get_named_option(args, opt_name, def_value):
    value = def_value
    try:
        index = args.index(opt_name)
        if index + 1 < len(args):
            value = args[index + 1]
    except ValueError:
        pass
    return value

def usage(argv0):
    print(f'Usage: {argv0} file [options]')
    print('Options:')
    print('  -o [filename]     Override default output filename.')
    print('  --symbol [sym]    Override default data C symbol.')
    print('  --null-terminated Add null byte to data (not reflected in "len").')
    print('  --string          Use char for data array. Implies "--null-terminated".')
    print('  --help            Yo dawg.')

def main():
    if len(sys.argv) < 2:
        usage(sys.argv[0])
        return 0

    input_file = sys.argv[1]
    rest_args = sys.argv[2:]

    if '--help' in rest_args:
        usage(sys.argv[0])
        return 0

    null_term = False
    string_data = False

    if '--null-terminated' in rest_args:
        null_term = True

    if '--string' in rest_args:
        string_data = True
        null_term = True

    out_symbol = os.path.basename(input_file)
    pos = out_symbol.rfind('.')
    if pos != -1:
        out_symbol = out_symbol[:pos] + '_' + out_symbol[pos+1:]

    output_file = get_named_option(rest_args, '-o', input_file + '.xxd')
    out_symbol = get_named_option(rest_args, '--symbol', out_symbol)

    data_type = 'char' if string_data else 'unsigned char'

    target_dir = os.path.dirname(output_file)
    if not os.path.exists(target_dir) and target_dir != '':
      os.makedirs(target_dir)

    write_dump(input_file, output_file, data_type, 'unsigned int', out_symbol, null_term)

if __name__ == '__main__':
    sys.exit(main())
