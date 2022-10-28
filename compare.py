with open("INPUT.bin", "rb") as file:
    a = file.read()
    input_file = str(a.hex())

with open("OUTPUT.bin", "rb") as file:
    a = file.read()
    output_file = str(a.hex())

assert len(input_file) == len(output_file), "Not the same length"

length = len(input_file)
correct = 0
for i in range(length):
    if input_file[i] == output_file[i]:
        correct += 1
    else:
        print(f"Incorrect: {i}-th hex")

print(f"Correction rate: {correct * 100 / length}")
