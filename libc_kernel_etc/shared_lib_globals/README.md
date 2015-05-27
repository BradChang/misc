This just demonstrates that shared library globals 
are only global within their process. In other words
two processes that both dynamically link the same
shared library at the same time do not share the global.
