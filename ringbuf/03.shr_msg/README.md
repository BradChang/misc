Shared message ring

See the 02.shr/README.md first.

The difference with this ring is that it supports message oriented init mode
`SHR_INIT_MESSAGES`. In this mode each write is considered a message. Each read
consumes one message if the given buffer can accomodate it, otherwise an error
is returned.
