outfile=paulstretch
rm -f $outfile

# Compile C source files with gcc
gcc -c \
-I./contrib \
contrib/*.c

# Compile C++ source files with g++
g++ -std=c++14 -DKISSFFT \
-c \
-I/opt/homebrew/include \
-I./contrib \
-I./Input \
./Input/*.cpp \
*.cpp

# Link all object files together with g++
g++ -std=c++14 \
*.o \
-L/opt/homebrew/lib -lz -lpthread -lmad \
-o $outfile

# Clean up object files if desired
rm -f *.o
