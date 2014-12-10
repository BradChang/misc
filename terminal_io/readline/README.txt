Two usage examples of readline(3)

The readline library supports interactive editing, history recall,
command tab-completion, editing modes and more. It is helpful for
shell-style interactive programs. 

Run "info readline" to get the full documention.

--------------------------------------------------------------------------------

readline.c: 

The most basic usage of readline.

--------------------------------------------------------------------------------

select-readline.c: 

The point of this example is to show how to avoid blocking in readline. You need
to avoid blocking if you have input that may occur on another descriptor. 
This example uses the "alternate API" of readline.

If you run ./select-readline you have ten seconds to interact with it, use the
history feature, or type quit. If you're still at the prompt in ten seconds,
the program exits.  Behind the scenes, our program had forked and closed a pipe
after those ten seconds. We used select to react immediately to that descriptor
closing, rather than blocking in readline.  

