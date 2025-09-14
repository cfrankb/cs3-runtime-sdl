source = "external/libxmp/src/control.c"
dest = source

lines = []
with open(source, "r") as sfile:
    data = sfile.read()
    if "\r\n" in data:
        lines = data.split("\r\n")
    elif "\n" in data:
        lines = data.split("\n")
    elif "\r" in data:
        lines = data.split("\r")

if lines:
    newlines = ["// " + line if line[0:4] == "asm(" else line for line in lines]
    with open(dest, "w") as tfile:
        tfile.write("\n".join(newlines))
