# assignment-2
This repository contains the code for assignment 2

worddefs: word definitions included in other scripts: default: 4-byte (16-bit) bigendian word
word: operations on word

    word                    echos usage message to stderr

    word 0x1A2F             echos the word, or hex or decimal value as a word

    word 0x1A2F 2           echos the specified bit as 0 or 1

    word 0x1A2F 2 1         sets the specified bit

alu: alu operations
    
    alu sub 0xFFFF 0x0001   echos difference of second from first word

unittest: unit test functions included in unit test script

testalu: script that runs unit tests on word and alu functions

Make word, alu, and testalu executable (e.g. chmod +x testalu) or run using bash (bash testalu)

NOTE: Cygwin users must install the 'bc' package to run these scripts
