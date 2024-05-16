outfile=paulstretch
rm -f $outfile

# Compile C source files with gcc
gcc -c \
-I./contrib \
contrib/*.c 
# gcc -c \
# -I./contrib/audiofile \
# -I./contrib/audiofile/alac \
# contrib/audiofile/*.c \
# contrib/audiofile/alac/*.c

# Compile C++ source files with g++
g++ -std=c++14 -DHAVE_UNISTD_H -DKISSFFT \
-c \
-I/opt/homebrew/include \
-I./contrib \
-I./Input \
./Input/*.cpp \
*.cpp

# Link all object files together with g++
g++ -std=c++14 \
*.o \
-L/opt/homebrew/lib -lgtest -lgtest_main -lFLAC -lz -lpthread -lmad \
-o $outfile

# # Clean up object files if desired
rm -f *.o
