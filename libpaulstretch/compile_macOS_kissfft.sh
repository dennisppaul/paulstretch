outfile=paulstretch
rm -f $outfile

# Compile C source files with gcc
gcc -c \
-DKISSFFT \
-I./include \
-I./contrib \
*.c \
./contrib/*.c \
./example/*.c

gcc \
*.o \
-o $outfile

# Clean up object files if desired
rm -f *.o
