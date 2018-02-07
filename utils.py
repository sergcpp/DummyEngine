
import os
import sys

for arg in sys.argv:
    if arg == "format":
        os.system('"C:\proj\occdemo\src\libs\AStyle.exe --style=java *.h *.cpp -r -n"')
