import csv
import sys


def print_dependencies(file, which='build dependencies'):
    with open(file, newline='') as csv_file:
        reader = csv.DictReader(csv_file, delimiter=',', quotechar='"')
        for row in reader:
            for item in row[which].split(';'):
                if len(item) > 0:
                    print(item)


print_dependencies(sys.argv[1], sys.argv[2])
