Entropy encoding

This example of entropy encoding is from Claude Shannon's
1948 paper, A Mathematical Theory of Communication, section 9.

There are many ways to compress or encode information. This method
uses the probability of each symbol to arithmetically produce a 
binary code that gives shorter encodings to common symbols. 

Method:

 Suppose you have N symbols.
 Sort them in order of decreasing probability p(i)
 For each symbol i, 
  m(i) is how many binary digits will encode it
  P(i) is the cumulative probability of the symbols more likely than itself
  m(i) is the integer satisfying: log2(1/p(i)) <= m(i) < 1 + log2(1/p(i)) 
 Encode symbol i as the binary expansion of P(i) to m(i) digits.

Since P(i) is a fractional probability (like 1/2), its binary expansion
is like .1 since 2^-1 = 1/2. (That's a binary point not a decimal point). 
As another example if a symbol had probability 1/3 and m(i)=2, its binary 
expansion would be .01 (truncated to 2 digits because m(i)=2).

An example

This example is from Shannon's paper (section 11, Examples).
Suppose there are four symbols A, B, C, D with probabilities 
(1/2, 1/4, 1/8, 1/8). 

         A           B            C            D
p(i)    1/2         1/4          1/8          1/8
m(i)     1           2            3            3
P(i)     0          1/2       1/2+1/4     1/2+1/4+1/8
code     0          10           110          111

Using this encoding we use on average 7/4 bits per symbol. This
7/4 is the entropy per symbol, -sum(p(i) * log2(p(i))). 

The relative entropy is the ratio the entropy of a source to the 
maximum entropy it could have. In this example the entropy of 
the source is 7/4 bits per symbol, and the maximum entropy would
occur if the four symbols were equally likely (p(i)=1/4 for all
symbols), giving a maximum entropy of 2 bits per byte. Therefore
the relative entropy is (7/4) / (2) = 7/8 bits per symbol. This
encoding of the symbols gives us a 7/8 compression ratio over 
simply differentiating the symbols as if they were equally likely.

