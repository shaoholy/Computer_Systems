#!/bin/sh
##
# This script tests the arithmetic logic unit (ALU)
# word-oriented arithmetic and logic operations

# directory of this script
dir="$(dirname $0)"

# include word definitions
source "$dir/worddefs"

# include unit test functions
source "$dir/unittest"

##
# calls 'word' command on same path as this script
word() { echo $($dir/word $*); }

##
# calls 'alu' command on same path as this script
alu() { echo $($dir/alu $*); }

##
# test basic word functions
test_word() {
    # test conversion from hex to word
    local w1=$(word "0xF0A6")
    local w2="1111111111111100" # -4
    local w3="0000000000000000" # 0
    local w4="1111000010100110" # -4
    # w4="0000000000000010" # 2
    # w5="0000000000000110" # 6
    assertWordEqual "$w1" "1111000010100110"

    # check that initial bits are set
    assertEqual $(word $w1 0) 0
    assertEqual $(word $w1 7) 1
    assertEqual $(word $w1 8) 0
    assertEqual $(word $w1 $wordtopbit) 1 # 5
    
    # reverse those bits    
    w1=$(word $w1 0 1)
    w1=$(word $w1 7 0)
    w1=$(word $w1 8 1)
    w1=$(word $w1 $wordtopbit 0)

    # check that modified bits are set
    assertEqual $(word $w1 0) 1
    assertEqual $(word $w1 7) 0
    assertEqual $(word $w1 8) 1
    assertEqual $(word $w1 $wordtopbit) 0

    # check comparison
    assertEqual $(alu testlt $w2) 1 # 10
    assertEqual $(alu testlt $w3) 0
    assertEqual $(alu testge $w2) 0
    assertEqual $(alu testge $w3) 1
    assertEqual $(alu testeq $w3) 1
    assertEqual $(alu testeq $w4) 0 # 15

    # check shift

    # test ash
    assertWordEqual "$(alu ash $w2 1)" "1111111111111000"
    assertWordEqual "$(alu ash $w2 0)" "1111111111111100"
    assertWordEqual "$(alu ash $w2 -1)" "1111111111111110"
    assertWordEqual "$(alu ash $w2 33)" "1000000000000000"
    assertWordEqual "$(alu ash $w2 -33)" "1111111111111111" #20

    # test csh
    assertWordEqual "$(alu csh $w2 1)" "1111111111111001"
    assertWordEqual "$(alu csh $w2 0)" "1111111111111100"
    assertWordEqual "$(alu csh $w2 -1)" "0111111111111110"
    assertWordEqual "$(alu csh $w2 32)" "1111111111111100"
    assertWordEqual "$(alu csh $w2 33)" "1111111111111001" #25
    assertWordEqual "$(alu csh $w2 -33)" "0111111111111110"

    # test lsh
    assertWordEqual "$(alu lsh $w2 1)" "1111111111111000"
    assertWordEqual "$(alu lsh $w2 0)" "1111111111111100"
    assertWordEqual "$(alu lsh $w2 -2)" "0011111111111111"
    assertWordEqual "$(alu lsh $w2 32)" "0000000000000000" # 30
    assertWordEqual "$(alu lsh $w2 33)" "0000000000000000" 
    assertWordEqual "$(alu lsh $w2 -33)" "0000000000000000"

    # check logic

    # check and()
    assertWordEqual "$(alu and $w2 $w3)" "0000000000000000"
    assertWordEqual "$(alu and $w4 $w2)" "1111000010100100"

    # check or()
    assertWordEqual "$(alu or $w2 $w3)" "1111111111111100" # 35
    assertWordEqual "$(alu or $w4 $w2)" "1111111111111110"

    # check xor()
    assertWordEqual "$(alu xor $w2 $w3)" "1111111111111100" 
    assertWordEqual "$(alu xor $w4 $w2)" "0000111101011010"

    # check not()
    assertWordEqual "$(alu not $w2)" "0000000000000011" 
    assertWordEqual "$(alu not $w4)" "0000111101011001" # 40
}

## 
# test math functions (add, sub, mul, div, negative)
test_math() {
    w1="1111111111111111" # -1
    w2="1111111111111100" # -4
    w3="0000000000000000" # 0
    w4="0000000000000010" # 2
    w5="0000000000000110" # 6
    w6="0000000000000101" # 5

    # test top bit of word
    assertTrue "$(word $w2 $wordtopbit)"

    # test difference of w1 and w2
    assertWordEqual "$(alu sub $w1 $w2)" "0000000000000011"

    # test minus of w1
    assertWordEqual "$(alu minus $w1)" "0000000000000001"

    # test minus of w2
    assertWordEqual "$(alu minus $w2)" "0000000000000100"

    # test minus of w3
    assertWordEqual "$(alu minus $w3)" "0000000000000000"

    # test sum of w1 and w2
    assertWordEqual "$(alu add $w1 $w2)" "1111111111111011"

    # test sum of w1 and w3
    assertWordEqual "$(alu add $w3 $w1)" "1111111111111111"

    # test mul of w1 and w2
    assertWordEqual "$(alu mul $w1 $w2)" "0000000000000100"

    # test mul of w2 and w1
    assertWordEqual "$(alu mul $w2 $w1)" "0000000000000100"

    # test mul of w1 and w3
    assertWordEqual "$(alu mul $w1 $w3)" "0000000000000000"

    # test mul of w1 and w4
    assertWordEqual "$(alu mul $w1 $w4)" "1111111111111110"

    # test mul of w5 and w4
    assertWordEqual "$(alu mul $w5 $w4)" "0000000000001100"

    # test div of w5 and w4 (pos div pos)
    assertWordEqual "$(alu div $w5 $w4)" "0000000000000011"

    # test div of w2 and w4  (div div pos)
    assertWordEqual "$(alu div $w2 $w4)" "1111111111111110"

    # test div of w4 and w3 ( divided by 0)
    assertWordEqual "$(alu div $w4 $w3)" "0111111111111111"

    # test div of w2 and w3 ( divided by 0)
    assertWordEqual "$(alu div $w2 $w3)" "1000000000000000"

    # test div of w3 and w4 ( 0 to divide )
    assertWordEqual "$(alu div $w3 $w4)" "0000000000000000"

    # test div of w3 and w2 ( 0 to divide )
    assertWordEqual "$(alu div $w3 $w2)" "0000000000000000"

    # test div of w6 and w4 (pos to pos with rmdr)
    assertWordEqual "$(alu div $w6 $w4)" "0000000000000010"

    # test rmdr of w6 and w4
    assertWordEqual "$(alu rmdr $w6 $w4)" "0000000000000001"

    # test rmdr of w6 and w2
    assertWordEqual "$(alu rmdr $w6 $w2)" "0000000000000001"

}

test_word
test_math

# show unit test summary
unitTestSummary
