outfile=paulstretch
rm -f $outfile

# Compile C source files with gcc
gcc -c \
-I./include \
-I/opt/homebrew/include \
*.c \
example/*.c

gcc \
*.o \
-L/opt/homebrew/lib -lfftw3f \
-o $outfile

# Clean up object files if desired
rm -f *.o
