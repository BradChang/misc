This example of an error correcting code is from Claude Shannon's
1948 paper, A Mathematical Theory of Communication, section 17.

The encoding processes takes every 4 bits and makes 7 bits from them.
The decoding process takes every 7 bits and produces 4 bits from them.
The decoding can correct for 1 erroneous bit in each group of 7.

    % echo "hello, world!" > original
    % ./ecc -i original -o encoded
    % ./ecc -n -i encoded -o noisy
    % ./ecc -d -i noisy -o decoded
    % diff original decoded
    %     

