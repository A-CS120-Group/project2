with open("INPUT.bin", "rb") as file:
    a = file.read()
    input_file = str(a.hex())

with open("OUTPUT.bin", "rb") as file:
    a = file.read()
    output_file = str(a.hex())

print(input_file)
print(output_file)
if len(input_file) != len(output_file):
    print(f"Not the same length: {len(input_file)} and {len(output_file)}")
    exit(0)

length = len(input_file)
correct = 0
for i in range(length):
    if input_file[i] == output_file[i]:
        correct += 1
    else:
        print(f"Incorrect: {i}-th hex, Should be {bin(int(input_file[i], 16))}, actually {bin(int(output_file[i], 16))}")

print(f"Correction rate: {correct * 100 / length}")
