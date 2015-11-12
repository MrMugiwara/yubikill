# yubikill
Shutsdown your computer whenever your yubikey gets disconnected. 
For those cases when the adversary is storming your house and you don't wanna leave your encryption keys lying around in memory.

## Usage
1. Build the program with ```gcc -lusb main.c -o yubikill```
2. Execute `./yubikill` with your yubikey plugged in.

It will periodically check if the yubikey is still plugged in. If it gets removed, yubikill will initiate a poweroff or hibernation of the system.

Note: You will have to set `poweroff` and/or `pm-hibernate` to not prompt a passwd when used with sudo. 
You can simply add the following lines to `/etc/sudoers`:

```
youruser ALL=NOPASSWD: /usr/bin/pm-hibernate
youruser ALL=NOPASSWD: /usr/bin/poweroff
```

## Configuration
There are currently 4 options available for yubikill:

* `-p`  Poweroff the system (this is the default behaviour)
* `-h`  Hibernate the system
* `-d`  Delay in seconds between yubikey removal and poweroff/hibernation
* `-i`  Display a warning message on i3-nagbar when yubikey is removed

Alternatively these values can be set in a config file. You can find an [example config file here](../master/.yubikill).
Default config file location is `~/.yubikill`. You can override this by defining a custom location via the `-c` argument.
