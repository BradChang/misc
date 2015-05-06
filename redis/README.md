This is an example of using the hiredis C binding for Redis.
It reads UDP packets and then uses their payloads in redis
commands. The commands are read at startup from cmds.conf.
The commands in that file can use %s for the UDP payload.
