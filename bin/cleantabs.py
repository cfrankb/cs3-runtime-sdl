import glob

def main():
    paths = ['src/*.cpp', "src/**/*.cpp", 'src/*.h', "src/**/*.h"]
    deps_blocks = ["all: $(TARGET)"]
    deps = []

    for pattern in paths:
        for f in glob.glob(pattern):
            with open(f) as s:
                data = s.read()
            if '\t' in data:
                with open(f, 'w') as t:
                    data = data.replace('\t', '    ')
                    t.write(data)
                print(f)

main()