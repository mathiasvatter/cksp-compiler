import sys

def convert_to_header(input_file, output_file):
    try:
        with open(input_file, 'rb') as infile, open(output_file, 'w') as outfile:
            # Erstellen des Array-Namens aus dem Dateinamen
            array_name = input_file.split('/')[-1].split('.')[0]
            outfile.write(f"unsigned char {array_name}[] = {{\n    ")

            byte_count = 0
            byte = infile.read(1)
            while byte:
                # Schreiben jedes Bytes als Hexadezimalwert
                outfile.write(f"0x{byte.hex()}, ")
                byte_count += 1
                if byte_count % 12 == 0:
                    outfile.write("\n    ")
                byte = infile.read(1)

            outfile.write("\n};\n")
            outfile.write(f"const unsigned int {array_name}_len = {byte_count};\n")
            print(f"File {input_file} converted to {output_file}")
    except IOError as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python convert_to_header.py <input_file> <output_file>")
        sys.exit(1)

    input_file_path = sys.argv[1]
    output_file_path = sys.argv[2]
    convert_to_header(input_file_path, output_file_path)
