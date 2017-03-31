# pia-resolv-fix
Ubuntu 16.04+ systemd daemon enhancing poor DNS performance with Private Internet Access client

What does this do?
------------------

Private Internet Access client writes DNS server entries to resolve.conf, removing the symlink to /etc/resolvconf/resolv.conf 
that is kept dynamically up to date by the system. Sometimes, PIA doesn't close cleanly and fails to restore the symbolic 
link, causing the PIA DNS servers to be used even when the VPN is not in use, leading to poor DNS performance compared to 
using a local DNS server. This happens most often when the system is shutdown, and rarely during normal use.

This small daemon uses the Linux inotify facilities to watch /etc/resolv.conf for changes and monitors the VPN state. The 
daemon effeciently enforces the following invariant:

```
We should use the local DNS configuration at /etc/resolvconf/resolv.conf when PIA is not running.
```

...even when PIA fails to restore the resolv state correctly. 


When should I use this?
-----------------------

```
dnsmasq[8542]: /etc/resolvconf/update.d/libc: Warning: /etc/resolv.conf is not a symbolic link to /run/resolvconf/resolv.conf
```

If you get errors like the above in your syslog while not connected to the VPN, notice a drop in DNS performance, and use 
Private Internet Access on Ubuntu 16.04+, this program might improve your DNS performance by ensuring that PIA DNS servers are 
only used when connected to the VPN, and (presumably) faster local DNS servers (such as the ones provided by your ISP) are 
used when the VPN is not connected.

Demo
----

```
Mar 31 15:58:07 freedom systemd[1]: Starting PIA OpenVPN Resolv Fixup Daemon...
Mar 31 15:58:07 freedom pia-resolv-fix[26698]: Parsed arguments: daemon=1, pid_file=1
Mar 31 15:58:07 freedom pia-resolv-fix[26698]: Check resolv status prompted by inotify
Mar 31 15:58:07 freedom systemd[1]: Started PIA OpenVPN Resolv Fixup Daemon.
Mar 31 15:58:07 freedom pia-resolv-fix[26698]: All good, not a symlink and PIA is running
Mar 31 15:58:07 freedom pia-resolv-fix[26698]: Allocating event buffer of 272 bytes
Mar 31 15:58:07 freedom pia-resolv-fix[26698]: Begun monitoring /etc/resolv.conf for changes
```
PIA disconnect:
```
Mar 31 15:58:26 freedom pia-resolv-fix[26698]: Check resolv status prompted by inotify
Mar 31 15:58:26 freedom pia-resolv-fix[26698]: Failed to stat. Trying to fix because PIA is not running right now...
Mar 31 15:58:26 freedom pia-resolv-fix[26698]: Restoring symlink to /run/resolvconf/resolv.conf
Mar 31 15:58:26 freedom pia-resolv-fix[26698]: Check resolv status prompted by inotify
Mar 31 15:58:26 freedom pia-resolv-fix[26698]: All good, symlink and PIA isn't running
```
PIA connect:
```
Mar 31 15:58:54 freedom pia-resolv-fix[26698]: Check resolv status prompted by inotify
Mar 31 15:58:54 freedom pia-resolv-fix[26698]: Cannot stat, assuming PIA must be writing resolv.conf
Mar 31 15:58:54 freedom pia-resolv-fix[26698]: Check resolv status prompted by inotify
Mar 31 15:58:54 freedom pia-resolv-fix[26698]: All good, not a symlink and PIA is running
```

